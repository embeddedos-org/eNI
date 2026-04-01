// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
#include "eni_min/feedback.h"
#include <string.h>

eni_status_t eni_min_feedback_init(eni_min_feedback_t *fb,
                                   const eni_stimulator_ops_t *stim_ops,
                                   float max_amp, uint32_t max_dur_ms) {
    if (!fb || !stim_ops) return ENI_ERR_INVALID;
    memset(fb, 0, sizeof(*fb));
    fb->stimulator.ops = stim_ops;
    strncpy(fb->stimulator.name, stim_ops->name ? stim_ops->name : "unknown", 63);
    if (stim_ops->init) {
        eni_status_t st = stim_ops->init(&fb->stimulator, NULL);
        if (st != ENI_OK) return st;
    }
    eni_status_t st = eni_feedback_controller_init(&fb->ctrl, &fb->stimulator,
                                                    max_amp, max_dur_ms);
    if (st != ENI_OK) return st;
    fb->initialized = true;
    return ENI_OK;
}

eni_status_t eni_min_feedback_add_rule(eni_min_feedback_t *fb, const char *intent,
                                       float min_conf, const eni_stim_params_t *response) {
    if (!fb || !fb->initialized) return ENI_ERR_INVALID;
    return eni_feedback_controller_add_rule(&fb->ctrl, intent, min_conf, response);
}

eni_status_t eni_min_feedback_evaluate(eni_min_feedback_t *fb,
                                       const eni_event_t *intent_ev,
                                       eni_event_t *feedback_ev,
                                       uint64_t current_time_ms) {
    if (!fb || !fb->initialized) return ENI_ERR_INVALID;
    return eni_feedback_controller_evaluate(&fb->ctrl, intent_ev, feedback_ev, current_time_ms);
}

void eni_min_feedback_shutdown(eni_min_feedback_t *fb) {
    if (!fb || !fb->initialized) return;
    eni_feedback_controller_shutdown(&fb->ctrl);
    fb->initialized = false;
}
