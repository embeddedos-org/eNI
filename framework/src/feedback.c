// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

#include "eni_fw/feedback.h"
#include "eni/log.h"
#include <string.h>

eni_status_t eni_fw_feedback_init(eni_fw_feedback_t *fb,
                                   const eni_stimulator_ops_t *stim_ops,
                                   float max_amp, uint32_t max_dur_ms)
{
    if (!fb) return ENI_ERR_INVALID;

    memset(fb, 0, sizeof(*fb));

    eni_stim_safety_init(&fb->safety, max_amp, max_dur_ms, 100, 1000);
    fb->global_intensity = 1.0f;
    fb->enabled = true;

    eni_status_t rc = eni_mutex_init(&fb->lock);
    if (rc != ENI_OK) return rc;

    /* If stim_ops provided, we create a default output later when needed */
    (void)stim_ops;

    fb->initialized = true;
    ENI_LOG_INFO("fw.fb", "initialized: max_amp=%.2f max_dur=%u ms", max_amp, max_dur_ms);
    return ENI_OK;
}

eni_status_t eni_fw_feedback_add_output(eni_fw_feedback_t *fb, eni_stimulator_t *stim)
{
    if (!fb || !fb->initialized || !stim) return ENI_ERR_INVALID;

    eni_mutex_lock(&fb->lock);
    if (fb->output_count >= ENI_FW_FEEDBACK_MAX_OUTPUTS) {
        eni_mutex_unlock(&fb->lock);
        return ENI_ERR_OVERFLOW;
    }
    fb->outputs[fb->output_count++] = stim;
    eni_mutex_unlock(&fb->lock);
    return ENI_OK;
}

eni_status_t eni_fw_feedback_add_rule(eni_fw_feedback_t *fb,
                                       const char *intent, float min_conf,
                                       const eni_stim_params_t *response,
                                       eni_fw_adapt_mode_t adapt_mode,
                                       float adapt_rate)
{
    if (!fb || !fb->initialized || !intent || !response) return ENI_ERR_INVALID;

    eni_mutex_lock(&fb->lock);

    if (fb->rule_count >= ENI_FW_FEEDBACK_MAX_RULES) {
        eni_mutex_unlock(&fb->lock);
        return ENI_ERR_OVERFLOW;
    }

    eni_fw_feedback_rule_t *r = &fb->rules[fb->rule_count];
    memset(r, 0, sizeof(*r));
    strncpy(r->trigger_intent, intent, sizeof(r->trigger_intent) - 1);
    r->min_confidence    = min_conf;
    r->response          = *response;
    r->adapt_mode        = adapt_mode;
    r->adapt_rate        = (adapt_rate > 0.0f) ? adapt_rate : 0.01f;
    r->current_intensity = 1.0f;
    r->active            = true;
    fb->rule_count++;

    eni_mutex_unlock(&fb->lock);

    ENI_LOG_INFO("fw.fb", "added rule: intent='%s' min_conf=%.2f adapt=%d",
                 intent, min_conf, adapt_mode);
    return ENI_OK;
}

static float adapt_intensity(eni_fw_feedback_rule_t *rule, float confidence)
{
    float intensity = rule->current_intensity;

    switch (rule->adapt_mode) {
    case ENI_FW_ADAPT_CONFIDENCE:
        /* Scale intensity proportionally to confidence */
        intensity = confidence;
        break;
    case ENI_FW_ADAPT_PERFORMANCE:
        /* Gradually increase if confidence is high, decrease if low */
        if (confidence > 0.8f)
            intensity += rule->adapt_rate;
        else if (confidence < 0.5f)
            intensity -= rule->adapt_rate;
        break;
    case ENI_FW_ADAPT_FATIGUE:
        /* Gradually decrease over repeated stimulations */
        intensity -= rule->adapt_rate * 0.1f;
        break;
    case ENI_FW_ADAPT_NONE:
    default:
        break;
    }

    /* Clamp to valid range */
    if (intensity < 0.1f) intensity = 0.1f;
    if (intensity > 1.0f) intensity = 1.0f;
    rule->current_intensity = intensity;
    return intensity;
}

