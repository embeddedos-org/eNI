// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eni/feedback.h"
#include <string.h>

eni_status_t eni_feedback_controller_init(eni_feedback_controller_t *ctrl,
                                          eni_stimulator_t *stim,
                                          float max_amp, uint32_t max_dur_ms) {
    if (!ctrl) return ENI_ERR_INVALID;
    memset(ctrl, 0, sizeof(*ctrl));
    ctrl->stimulator = stim;
    eni_stim_safety_init(&ctrl->safety, max_amp, max_dur_ms, 1000, 100);
    ctrl->enabled = true;
    return ENI_OK;
}

eni_status_t eni_feedback_controller_add_rule(eni_feedback_controller_t *ctrl,
                                              const char *intent, float min_conf,
                                              const eni_stim_params_t *response) {
    if (!ctrl || !intent || !response) return ENI_ERR_INVALID;
    if (ctrl->rule_count >= ENI_FEEDBACK_MAX_RULES) return ENI_ERR_OVERFLOW;

    eni_feedback_rule_t *rule = &ctrl->rules[ctrl->rule_count];
    strncpy(rule->trigger_intent, intent, 63);
    rule->min_confidence = min_conf;
    rule->response = *response;
    rule->active = true;
    ctrl->rule_count++;
    return ENI_OK;
}

eni_status_t eni_feedback_controller_evaluate(eni_feedback_controller_t *ctrl,
                                              const eni_event_t *intent_ev,
                                              eni_event_t *feedback_ev,
                                              uint64_t current_time_ms) {
    if (!ctrl || !intent_ev || !feedback_ev) return ENI_ERR_INVALID;
    if (!ctrl->enabled) return ENI_ERR_PERMISSION;
    if (intent_ev->type != ENI_EVENT_INTENT) return ENI_ERR_INVALID;

    const char *intent_name = intent_ev->payload.intent.name;
    float confidence = intent_ev->payload.intent.confidence;

    for (int i = 0; i < ctrl->rule_count; i++) {
        eni_feedback_rule_t *rule = &ctrl->rules[i];
        if (!rule->active) continue;
        if (strcmp(rule->trigger_intent, intent_name) != 0) continue;
        if (confidence < rule->min_confidence) continue;

        /* Safety check */
        eni_status_t st = eni_stim_safety_check(&ctrl->safety, &rule->response,
                                                 current_time_ms);
        if (st != ENI_OK) return st;

        /* Stimulate */
        if (ctrl->stimulator && ctrl->stimulator->ops && ctrl->stimulator->ops->stimulate) {
            st = ctrl->stimulator->ops->stimulate(ctrl->stimulator, &rule->response);
            if (st != ENI_OK) return st;
        }

        eni_stim_safety_record(&ctrl->safety, current_time_ms);

        /* Emit feedback event */
        memset(feedback_ev, 0, sizeof(*feedback_ev));
        feedback_ev->type = ENI_EVENT_FEEDBACK;
        feedback_ev->payload.feedback.type = rule->response.type;
        feedback_ev->payload.feedback.channel = rule->response.channel;
        feedback_ev->payload.feedback.amplitude = rule->response.amplitude;
        feedback_ev->payload.feedback.duration_ms = rule->response.duration_ms;
        feedback_ev->payload.feedback.frequency_hz = rule->response.frequency_hz;
        strncpy(feedback_ev->source, "feedback_ctrl", ENI_EVENT_SOURCE_MAX - 1);

        return ENI_OK;
    }
    return ENI_ERR_NOT_FOUND;
}

void eni_feedback_controller_shutdown(eni_feedback_controller_t *ctrl) {
    if (!ctrl) return;
    ctrl->enabled = false;
    if (ctrl->stimulator && ctrl->stimulator->ops && ctrl->stimulator->ops->shutdown)
        ctrl->stimulator->ops->shutdown(ctrl->stimulator);
}
