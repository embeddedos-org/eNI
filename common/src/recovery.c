// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

#include "eni/recovery.h"
#include "eni/log.h"
#include <stdlib.h>

void eni_recovery_init(eni_recovery_t *rec, int max_retries,
                        uint32_t base_delay_ms, uint32_t max_delay_ms)
{
    if (!rec) return;
    rec->max_retries    = max_retries;
    rec->base_delay_ms  = base_delay_ms;
    rec->max_delay_ms   = max_delay_ms;
    rec->current_retries = 0;
    rec->current_delay_ms = base_delay_ms;
    rec->jitter = true;
}

bool eni_recovery_should_retry(eni_recovery_t *rec)
{
    if (!rec) return false;
    if (rec->current_retries >= rec->max_retries) return false;

    rec->current_retries++;

    /* Exponential backoff */
    rec->current_delay_ms = rec->base_delay_ms;
    for (int i = 1; i < rec->current_retries && rec->current_delay_ms < rec->max_delay_ms; i++)
        rec->current_delay_ms *= 2;

    if (rec->current_delay_ms > rec->max_delay_ms)
        rec->current_delay_ms = rec->max_delay_ms;

    /* Add jitter (up to 25% of delay) */
    if (rec->jitter && rec->current_delay_ms > 0) {
        uint32_t jitter = (uint32_t)(rand() % (int)(rec->current_delay_ms / 4 + 1));
        rec->current_delay_ms += jitter;
    }

    return true;
}

uint32_t eni_recovery_get_delay_ms(const eni_recovery_t *rec)
{
    return rec ? rec->current_delay_ms : 0;
}

void eni_recovery_reset(eni_recovery_t *rec)
{
    if (!rec) return;
    rec->current_retries = 0;
    rec->current_delay_ms = rec->base_delay_ms;
}

void eni_auto_reconnect_init(eni_auto_reconnect_t *ar,
                              eni_connect_func_t fn, void *ctx,
                              int max_retries, uint32_t base_delay_ms)
{
    if (!ar) return;
    eni_recovery_init(&ar->recovery, max_retries, base_delay_ms, base_delay_ms * 64);
    ar->connect_fn  = fn;
    ar->connect_ctx = ctx;
    ar->connected   = false;
}

eni_status_t eni_auto_reconnect_attempt(eni_auto_reconnect_t *ar)
{
    if (!ar || !ar->connect_fn) return ENI_ERR_INVALID;

    while (eni_recovery_should_retry(&ar->recovery)) {
        ENI_LOG_INFO("recovery", "reconnect attempt %d/%d (delay=%u ms)",
                     ar->recovery.current_retries, ar->recovery.max_retries,
                     ar->recovery.current_delay_ms);

        eni_platform_sleep_ms(eni_recovery_get_delay_ms(&ar->recovery));

        eni_status_t rc = ar->connect_fn(ar->connect_ctx);
        if (rc == ENI_OK) {
            ar->connected = true;
            eni_recovery_reset(&ar->recovery);
            ENI_LOG_INFO("recovery", "reconnected successfully");
            return ENI_OK;
        }
    }

    ENI_LOG_ERROR("recovery", "all reconnect attempts exhausted");
    return ENI_ERR_CONNECT;
}
