// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
#include "eni_min/signal_processor.h"
#include <string.h>

eni_status_t eni_min_signal_processor_init(eni_min_signal_processor_t *sp,
                                           uint32_t epoch_size, uint32_t sample_rate,
                                           float artifact_threshold) {
    if (!sp) return ENI_ERR_INVALID;
    memset(sp, 0, sizeof(*sp));
    int esize = (int)epoch_size;
    if (esize == 0) esize = 256;
    if (esize > ENI_DSP_MAX_FFT_SIZE) esize = ENI_DSP_MAX_FFT_SIZE;
    eni_status_t st = eni_dsp_fft_init(&sp->fft_ctx, esize);
    if (st != ENI_OK) return st;
    eni_dsp_epoch_init(&sp->epoch, esize, (float)sample_rate);
    sp->artifact_threshold = (artifact_threshold > 0) ? artifact_threshold : 100.0f;
    return ENI_OK;
}

eni_status_t eni_min_signal_processor_process(eni_min_signal_processor_t *sp,
                                              const eni_event_t *raw_ev,
                                              eni_event_t *features_ev) {
    if (!sp || !raw_ev || !features_ev) return ENI_ERR_INVALID;
    if (raw_ev->type != ENI_EVENT_RAW) return ENI_ERR_INVALID;

    /* Push first byte of raw data as a sample (simplified) */
    float sample = 0.0f;
    if (raw_ev->payload.raw.len >= sizeof(float))
        memcpy(&sample, raw_ev->payload.raw.data, sizeof(float));
    eni_dsp_epoch_push(&sp->epoch, sample);

    if (!eni_dsp_epoch_ready(&sp->epoch)) return ENI_ERR_TIMEOUT;

    /* Extract features */
    eni_status_t st = eni_dsp_extract_features(&sp->fft_ctx, sp->epoch.samples,
                                                sp->epoch.count, sp->epoch.sample_rate,
                                                &sp->last_features);
    if (st != ENI_OK) return st;
    sp->has_features = true;

    /* Artifact check */
    eni_dsp_artifact_t art = eni_dsp_artifact_detect(sp->epoch.samples, sp->epoch.count,
                                                      sp->artifact_threshold);
    eni_dsp_epoch_reset(&sp->epoch);

    if (art.severity > 0.5f) return ENI_ERR_TIMEOUT; /* Drop artifact-contaminated epoch */

    /* Emit features event */
    memset(features_ev, 0, sizeof(*features_ev));
    features_ev->type = ENI_EVENT_FEATURES;
    features_ev->timestamp = raw_ev->timestamp;
    for (int i = 0; i < ENI_DSP_NUM_BANDS && i < ENI_EVENT_FEATURES_MAX; i++) {
        static const char *band_names[] = {"delta","theta","alpha","beta","gamma"};
        strncpy(features_ev->payload.features.features[i].name, band_names[i], 31);
        features_ev->payload.features.features[i].value = sp->last_features.band_power[i];
    }
    features_ev->payload.features.count = ENI_DSP_NUM_BANDS;
    return ENI_OK;
}