eni_status_t eni_fw_feedback_evaluate(eni_fw_feedback_t *fb,
                                       const eni_event_t *intent_ev,
                                       eni_event_t *feedback_ev,
                                       uint64_t current_time_ms)
{
    if (!fb || !fb->initialized || !intent_ev || !feedback_ev)
        return ENI_ERR_INVALID;

    if (!fb->enabled) return ENI_ERR_PERMISSION;

    eni_mutex_lock(&fb->lock);
    fb->evaluations++;

    if (intent_ev->type != ENI_EVENT_INTENT) {
        eni_mutex_unlock(&fb->lock);
        return ENI_ERR_INVALID;
    }

    const char *intent_name = intent_ev->payload.intent.name;
    float confidence = intent_ev->payload.intent.confidence;

    eni_fw_feedback_rule_t *matched = NULL;
    for (int i = 0; i < fb->rule_count; i++) {
        eni_fw_feedback_rule_t *r = &fb->rules[i];
        if (!r->active) continue;
        if (strcmp(r->trigger_intent, intent_name) != 0) continue;
        if (confidence < r->min_confidence) continue;
        matched = r;
        break;
    }

    if (!matched) {
        eni_mutex_unlock(&fb->lock);
        return ENI_ERR_NOT_FOUND;
    }

    /* Adapt intensity */
    float intensity = adapt_intensity(matched, confidence) * fb->global_intensity;

    /* Build stimulation params with adapted intensity */
    eni_stim_params_t adapted_params = matched->response;
    adapted_params.amplitude *= intensity;

    /* Safety check */
    eni_status_t rc = eni_stim_safety_check(&fb->safety, &adapted_params, current_time_ms);
    if (rc != ENI_OK) {
        fb->stimulations_blocked++;
        eni_mutex_unlock(&fb->lock);
        return rc;
    }

    /* Deliver to all outputs */
    bool any_delivered = false;
    for (int o = 0; o < fb->output_count; o++) {
        eni_stimulator_t *stim = fb->outputs[o];
        if (stim && stim->ops && stim->ops->stimulate) {
            if (stim->ops->stimulate(stim, &adapted_params) == ENI_OK)
                any_delivered = true;
        }
    }

    eni_stim_safety_record(&fb->safety, current_time_ms);
    matched->trigger_count++;
    fb->stimulations_delivered++;

    /* Build feedback event */
    eni_event_init(feedback_ev, ENI_EVENT_FEEDBACK, "fw.feedback");
    feedback_ev->payload.feedback.type         = adapted_params.type;
    feedback_ev->payload.feedback.channel      = adapted_params.channel;
    feedback_ev->payload.feedback.amplitude    = adapted_params.amplitude;
    feedback_ev->payload.feedback.duration_ms  = adapted_params.duration_ms;
    feedback_ev->payload.feedback.frequency_hz = adapted_params.frequency_hz;

    eni_mutex_unlock(&fb->lock);
    return ENI_OK;
}

eni_status_t eni_fw_feedback_remove_rule(eni_fw_feedback_t *fb, int index)
{
    if (!fb || !fb->initialized) return ENI_ERR_INVALID;

    eni_mutex_lock(&fb->lock);
    if (index < 0 || index >= fb->rule_count) {
        eni_mutex_unlock(&fb->lock);
        return ENI_ERR_INVALID;
    }

    /* Shift remaining rules */
    for (int i = index; i < fb->rule_count - 1; i++) {
        fb->rules[i] = fb->rules[i + 1];
    }
    fb->rule_count--;
    eni_mutex_unlock(&fb->lock);
    return ENI_OK;
}

void eni_fw_feedback_set_intensity(eni_fw_feedback_t *fb, float intensity)
{
    if (!fb || !fb->initialized) return;
    if (intensity < 0.0f) intensity = 0.0f;
    if (intensity > 1.0f) intensity = 1.0f;
    eni_mutex_lock(&fb->lock);
    fb->global_intensity = intensity;
    eni_mutex_unlock(&fb->lock);
}

void eni_fw_feedback_enable(eni_fw_feedback_t *fb, bool enabled)
{
    if (!fb || !fb->initialized) return;
    eni_mutex_lock(&fb->lock);
    fb->enabled = enabled;
    eni_mutex_unlock(&fb->lock);
}

void eni_fw_feedback_shutdown(eni_fw_feedback_t *fb)
{
    if (!fb || !fb->initialized) return;

    ENI_LOG_INFO("fw.fb", "shutdown: evaluations=%llu delivered=%llu blocked=%llu",
                 (unsigned long long)fb->evaluations,
                 (unsigned long long)fb->stimulations_delivered,
                 (unsigned long long)fb->stimulations_blocked);

    eni_mutex_destroy(&fb->lock);
    fb->initialized = false;
}
