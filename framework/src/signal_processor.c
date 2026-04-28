// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

#include "eni_fw/signal_processor.h"
#include "eni/log.h"
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void compute_filter_coeffs(eni_fw_filter_t *f, uint32_t sample_rate)
{
    float fs = (float)sample_rate;
    memset(f->coeffs_b, 0, sizeof(f->coeffs_b));
    memset(f->coeffs_a, 0, sizeof(f->coeffs_a));

    switch (f->cfg.type) {
    case ENI_FW_FILTER_LOWPASS: {
        float rc = 1.0f / (2.0f * (float)M_PI * f->cfg.high_hz);
        float dt = 1.0f / fs;
        float alpha = dt / (rc + dt);
        f->coeffs_b[0] = alpha;
        f->coeffs_a[0] = 1.0f;
        f->coeffs_a[1] = -(1.0f - alpha);
        f->n_coeffs = 2;
        break;
    }
    case ENI_FW_FILTER_HIGHPASS: {
        float rc = 1.0f / (2.0f * (float)M_PI * f->cfg.low_hz);
        float dt = 1.0f / fs;
        float alpha = rc / (rc + dt);
        f->coeffs_b[0] = alpha;
        f->coeffs_b[1] = -alpha;
        f->coeffs_a[0] = 1.0f;
        f->coeffs_a[1] = -(1.0f - 1.0f / (rc / dt + 1.0f));
        f->n_coeffs = 2;
        break;
    }
    case ENI_FW_FILTER_NOTCH: {
        float w0 = 2.0f * (float)M_PI * f->cfg.low_hz / fs;
        float bw = 2.0f * (float)M_PI * 2.0f / fs; /* 2 Hz bandwidth */
        float alpha = sinf(w0) * sinhf(logf(2.0f) / 2.0f * bw * w0 / sinf(w0));
        float cos_w0 = cosf(w0);
        float a0 = 1.0f + alpha;
        f->coeffs_b[0] = 1.0f / a0;
        f->coeffs_b[1] = -2.0f * cos_w0 / a0;
        f->coeffs_b[2] = 1.0f / a0;
        f->coeffs_a[0] = 1.0f;
        f->coeffs_a[1] = -2.0f * cos_w0 / a0;
        f->coeffs_a[2] = (1.0f - alpha) / a0;
        f->n_coeffs = 3;
        break;
    }
    case ENI_FW_FILTER_BANDPASS: {
        float w0 = 2.0f * (float)M_PI * (f->cfg.low_hz + f->cfg.high_hz) / 2.0f / fs;
        float bw = 2.0f * (float)M_PI * (f->cfg.high_hz - f->cfg.low_hz) / fs;
        float alpha = sinf(w0) * sinhf(logf(2.0f) / 2.0f * bw * w0 / sinf(w0));
        float a0 = 1.0f + alpha;
        f->coeffs_b[0] = alpha / a0;
        f->coeffs_b[1] = 0.0f;
        f->coeffs_b[2] = -alpha / a0;
        f->coeffs_a[0] = 1.0f;
        f->coeffs_a[1] = -2.0f * cosf(w0) / a0;
        f->coeffs_a[2] = (1.0f - alpha) / a0;
        f->n_coeffs = 3;
        break;
    }
    default:
        f->coeffs_b[0] = 1.0f;
        f->coeffs_a[0] = 1.0f;
        f->n_coeffs = 1;
        break;
    }
}

static float apply_filter(eni_fw_filter_t *f, float sample, int channel)
{
    if (f->n_coeffs <= 1) return sample;

    float *state = f->state[channel];
    float output = f->coeffs_b[0] * sample + state[0];
    for (int i = 1; i < f->n_coeffs; i++) {
        state[i - 1] = f->coeffs_b[i] * sample - f->coeffs_a[i] * output;
        if (i < f->n_coeffs - 1)
            state[i - 1] += state[i];
    }
    return output;
}

eni_status_t eni_fw_signal_processor_init(eni_fw_signal_processor_t *sp,
                                          int num_channels,
                                          uint32_t epoch_size,
                                          uint32_t sample_rate,
                                          float artifact_threshold)
{
    if (!sp || num_channels <= 0 || num_channels > ENI_FW_SP_MAX_CHANNELS)
        return ENI_ERR_INVALID;
    if (epoch_size == 0 || epoch_size > ENI_DSP_MAX_FFT_SIZE)
        return ENI_ERR_INVALID;

    memset(sp, 0, sizeof(*sp));
    sp->num_channels       = num_channels;
    sp->sample_rate        = sample_rate;
    sp->epoch_size         = epoch_size;
    sp->artifact_threshold = artifact_threshold;

    eni_status_t rc = eni_dsp_fft_init(&sp->fft_ctx, (int)epoch_size);
    if (rc != ENI_OK) return rc;

    for (int ch = 0; ch < num_channels; ch++) {
        eni_dsp_epoch_init(&sp->epochs[ch], (int)epoch_size, (float)sample_rate);
    }

    rc = eni_mutex_init(&sp->lock);
    if (rc != ENI_OK) return rc;

    sp->initialized = true;
    ENI_LOG_INFO("fw.sp", "initialized: %d channels, epoch=%u, rate=%u Hz",
                 num_channels, epoch_size, sample_rate);
    return ENI_OK;
}

