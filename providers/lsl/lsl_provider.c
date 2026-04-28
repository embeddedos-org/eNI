// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "lsl_provider.h"
#include "eni/log.h"
#include <string.h>
#include <stdlib.h>

/* ── Internal context ──────────────────────────────────────────────── */

typedef struct {
    eni_lsl_config_t config;

    /* Stubbed socket handles — replaced with real sockets when
     * actual LSL network transport is integrated. */
    int  outlet_fd;   /* TCP socket for pushing data       */
    int  inlet_fd;    /* TCP socket for receiving data      */
    int  discover_fd; /* UDP multicast for LSL discovery    */
    bool outlet_open;
    bool inlet_open;
} lsl_ctx_t;

/* ── Socket stubs ──────────────────────────────────────────────────── */

static int lsl_socket_create_tcp(void)
{
    /* TODO: socket(AF_INET, SOCK_STREAM, 0) */
    return 1; /* stub handle */
}

static int lsl_socket_create_udp_multicast(void)
{
    /* TODO: socket(AF_INET, SOCK_DGRAM, 0) + setsockopt SO_REUSEADDR
     *       + bind + IP_ADD_MEMBERSHIP for multicast group */
    return 2; /* stub handle */
}

static void lsl_socket_close(int fd)
{
    (void)fd;
    /* TODO: close(fd) */
}

/* ── Provider ops implementation ───────────────────────────────────── */

static eni_status_t lsl_init(eni_provider_t *prov, const void *config)
{
    lsl_ctx_t *ctx = (lsl_ctx_t *)calloc(1, sizeof(lsl_ctx_t));
    if (!ctx) return ENI_ERR_NOMEM;

    if (config) {
        const eni_lsl_config_t *cfg = (const eni_lsl_config_t *)config;
        ctx->config = *cfg;
    } else {
        strncpy(ctx->config.stream_name, "eNI_Stream", ENI_LSL_NAME_MAX - 1);
        strncpy(ctx->config.stream_type, "EEG", ENI_LSL_TYPE_MAX - 1);
        strncpy(ctx->config.host, "127.0.0.1", ENI_LSL_HOST_MAX - 1);
        ctx->config.port          = ENI_LSL_PORT_DEFAULT;
        ctx->config.channel_count = 8;
        ctx->config.nominal_srate = 256.0f;
    }

    /* Create outlet (TCP for data streaming) */
    ctx->outlet_fd   = lsl_socket_create_tcp();
    ctx->outlet_open = (ctx->outlet_fd >= 0);

    /* Create inlet (TCP for receiving) */
    ctx->inlet_fd   = lsl_socket_create_tcp();
    ctx->inlet_open = (ctx->inlet_fd >= 0);

    /* Create discovery socket (UDP multicast) */
    ctx->discover_fd = lsl_socket_create_udp_multicast();

    prov->ctx = ctx;
    prov->transport = ENI_TRANSPORT_TCP;

    ENI_LOG_INFO("lsl", "initialized stream='%s' type='%s' host=%s:%u ch=%u rate=%.1f",
                 ctx->config.stream_name, ctx->config.stream_type,
                 ctx->config.host, ctx->config.port,
                 ctx->config.channel_count, (double)ctx->config.nominal_srate);
    return ENI_OK;
}

static eni_status_t lsl_start(eni_provider_t *prov)
{
    if (!prov || !prov->ctx) return ENI_ERR_INVALID;
    ENI_LOG_INFO("lsl", "started");
    return ENI_OK;
}

static eni_status_t lsl_poll(eni_provider_t *prov, eni_event_t *ev)
{
    if (!prov || !ev) return ENI_ERR_INVALID;

    lsl_ctx_t *ctx = (lsl_ctx_t *)prov->ctx;
    if (!ctx || !ctx->inlet_open) return ENI_ERR_IO;

    /*
     * TODO: read from inlet_fd, deserialize LSL packet into eni_event_t.
     * For now, return timeout to indicate no data available.
     */
    (void)ctx;
    return ENI_ERR_TIMEOUT;
}

static eni_status_t lsl_stop(eni_provider_t *prov)
{
    if (!prov || !prov->ctx) return ENI_ERR_INVALID;
    ENI_LOG_INFO("lsl", "stopped");
    return ENI_OK;
}

static void lsl_shutdown(eni_provider_t *prov)
{
    if (!prov || !prov->ctx) return;

    lsl_ctx_t *ctx = (lsl_ctx_t *)prov->ctx;

    if (ctx->outlet_open) {
        lsl_socket_close(ctx->outlet_fd);
        ctx->outlet_open = false;
    }
    if (ctx->inlet_open) {
        lsl_socket_close(ctx->inlet_fd);
        ctx->inlet_open = false;
    }
    if (ctx->discover_fd >= 0) {
        lsl_socket_close(ctx->discover_fd);
    }

    free(ctx);
    prov->ctx = NULL;

    ENI_LOG_INFO("lsl", "shutdown");
}

/* ── Provider ops table ────────────────────────────────────────────── */

const eni_provider_ops_t eni_provider_lsl_ops = {
    .name     = "lsl",
    .init     = lsl_init,
    .poll     = lsl_poll,
    .start    = lsl_start,
    .stop     = lsl_stop,
    .shutdown = lsl_shutdown,
};

/* ── Convenience wrappers ──────────────────────────────────────────── */

eni_status_t eni_lsl_provider_init(eni_lsl_provider_t *lsl, const eni_lsl_config_t *config)
{
    if (!lsl) return ENI_ERR_INVALID;
    return eni_provider_init(&lsl->base, &eni_provider_lsl_ops, "lsl", config);
}

eni_status_t eni_lsl_provider_start(eni_lsl_provider_t *lsl)
{
    if (!lsl) return ENI_ERR_INVALID;
    return eni_provider_start(&lsl->base);
}

eni_status_t eni_lsl_provider_stop(eni_lsl_provider_t *lsl)
{
    if (!lsl) return ENI_ERR_INVALID;
    return eni_provider_stop(&lsl->base);
}

eni_status_t eni_lsl_provider_push_sample(eni_lsl_provider_t *lsl,
                                           const float *data, uint32_t count)
{
    if (!lsl || !data || count == 0) return ENI_ERR_INVALID;

    lsl_ctx_t *ctx = (lsl_ctx_t *)lsl->base.ctx;
    if (!ctx || !ctx->outlet_open) return ENI_ERR_IO;

    if (count > ENI_LSL_SAMPLE_MAX) return ENI_ERR_OVERFLOW;

    /*
     * TODO: serialize float array into LSL wire format and
     * send via outlet_fd TCP socket.
     */
    (void)data;

    return ENI_OK;
}

eni_status_t eni_lsl_provider_pull_sample(eni_lsl_provider_t *lsl,
                                           float *data, uint32_t max_count,
                                           uint32_t *actual_count)
{
    if (!lsl || !data || max_count == 0) return ENI_ERR_INVALID;

    lsl_ctx_t *ctx = (lsl_ctx_t *)lsl->base.ctx;
    if (!ctx || !ctx->inlet_open) return ENI_ERR_IO;

    /*
     * TODO: receive from inlet_fd TCP socket, deserialize LSL packet,
     * and fill data[] with up to max_count floats.
     */
    if (actual_count) *actual_count = 0;

    return ENI_ERR_TIMEOUT;
}

void eni_lsl_provider_shutdown(eni_lsl_provider_t *lsl)
{
    if (!lsl) return;
    eni_provider_shutdown(&lsl->base);
}
