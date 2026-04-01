// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
#ifndef ENI_MIN_SIGNAL_PROCESSOR_H
#define ENI_MIN_SIGNAL_PROCESSOR_H

#include "eni/dsp.h"
#include "eni/event.h"

typedef struct {
    eni_dsp_fft_ctx_t  fft_ctx;
    eni_dsp_epoch_t    epoch;
    eni_dsp_features_t last_features;
    bool               has_features;
    float              artifact_threshold;
} eni_min_signal_processor_t;

eni_status_t eni_min_signal_processor_init(eni_min_signal_processor_t *sp,
                                           uint32_t epoch_size, uint32_t sample_rate,
                                           float artifact_threshold);
eni_status_t eni_min_signal_processor_process(eni_min_signal_processor_t *sp,
                                              const eni_event_t *raw_ev,
                                              eni_event_t *features_ev);

#endif
