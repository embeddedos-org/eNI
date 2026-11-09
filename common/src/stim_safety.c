// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eni/stim_safety.h"
#include <math.h>
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

    /* A NaN ceiling would make every `amplitude > max` comparison in the
     * check false, so the configured limit would stop limiting anything
     * below the absolute ceiling. Fail closed: a ceiling that is not a
     * number allows no stimulation above 0 mA. */
    if (isnan(safety->max_amplitude))
        safety->max_amplitude = 0.0f;

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

    /* F3-1: refuse non-finite and negative amplitudes explicitly,
     * before any comparison runs. Every amplitude comparison below
     * is of the form `amplitude > max`: a NaN makes all of them
     * false and a negative amplitude is below every cap, so both
     * would otherwise pass this gate and return ENI_OK — a
     * fail-open edge on the one surface whose failure is physical
     * (audit phase 3, docs/security-audit-stimulation-output.md). */
    if (!isfinite(params->amplitude) || params->amplitude < 0.0f)
        return ENI_ERR_PERMISSION;

    /* Absolute hard limits — never overridable */
    if (params->amplitude > ENI_STIM_ABSOLUTE_MAX_AMPLITUDE)
        return ENI_ERR_PERMISSION;
    if (params->duration_ms > ENI_STIM_ABSOLUTE_MAX_DURATION_MS)
        return ENI_ERR_PERMISSION;

    /* Configured limits. Written as !(a <= max) so that a NaN max, set on
     * the public struct after init, refuses instead of allowing. */
    if (!(params->amplitude <= safety->max_amplitude))
        return ENI_ERR_PERMISSION;
    if (params->duration_ms > safety->max_duration_ms)
        return ENI_ERR_PERMISSION;

    /* Rate limiting. F3-2: fail closed on a backwards clock. The
     * unsigned subtraction wraps to a huge elapsed value when
     * current_time_ms < last_stim_time_ms, which would pass the
     * interval check immediately — so a non-monotonic reading is
     * refused outright instead of being trusted. */
    if (safety->total_count > 0 && safety->min_interval_ms > 0) {
        if (current_time_ms < safety->last_stim_time_ms)
            return ENI_ERR_PERMISSION;
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
