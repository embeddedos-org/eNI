// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Mock provider implementation for lifecycle testing

#include "mock_provider.h"
#include <string.h>
#include <stdio.h>

static eni_status_t mock_init(eni_provider_t *prov, const void *config) {
    (void)config;
    mock_provider_ctx_t *ctx = (mock_provider_ctx_t *)prov->ctx;
    if (!ctx) return ENI_ERR_INVALID;

    ctx->init_called++;
    if (ctx->fail_on_init) return ENI_ERR_RUNTIME;
    return ENI_OK;
}

static eni_status_t mock_start(eni_provider_t *prov) {
    mock_provider_ctx_t *ctx = (mock_provider_ctx_t *)prov->ctx;
    if (!ctx) return ENI_ERR_INVALID;

    ctx->start_called++;
    if (ctx->fail_on_start) return ENI_ERR_RUNTIME;
    return ENI_OK;
}

static eni_status_t mock_poll(eni_provider_t *prov, eni_event_t *ev) {
    mock_provider_ctx_t *ctx = (mock_provider_ctx_t *)prov->ctx;
    if (!ctx || !ev) return ENI_ERR_INVALID;

    ctx->poll_count++;
    if (ctx->fail_on_poll) return ENI_ERR_RUNTIME;

    if (ctx->event_queue_head < ctx->event_queue_count) {
        *ev = ctx->event_queue[ctx->event_queue_head++];
        return ENI_OK;
    }

    if (ctx->events_to_generate > 0 && ctx->events_generated < ctx->events_to_generate) {
        memset(ev, 0, sizeof(*ev));
        ev->type = ENI_EVENT_INTENT;
        ev->id = (uint32_t)ctx->events_generated;
        snprintf(ev->source, ENI_EVENT_SOURCE_MAX, "mock");
        snprintf(ev->payload.intent.name, ENI_EVENT_INTENT_MAX, "mock_intent_%d",
                 ctx->events_generated);
        ev->payload.intent.confidence = 0.9f;
        ctx->events_generated++;
        return ENI_OK;
    }

    return ENI_ERR_NOT_FOUND;
}

static eni_status_t mock_stop(eni_provider_t *prov) {
    mock_provider_ctx_t *ctx = (mock_provider_ctx_t *)prov->ctx;
    if (!ctx) return ENI_ERR_INVALID;

    ctx->stop_called++;
    return ENI_OK;
}

static void mock_shutdown(eni_provider_t *prov) {
    mock_provider_ctx_t *ctx = (mock_provider_ctx_t *)prov->ctx;
    if (!ctx) return;
    ctx->shutdown_called++;
}

const eni_provider_ops_t mock_provider_ops = {
    .name     = "mock",
    .init     = mock_init,
    .poll     = mock_poll,
    .start    = mock_start,
    .stop     = mock_stop,
    .shutdown = mock_shutdown,
};

static eni_status_t mock_fail_init(eni_provider_t *prov, const void *config) {
    (void)prov; (void)config;
    return ENI_ERR_RUNTIME;
}

const eni_provider_ops_t mock_provider_failing_ops = {
    .name     = "mock_failing",
    .init     = mock_fail_init,
    .poll     = mock_poll,
    .start    = mock_start,
    .stop     = mock_stop,
    .shutdown = mock_shutdown,
};

void mock_provider_ctx_init(mock_provider_ctx_t *ctx) {
    if (!ctx) return;
    memset(ctx, 0, sizeof(*ctx));
}

void mock_provider_ctx_reset(mock_provider_ctx_t *ctx) {
    mock_provider_ctx_init(ctx);
}

void mock_provider_enqueue_event(mock_provider_ctx_t *ctx, const eni_event_t *ev) {
    if (!ctx || !ev) return;
    if (ctx->event_queue_count >= MOCK_PROVIDER_MAX_EVENTS) return;
    ctx->event_queue[ctx->event_queue_count++] = *ev;
}

void mock_provider_set_fail_init(mock_provider_ctx_t *ctx, int fail) {
    if (ctx) ctx->fail_on_init = fail;
}

void mock_provider_set_fail_start(mock_provider_ctx_t *ctx, int fail) {
    if (ctx) ctx->fail_on_start = fail;
}

void mock_provider_set_fail_poll(mock_provider_ctx_t *ctx, int fail) {
    if (ctx) ctx->fail_on_poll = fail;
}
