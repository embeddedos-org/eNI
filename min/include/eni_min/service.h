// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef ENI_MIN_SERVICE_H
#define ENI_MIN_SERVICE_H

#include "eni/common.h"
#include "eni_min/input.h"
#include "eni_min/normalizer.h"
#include "eni_min/filter.h"
#include "eni_min/mapper.h"
#include "eni_min/tool_bridge.h"
#ifdef ENI_HAS_DSP
#include "eni_min/signal_processor.h"
#endif
#ifdef ENI_HAS_DECODER
#include "eni_min/decoder.h"
#endif
#ifdef ENI_HAS_STIMULATOR
#include "eni_min/feedback.h"
#endif

typedef enum {
    ENI_MIN_STATE_INIT,
    ENI_MIN_STATE_RUNNING,
    ENI_MIN_STATE_STOPPED,
    ENI_MIN_STATE_ERROR,
} eni_min_state_t;

typedef struct {
    eni_config_t           config;
    eni_min_input_t        input;
    eni_min_normalizer_t   normalizer;
    eni_min_filter_t       filter;
    eni_min_mapper_t       mapper;
    eni_policy_engine_t    policy;
    eni_min_tool_bridge_t  tool_bridge;
    eni_min_state_t        state;
    uint64_t               events_processed;
    uint64_t               events_filtered;
    uint64_t               events_executed;
    uint64_t               events_decoded;
    uint64_t               events_stimulated;
    void                  *eipc_bridge;
#ifdef ENI_HAS_DSP
    eni_min_signal_processor_t signal_processor;
#endif
#ifdef ENI_HAS_DECODER
    eni_min_decoder_t      decoder;
#endif
#ifdef ENI_HAS_STIMULATOR
    eni_min_feedback_t     feedback;
#endif
    uint64_t               events_stimulated;
    void                  *eipc_bridge; /* eni_eipc_bridge_t* when ENI_EIPC_ENABLED */
#ifdef ENI_HAS_DSP
    eni_min_signal_processor_t signal_processor;
#endif
#ifdef ENI_HAS_DECODER
    eni_min_decoder_t      decoder;
#endif
#ifdef ENI_HAS_STIMULATOR
    eni_min_feedback_t     feedback;
#endif
} eni_min_service_t;

eni_status_t eni_min_service_init(eni_min_service_t *svc, const eni_config_t *cfg,
                                   const eni_provider_ops_t *provider_ops);
eni_status_t eni_min_service_start(eni_min_service_t *svc);
eni_status_t eni_min_service_tick(eni_min_service_t *svc);
eni_status_t eni_min_service_stop(eni_min_service_t *svc);
void         eni_min_service_shutdown(eni_min_service_t *svc);
void         eni_min_service_stats(const eni_min_service_t *svc);

#endif /* ENI_MIN_SERVICE_H */
