// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Health monitoring and watchdog

#ifndef ENI_FW_HEALTH_H
#define ENI_FW_HEALTH_H

#include "eni/types.h"
#include "eni_platform/platform.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ENI_HEALTH_MAX_CHECKS 16

typedef enum {
    ENI_HEALTH_UNKNOWN,
    ENI_HEALTH_HEALTHY,
    ENI_HEALTH_DEGRADED,
    ENI_HEALTH_UNHEALTHY,
} eni_health_status_t;

typedef eni_health_status_t (*eni_health_check_fn_t)(void *ctx);

typedef struct {
    char                  name[32];
    eni_health_check_fn_t check_fn;
    void                 *ctx;
    uint32_t              interval_ms;
    uint64_t              last_check_ms;
    eni_health_status_t   last_status;
    int                   consecutive_failures;
} eni_health_check_t;

typedef struct {
    char                name[32];
    eni_health_status_t status;
    int                 failures;
} eni_health_report_entry_t;

typedef struct {
    eni_health_report_entry_t entries[ENI_HEALTH_MAX_CHECKS];
    int                       count;
    eni_health_status_t       overall;
} eni_health_report_t;

typedef struct {
    eni_health_check_t checks[ENI_HEALTH_MAX_CHECKS];
    int                check_count;
    uint32_t           watchdog_timeout_ms;
    uint64_t           last_heartbeat_ms;
    eni_mutex_t        lock;
    bool               initialized;
} eni_health_monitor_t;

eni_status_t eni_health_monitor_init(eni_health_monitor_t *hm, uint32_t watchdog_timeout_ms);
eni_status_t eni_health_monitor_register(eni_health_monitor_t *hm, const char *name,
                                          eni_health_check_fn_t fn, void *ctx,
                                          uint32_t interval_ms);
eni_status_t eni_health_monitor_tick(eni_health_monitor_t *hm);
void         eni_health_monitor_heartbeat(eni_health_monitor_t *hm);
eni_health_status_t eni_health_monitor_get_status(const eni_health_monitor_t *hm);
eni_status_t eni_health_monitor_get_report(const eni_health_monitor_t *hm,
                                            eni_health_report_t *report);
void         eni_health_monitor_destroy(eni_health_monitor_t *hm);

#ifdef __cplusplus
}
#endif

#endif /* ENI_FW_HEALTH_H */
