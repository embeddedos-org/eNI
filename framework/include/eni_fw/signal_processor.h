// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
#ifndef ENI_FW_SIGNAL_PROCESSOR_H
#define ENI_FW_SIGNAL_PROCESSOR_H

#include "eni/dsp.h"
#include "eni/event.h"
#include "eni_platform/platform.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ENI_FW_SP_MAX_CHANNELS  256
#define ENI_FW_SP_MAX_FILTERS   16

typedef enum {
    ENI_FW_FILTER_BANDPASS,
    ENI_FW_FILTER_NOTCH,
    ENI_FW_FILTER_LOWPASS,
    ENI_FW_FILTER_HIGHPASS,
    ENI_FW_FILTER_CUSTOM,
} eni_fw_filter_type_t;

typedef struct {
    eni_fw_filter_type_t type;
    float                low_hz;
    float                high_hz;
    int                  order;
} eni_fw_filter_config_t;

typedef struct {
    eni_fw_filter_config_t cfg;
    float                  coeffs_b[8];
    float                  coeffs_a[8];
    int                    n_coeffs;
    float                  state[ENI_FW_SP_MAX_CHANNELS][8];
} eni_fw_filter_t;

typedef struct {
    /* Per-channel processing state */
    eni_dsp_fft_ctx_t   fft_ctx;
    eni_dsp_epoch_t     epochs[ENI_FW_SP_MAX_CHANNELS];
    eni_dsp_features_t  channel_features[ENI_FW_SP_MAX_CHANNELS];
    int                 num_channels;
    uint32_t            sample_rate;
    uint32_t            epoch_size;
    float               artifact_threshold;

    /* Filter chain */
    eni_fw_filter_t     filters[ENI_FW_SP_MAX_FILTERS];
    int                 filter_count;

    /* Thread safety */
    eni_mutex_t         lock;
    bool                initialized;

    /* Stats */
    uint64_t            frames_processed;
    uint64_t            artifacts_rejected;
} eni_fw_signal_processor_t;

eni_status_t eni_fw_signal_processor_init(eni_fw_signal_processor_t *sp,
                                          int num_channels,
                                          uint32_t epoch_size,
                                          uint32_t sample_rate,
                                          float artifact_threshold);

eni_status_t eni_fw_signal_processor_add_filter(eni_fw_signal_processor_t *sp,
                                                 const eni_fw_filter_config_t *cfg);

eni_status_t eni_fw_signal_processor_process(eni_fw_signal_processor_t *sp,
                                              const float *samples,
                                              int num_samples,
                                              int channel,
                                              eni_dsp_features_t *features_out);

eni_status_t eni_fw_signal_processor_process_multi(eni_fw_signal_processor_t *sp,
                                                    const float *interleaved_samples,
                                                    int num_frames,
                                                    eni_dsp_features_t *features_out);

void eni_fw_signal_processor_reset(eni_fw_signal_processor_t *sp);
void eni_fw_signal_processor_shutdown(eni_fw_signal_processor_t *sp);

#ifdef __cplusplus
}
#endif

#endif /* ENI_FW_SIGNAL_PROCESSOR_H */
