// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
#ifndef ENI_MIN_FEEDBACK_H
#define ENI_MIN_FEEDBACK_H

#include "eni/feedback.h"

typedef struct {
    eni_feedback_controller_t ctrl;
    eni_stimulator_t          stimulator;
    bool                      initialized;
} eni_min_feedback_t;

eni_status_t eni_min_feedback_init(eni_min_feedback_t *fb,
                                   const eni_stimulator_ops_t *stim_ops,
                                   float max_amp, uint32_t max_dur_ms);
eni_status_t eni_min_feedback_add_rule(eni_min_feedback_t *fb, const char *intent,
                                       float min_conf, const eni_stim_params_t *response);
eni_status_t eni_min_feedback_evaluate(eni_min_feedback_t *fb,
                                       const eni_event_t *intent_ev,
                                       eni_event_t *feedback_ev,
                                       uint64_t current_time_ms);
void         eni_min_feedback_shutdown(eni_min_feedback_t *fb);

#endif