eni_status_t eni_fw_signal_processor_add_filter(eni_fw_signal_processor_t *sp,
                                                 const eni_fw_filter_config_t *cfg)
{
    if (!sp || !sp->initialized || !cfg) return ENI_ERR_INVALID;

    eni_mutex_lock(&sp->lock);

    if (sp->filter_count >= ENI_FW_SP_MAX_FILTERS) {
        eni_mutex_unlock(&sp->lock);
        return ENI_ERR_OVERFLOW;
    }

    eni_fw_filter_t *f = &sp->filters[sp->filter_count];
    memset(f, 0, sizeof(*f));
    f->cfg = *cfg;
    compute_filter_coeffs(f, sp->sample_rate);
    sp->filter_count++;

    eni_mutex_unlock(&sp->lock);

    ENI_LOG_INFO("fw.sp", "added filter type=%d low=%.1f high=%.1f",
                 cfg->type, cfg->low_hz, cfg->high_hz);
    return ENI_OK;
}

eni_status_t eni_fw_signal_processor_process(eni_fw_signal_processor_t *sp,
                                              const float *samples,
                                              int num_samples,
                                              int channel,
                                              eni_dsp_features_t *features_out)
{
    if (!sp || !sp->initialized || !samples || !features_out)
        return ENI_ERR_INVALID;
    if (channel < 0 || channel >= sp->num_channels)
        return ENI_ERR_INVALID;

    eni_mutex_lock(&sp->lock);

    for (int i = 0; i < num_samples; i++) {
        float s = samples[i];

        /* Apply filter chain */
        for (int f = 0; f < sp->filter_count; f++) {
            s = apply_filter(&sp->filters[f], s, channel);
        }

        eni_dsp_epoch_push(&sp->epochs[channel], s);
    }

    eni_status_t rc = ENI_ERR_TIMEOUT; /* Not ready yet */

    if (eni_dsp_epoch_ready(&sp->epochs[channel])) {
        /* Artifact detection */
        eni_dsp_artifact_t art = eni_dsp_artifact_detect(
            sp->epochs[channel].samples,
            sp->epochs[channel].count,
            sp->artifact_threshold);

        if (art.severity > sp->artifact_threshold) {
            sp->artifacts_rejected++;
            eni_dsp_epoch_reset(&sp->epochs[channel]);
            eni_mutex_unlock(&sp->lock);
            return ENI_ERR_RUNTIME; /* Artifact rejected */
        }

        rc = eni_dsp_extract_features(
            &sp->fft_ctx,
            sp->epochs[channel].samples,
            sp->epochs[channel].count,
            (float)sp->sample_rate,
            features_out);

        if (rc == ENI_OK) {
            sp->channel_features[channel] = *features_out;
            sp->frames_processed++;
        }

        eni_dsp_epoch_reset(&sp->epochs[channel]);
    }

    eni_mutex_unlock(&sp->lock);
    return rc;
}

eni_status_t eni_fw_signal_processor_process_multi(eni_fw_signal_processor_t *sp,
                                                    const float *interleaved_samples,
                                                    int num_frames,
                                                    eni_dsp_features_t *features_out)
{
    if (!sp || !sp->initialized || !interleaved_samples || !features_out)
        return ENI_ERR_INVALID;

    eni_status_t last_rc = ENI_ERR_TIMEOUT;

    for (int frame = 0; frame < num_frames; frame++) {
        for (int ch = 0; ch < sp->num_channels; ch++) {
            float sample = interleaved_samples[frame * sp->num_channels + ch];
            eni_status_t rc = eni_fw_signal_processor_process(
                sp, &sample, 1, ch, &features_out[ch]);
            if (rc == ENI_OK) last_rc = ENI_OK;
        }
    }

    return last_rc;
}

void eni_fw_signal_processor_reset(eni_fw_signal_processor_t *sp)
{
    if (!sp || !sp->initialized) return;
    eni_mutex_lock(&sp->lock);
    for (int ch = 0; ch < sp->num_channels; ch++) {
        eni_dsp_epoch_reset(&sp->epochs[ch]);
    }
    for (int f = 0; f < sp->filter_count; f++) {
        memset(sp->filters[f].state, 0, sizeof(sp->filters[f].state));
    }
    eni_mutex_unlock(&sp->lock);
}

void eni_fw_signal_processor_shutdown(eni_fw_signal_processor_t *sp)
{
    if (!sp || !sp->initialized) return;

    ENI_LOG_INFO("fw.sp", "shutdown: processed=%llu artifacts_rejected=%llu",
                 (unsigned long long)sp->frames_processed,
                 (unsigned long long)sp->artifacts_rejected);

    eni_mutex_destroy(&sp->lock);
    sp->initialized = false;
}
