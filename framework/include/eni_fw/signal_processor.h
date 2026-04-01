// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
#ifndef ENI_FW_SIGNAL_PROCESSOR_H
#define ENI_FW_SIGNAL_PROCESSOR_H
#include "eni_min/signal_processor.h"
/* Framework DSP — reuses eni_min_signal_processor_t */
typedef eni_min_signal_processor_t eni_fw_signal_processor_t;
#define eni_fw_signal_processor_init   eni_min_signal_processor_init
#define eni_fw_signal_processor_process eni_min_signal_processor_process
#endif
