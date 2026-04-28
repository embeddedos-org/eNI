// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
#ifndef ENI_FW_FEEDBACK_H
#define ENI_FW_FEEDBACK_H

#include "eni/feedback.h"
#include "eni_platform/platform.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ENI_FW_FEEDBACK_MAX_RULES    16
#define ENI_FW_FEEDBACK_MAX_OUTPUTS  4

typedef enum {
    ENI_FW_ADAPT_NONE,
    ENI_FW_ADAPT_CONFIDENCE,
    ENI_FW_ADAPT_PERFORMANCE,
    ENI_FW_ADAPT_FATIGUE,
} eni_fw_adapt_mode_t;

typedef struct {
    char                trigger_intent[64];
    float               min_confidence;
    eni_stim_params_t   response;
    eni_fw_adapt_mode_t adapt_mode;
    float               adapt_rate;
    float               current_intensity;  /* 0.0 to 1.0 */
    bool                active;
    uint64_t            trigger_count;
} eni_fw_feedback_rule_t;

typedef struct {
    eni_stimulator_t      *outputs[ENI_FW_FEEDBACK_MAX_OUTPUTS];
    int                    output_count;
    eni_stim_safety_t      safety;

    eni_fw_feedback_rule_t rules[ENI_FW_FEEDBACK_MAX_RULES];
    int                    rule_count;

    /* Global intensity scaling */
    float                  global_intensity;
    bool                   enabled;

    /* Thread safety */
    eni_mutex_t            lock;
    bool                   initialized;

    /* Stats */
    uint64_t               evaluations;
    uint64_t               stimulations_delivered;
    uint64_t               stimulations_blocked;
} eni_fw_feedback_t;

eni_status_t eni_fw_feedback_init(eni_fw_feedback_t *fb,
                                   const eni_stimulator_ops_t *stim_ops,
                                   float max_amp, uint32_t max_dur_ms);

eni_status_t eni_fw_feedback_add_output(eni_fw_feedback_t *fb,
                                         eni_stimulator_t *stim);

eni_status_t eni_fw_feedback_add_rule(eni_fw_feedback_t *fb,
                                       const char *intent, float min_conf,
                                       const eni_stim_params_t *response,
                                       eni_fw_adapt_mode_t adapt_mode,
                                       float adapt_rate);

eni_status_t eni_fw_feedback_evaluate(eni_fw_feedback_t *fb,
                                       const eni_event_t *intent_ev,
                                       eni_event_t *feedback_ev,
                                       uint64_t current_time_ms);

eni_status_t eni_fw_feedback_remove_rule(eni_fw_feedback_t *fb, int index);
void         eni_fw_feedback_set_intensity(eni_fw_feedback_t *fb, float intensity);
void         eni_fw_feedback_enable(eni_fw_feedback_t *fb, bool enabled);
void         eni_fw_feedback_shutdown(eni_fw_feedback_t *fb);

#ifdef __cplusplus
}
#endif

#endif /* ENI_FW_FEEDBACK_H */
