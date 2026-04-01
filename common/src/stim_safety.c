// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eni/stim_safety.h"
#include <string.h>

void eni_stim_safety_init(eni_stim_safety_t *safety, float max_amp,
                          uint32_t max_dur_ms, uint32_t min_interval_ms,
                          uint32_t max_daily) {
    if (!safety) return;
    memset(safety, 0, sizeof(*safety));
    safety->max_amplitude = max_amp;
    safety->max_duration_ms = max_dur_ms;
    safety->min_interval_ms = min_interval_ms;
    safety->max_daily_count = max_daily;

    /* Enforce absolute maximums */
    if (safety->max_amplitude > ENI_STIM_ABSOLUTE_MAX_AMPLITUDE)
        safety->max_amplitude = ENI_STIM_ABSOLUTE_MAX_AMPLITUDE;
    if (safety->max_duration_ms > ENI_STIM_ABSOLUTE_MAX_DURATION_MS)
        safety->max_duration_ms = ENI_STIM_ABSOLUTE_MAX_DURATION_MS;
}

eni_status_t eni_stim_safety_check(const eni_stim_safety_t *safety,
                                   const eni_stim_params_t *params,
                                   uint64_t current_time_ms) {
    if (!safety || !params) return ENI_ERR_INVALID;

    /* Absolute hard limits — never overridable */
    if (params->amplitude > ENI_STIM_ABSOLUTE_MAX_AMPLITUDE)
        return ENI_ERR_PERMISSION;
    if (params->duration_ms > ENI_STIM_ABSOLUTE_MAX_DURATION_MS)
        return ENI_ERR_PERMISSION;

    /* Configured limits */
    if (params->amplitude > safety->max_amplitude)
        return ENI_ERR_PERMISSION;
    if (params->duration_ms > safety->max_duration_ms)
        return ENI_ERR_PERMISSION;

    /* Rate limiting */
    if (safety->last_stim_time_ms > 0 && safety->min_interval_ms > 0) {
        uint64_t elapsed = current_time_ms - safety->last_stim_time_ms;
        if (elapsed < safety->min_interval_ms)
            return ENI_ERR_PERMISSION;
    }

    /* Daily count */
    if (safety->max_daily_count > 0 && safety->daily_count >= safety->max_daily_count)
        return ENI_ERR_PERMISSION;

    return ENI_OK;
}

void eni_stim_safety_record(eni_stim_safety_t *safety, uint64_t current_time_ms) {
    if (!safety) return;
    safety->last_stim_time_ms = current_time_ms;
    safety->daily_count++;
    safety->total_count++;
}

void eni_stim_safety_reset_daily(eni_stim_safety_t *safety) {
    if (safety) safety->daily_count = 0;
}
