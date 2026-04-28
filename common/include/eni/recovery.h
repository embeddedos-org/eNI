// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Error recovery — exponential backoff retry and auto-reconnect

#ifndef ENI_RECOVERY_H
#define ENI_RECOVERY_H

#include "eni/types.h"
#include "eni_platform/platform.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int      max_retries;
    uint32_t base_delay_ms;
    uint32_t max_delay_ms;
    int      current_retries;
    uint32_t current_delay_ms;
    bool     jitter;
} eni_recovery_t;

typedef eni_status_t (*eni_connect_func_t)(void *ctx);

typedef struct {
    eni_recovery_t     recovery;
    eni_connect_func_t connect_fn;
    void              *connect_ctx;
    bool               connected;
} eni_auto_reconnect_t;

void         eni_recovery_init(eni_recovery_t *rec, int max_retries,
                                uint32_t base_delay_ms, uint32_t max_delay_ms);
bool         eni_recovery_should_retry(eni_recovery_t *rec);
uint32_t     eni_recovery_get_delay_ms(const eni_recovery_t *rec);
void         eni_recovery_reset(eni_recovery_t *rec);

void         eni_auto_reconnect_init(eni_auto_reconnect_t *ar,
                                      eni_connect_func_t fn, void *ctx,
                                      int max_retries, uint32_t base_delay_ms);
eni_status_t eni_auto_reconnect_attempt(eni_auto_reconnect_t *ar);

#ifdef __cplusplus
}
#endif

#endif /* ENI_RECOVERY_H */
