// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "stimulator_sim.h"
#include "eni/log.h"
#include <string.h>

typedef struct {
    eni_stim_status_t status;
    eni_stim_params_t last_params;
} stim_sim_ctx_t;

static stim_sim_ctx_t g_stim_sim;

static eni_status_t sim_init(eni_stimulator_t *stim, const void *config) {
    (void)config;
    memset(&g_stim_sim, 0, sizeof(g_stim_sim));
    stim->ctx = &g_stim_sim;
    stim->supported_types = (1u << ENI_STIM_VISUAL) | (1u << ENI_STIM_AUDITORY) |
                            (1u << ENI_STIM_HAPTIC) | (1u << ENI_STIM_NEURAL) |
                            (1u << ENI_STIM_TACS) | (1u << ENI_STIM_TDCS);
    return ENI_OK;
}

static eni_status_t sim_stimulate(eni_stimulator_t *stim, const eni_stim_params_t *params) {
    if (!stim || !params) return ENI_ERR_INVALID;
    stim_sim_ctx_t *ctx = (stim_sim_ctx_t *)stim->ctx;
    ctx->last_params = *params;
    ctx->status.active = true;
    ctx->status.current_type = params->type;
    ctx->status.current_amplitude = params->amplitude;
    ctx->status.elapsed_ms = 0;
    ctx->status.total_stimulations++;
    stim->active = true;

    static const char *type_names[] = {"visual","auditory","haptic","neural","tacs","tdcs"};
    const char *tname = (params->type <= ENI_STIM_TDCS) ? type_names[params->type] : "unknown";
    ENI_LOG_INFO("stim.sim", "stimulate: type=%s ch=%d amp=%.2f dur=%u freq=%.1f",
                 tname, params->channel, params->amplitude,
                 params->duration_ms, params->frequency_hz);
    return ENI_OK;
}

static eni_status_t sim_stop(eni_stimulator_t *stim) {
    if (!stim) return ENI_ERR_INVALID;
    stim_sim_ctx_t *ctx = (stim_sim_ctx_t *)stim->ctx;
    ctx->status.active = false;
    stim->active = false;
    return ENI_OK;
}

static eni_status_t sim_get_status(eni_stimulator_t *stim, eni_stim_status_t *status) {
    if (!stim || !status) return ENI_ERR_INVALID;
    stim_sim_ctx_t *ctx = (stim_sim_ctx_t *)stim->ctx;
    *status = ctx->status;
    return ENI_OK;
}

static void sim_shutdown(eni_stimulator_t *stim) {
    if (stim) { stim->active = false; stim->ctx = NULL; }
    memset(&g_stim_sim, 0, sizeof(g_stim_sim));
}

const eni_stimulator_ops_t eni_stimulator_sim_ops = {
    .name       = "simulator",
    .init       = sim_init,
    .stimulate  = sim_stimulate,
    .stop       = sim_stop,
    .get_status = sim_get_status,
    .shutdown   = sim_shutdown,
};
