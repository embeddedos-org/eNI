// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
#ifndef ENI_FW_FEEDBACK_H
#define ENI_FW_FEEDBACK_H
#include "eni_min/feedback.h"
typedef eni_min_feedback_t eni_fw_feedback_t;
#define eni_fw_feedback_init      eni_min_feedback_init
#define eni_fw_feedback_add_rule  eni_min_feedback_add_rule
#define eni_fw_feedback_evaluate  eni_min_feedback_evaluate
#define eni_fw_feedback_shutdown  eni_min_feedback_shutdown
#endif
