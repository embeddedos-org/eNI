// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef ENI_STIMULATOR_H
#define ENI_STIMULATOR_H

#include "eni/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    eni_stim_type_t type;
    uint8_t         channel;
    float           amplitude;
    uint32_t        duration_ms;
    float           frequency_hz;
    uint8_t         pattern;
} eni_stim_params_t;

typedef struct {
    bool            active;
    eni_stim_type_t current_type;
    float           current_amplitude;
    uint32_t        elapsed_ms;
    uint32_t        total_stimulations;
} eni_stim_status_t;

typedef struct eni_stimulator_s eni_stimulator_t;

typedef struct {
    const char  *name;
    eni_status_t (*init)(eni_stimulator_t *stim, const void *config);
    eni_status_t (*stimulate)(eni_stimulator_t *stim, const eni_stim_params_t *params);
    eni_status_t (*stop)(eni_stimulator_t *stim);
    eni_status_t (*get_status)(eni_stimulator_t *stim, eni_stim_status_t *status);
    void         (*shutdown)(eni_stimulator_t *stim);
} eni_stimulator_ops_t;

struct eni_stimulator_s {
    char                       name[64];
    const eni_stimulator_ops_t *ops;
    void                       *ctx;
    bool                        active;
    uint32_t                    supported_types;
};

#ifdef __cplusplus
}
#endif

#endif /* ENI_STIMULATOR_H */
