// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef ENI_CONFIG_H
#define ENI_CONFIG_H

#include "eni/types.h"

#define ENI_CONFIG_MAX_PROVIDERS 8
#define ENI_CONFIG_MAX_ALLOW     32
#define ENI_CONFIG_MAX_DENY      32

typedef struct {
    float    min_confidence;
    uint32_t debounce_ms;
} eni_filter_config_t;

typedef struct {
    const char *allow[ENI_CONFIG_MAX_ALLOW];
    int         allow_count;
    const char *deny[ENI_CONFIG_MAX_DENY];
    int         deny_count;
    bool        require_confirmation;
} eni_policy_config_t;

typedef struct {
    bool metrics;
    bool audit;
    bool trace;
} eni_observability_config_t;

typedef struct {
    const char     *name;
    eni_transport_t transport;
} eni_provider_config_t;

typedef struct {
    uint32_t max_latency_ms;
    bool     local_only;
} eni_routing_class_config_t;

typedef struct {
    uint32_t epoch_size;
    uint32_t overlap;
    uint32_t sample_rate;
    float    artifact_threshold;
} eni_dsp_config_t;

typedef struct {
    char model_path[256];
    int  num_classes;
    float confidence_threshold;
} eni_decoder_config_t;

typedef struct {
    float    max_amplitude;
    uint32_t max_duration_ms;
    float    rate_limit_hz;
    uint32_t enabled_types;
} eni_stimulator_config_t;

typedef struct {
    eni_variant_t variant;
    eni_mode_t    mode;

    /* Provider(s) */
    eni_provider_config_t providers[ENI_CONFIG_MAX_PROVIDERS];
    int                   provider_count;

    /* Filters (ENI-Min) */
    eni_filter_config_t filter;

    /* Policy */
    eni_policy_config_t policy;

    /* Routing (ENI-Framework) */
    eni_routing_class_config_t routing;

    /* Observability (ENI-Framework) */
    eni_observability_config_t observability;

    /* DSP (optional signal processing) */
    eni_dsp_config_t dsp;

    /* Decoder (optional neural decoding) */
    eni_decoder_config_t decoder;

    /* Stimulator (optional feedback loop) */
    eni_stimulator_config_t stimulator;
} eni_config_t;

eni_status_t eni_config_init(eni_config_t *cfg);
eni_status_t eni_config_load_file(eni_config_t *cfg, const char *path);
eni_status_t eni_config_load_defaults(eni_config_t *cfg, eni_variant_t variant);
void         eni_config_dump(const eni_config_t *cfg);

#endif /* ENI_CONFIG_H */
