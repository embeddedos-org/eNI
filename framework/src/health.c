// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

#include "eni_fw/health.h"
#include "eni/log.h"
#include <string.h>

eni_status_t eni_health_monitor_init(eni_health_monitor_t *hm, uint32_t watchdog_timeout_ms)
{
    if (!hm) return ENI_ERR_INVALID;
    memset(hm, 0, sizeof(*hm));
    hm->watchdog_timeout_ms = watchdog_timeout_ms;
    hm->last_heartbeat_ms = eni_platform_monotonic_ms();

    eni_status_t rc = eni_mutex_init(&hm->lock);
    if (rc != ENI_OK) return rc;

    hm->initialized = true;
    return ENI_OK;
}

eni_status_t eni_health_monitor_register(eni_health_monitor_t *hm, const char *name,
                                          eni_health_check_fn_t fn, void *ctx,
                                          uint32_t interval_ms)
{
    if (!hm || !hm->initialized || !name || !fn) return ENI_ERR_INVALID;

    eni_mutex_lock(&hm->lock);
    if (hm->check_count >= ENI_HEALTH_MAX_CHECKS) {
        eni_mutex_unlock(&hm->lock);
        return ENI_ERR_OVERFLOW;
    }

    eni_health_check_t *c = &hm->checks[hm->check_count++];
    memset(c, 0, sizeof(*c));
    strncpy(c->name, name, sizeof(c->name) - 1);
    c->check_fn = fn;
    c->ctx = ctx;
    c->interval_ms = interval_ms;
    c->last_status = ENI_HEALTH_UNKNOWN;

    eni_mutex_unlock(&hm->lock);
    return ENI_OK;
}

eni_status_t eni_health_monitor_tick(eni_health_monitor_t *hm)
{
    if (!hm || !hm->initialized) return ENI_ERR_INVALID;

    uint64_t now = eni_platform_monotonic_ms();

    eni_mutex_lock(&hm->lock);

    /* Check watchdog */
    if (hm->watchdog_timeout_ms > 0 &&
        (now - hm->last_heartbeat_ms) > hm->watchdog_timeout_ms) {
        ENI_LOG_WARN("health", "watchdog timeout! no heartbeat for %llu ms",
                     (unsigned long long)(now - hm->last_heartbeat_ms));
    }

    /* Run due health checks */
    for (int i = 0; i < hm->check_count; i++) {
        eni_health_check_t *c = &hm->checks[i];
        if ((now - c->last_check_ms) >= c->interval_ms) {
            c->last_check_ms = now;
            eni_health_status_t status = c->check_fn(c->ctx);
            c->last_status = status;

            if (status == ENI_HEALTH_UNHEALTHY) {
                c->consecutive_failures++;
                if (c->consecutive_failures == 3) {
                    ENI_LOG_ERROR("health", "check '%s' failed 3 times", c->name);
                }
            } else {
                c->consecutive_failures = 0;
            }
        }
    }

    eni_mutex_unlock(&hm->lock);
    return ENI_OK;
}

void eni_health_monitor_heartbeat(eni_health_monitor_t *hm)
{
    if (hm && hm->initialized)
        hm->last_heartbeat_ms = eni_platform_monotonic_ms();
}

eni_health_status_t eni_health_monitor_get_status(const eni_health_monitor_t *hm)
{
    if (!hm || !hm->initialized) return ENI_HEALTH_UNKNOWN;

    eni_health_status_t worst = ENI_HEALTH_HEALTHY;
    for (int i = 0; i < hm->check_count; i++) {
        if (hm->checks[i].last_status > worst)
            worst = hm->checks[i].last_status;
    }
    return worst;
}

eni_status_t eni_health_monitor_get_report(const eni_health_monitor_t *hm,
                                            eni_health_report_t *report)
{
    if (!hm || !hm->initialized || !report) return ENI_ERR_INVALID;

    memset(report, 0, sizeof(*report));
    report->overall = ENI_HEALTH_HEALTHY;

    for (int i = 0; i < hm->check_count; i++) {
        report->entries[i].status = hm->checks[i].last_status;
        report->entries[i].failures = hm->checks[i].consecutive_failures;
        strncpy(report->entries[i].name, hm->checks[i].name, sizeof(report->entries[i].name) - 1);
        if (hm->checks[i].last_status > report->overall)
            report->overall = hm->checks[i].last_status;
    }
    report->count = hm->check_count;
    return ENI_OK;
}

void eni_health_monitor_destroy(eni_health_monitor_t *hm)
{
    if (!hm || !hm->initialized) return;
    eni_mutex_destroy(&hm->lock);
    hm->initialized = false;
}
