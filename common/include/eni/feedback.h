// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef ENI_FEEDBACK_H
#define ENI_FEEDBACK_H

#include "eni/types.h"
#include "eni/event.h"
#include "eni/stimulator.h"
#include "eni/stim_safety.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ENI_FEEDBACK_MAX_RULES 8

typedef struct {
    char             trigger_intent[64];
    float            min_confidence;
    eni_stim_params_t response;
    bool             active;
} eni_feedback_rule_t;

typedef struct {
    eni_stimulator_t    *stimulator;
    eni_stim_safety_t    safety;
    eni_feedback_rule_t  rules[ENI_FEEDBACK_MAX_RULES];
    int                  rule_count;
    bool                 enabled;
} eni_feedback_controller_t;

eni_status_t eni_feedback_controller_init(eni_feedback_controller_t *ctrl,
                                          eni_stimulator_t *stim,
                                          float max_amp, uint32_t max_dur_ms);
eni_status_t eni_feedback_controller_add_rule(eni_feedback_controller_t *ctrl,
                                              const char *intent, float min_conf,
                                              const eni_stim_params_t *response);
eni_status_t eni_feedback_controller_evaluate(eni_feedback_controller_t *ctrl,
                                              const eni_event_t *intent_ev,
                                              eni_event_t *feedback_ev,
                                              uint64_t current_time_ms);
void         eni_feedback_controller_shutdown(eni_feedback_controller_t *ctrl);

#ifdef __cplusplus
}
#endif

#endif /* ENI_FEEDBACK_H */
