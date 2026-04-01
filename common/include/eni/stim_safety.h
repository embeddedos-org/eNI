// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef ENI_STIM_SAFETY_H
#define ENI_STIM_SAFETY_H

#include "eni/types.h"
#include "eni/stimulator.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ENI_STIM_ABSOLUTE_MAX_AMPLITUDE    2.0f     /* mA */
#define ENI_STIM_ABSOLUTE_MAX_DURATION_MS  1800000  /* 30 min */

typedef struct {
    float    max_amplitude;
    uint32_t max_duration_ms;
    uint32_t min_interval_ms;
    uint32_t max_daily_count;
    uint64_t last_stim_time_ms;
    uint32_t daily_count;
    uint32_t total_count;
} eni_stim_safety_t;

void         eni_stim_safety_init(eni_stim_safety_t *safety, float max_amp,
                                  uint32_t max_dur_ms, uint32_t min_interval_ms,
                                  uint32_t max_daily);
eni_status_t eni_stim_safety_check(const eni_stim_safety_t *safety,
                                   const eni_stim_params_t *params,
                                   uint64_t current_time_ms);
void         eni_stim_safety_record(eni_stim_safety_t *safety, uint64_t current_time_ms);
void         eni_stim_safety_reset_daily(eni_stim_safety_t *safety);

#ifdef __cplusplus
}
#endif

#endif /* ENI_STIM_SAFETY_H */
