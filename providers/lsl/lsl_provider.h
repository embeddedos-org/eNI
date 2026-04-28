// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef ENI_PROVIDER_LSL_H
#define ENI_PROVIDER_LSL_H

#include "eni/provider_contract.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ENI_LSL_NAME_MAX  64
#define ENI_LSL_TYPE_MAX  32
#define ENI_LSL_HOST_MAX  128
#define ENI_LSL_PORT_DEFAULT 16573
#define ENI_LSL_SAMPLE_MAX  64

extern const eni_provider_ops_t eni_provider_lsl_ops;

typedef struct {
    char     stream_name[ENI_LSL_NAME_MAX];
    char     stream_type[ENI_LSL_TYPE_MAX];
    char     host[ENI_LSL_HOST_MAX];
    uint16_t port;
    uint32_t channel_count;
    float    nominal_srate;
} eni_lsl_config_t;

typedef struct {
    eni_provider_t  base;
    eni_lsl_config_t config;
} eni_lsl_provider_t;

eni_status_t eni_lsl_provider_init(eni_lsl_provider_t *lsl, const eni_lsl_config_t *config);
eni_status_t eni_lsl_provider_start(eni_lsl_provider_t *lsl);
eni_status_t eni_lsl_provider_stop(eni_lsl_provider_t *lsl);
eni_status_t eni_lsl_provider_push_sample(eni_lsl_provider_t *lsl,
                                           const float *data, uint32_t count);
eni_status_t eni_lsl_provider_pull_sample(eni_lsl_provider_t *lsl,
                                           float *data, uint32_t max_count,
                                           uint32_t *actual_count);
void         eni_lsl_provider_shutdown(eni_lsl_provider_t *lsl);

#ifdef __cplusplus
}
#endif

#endif /* ENI_PROVIDER_LSL_H */
