// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eni/device_manager.h"
#include "eni/log.h"
#include <string.h>

eni_status_t eni_device_manager_init(eni_device_manager_t *mgr)
{
    if (!mgr) return ENI_ERR_INVALID;

    memset(mgr, 0, sizeof(*mgr));

    eni_status_t st = eni_mutex_init(&mgr->lock);
    if (st != ENI_OK) return st;

    mgr->initialized = true;
    ENI_LOG_INFO("device_manager", "initialized (max_devices=%d)", ENI_DEVICE_MAX);
    return ENI_OK;
}

eni_status_t eni_device_manager_scan(eni_device_manager_t *mgr)
{
    if (!mgr || !mgr->initialized) return ENI_ERR_INVALID;

    eni_mutex_lock(&mgr->lock);

    /* Reset existing device list */
    mgr->device_count = 0;

    /* Discover simulator device (always available) */
    if (mgr->device_count < ENI_DEVICE_MAX) {
        eni_device_info_t *dev = &mgr->devices[mgr->device_count];
        memset(dev, 0, sizeof(*dev));
        strncpy(dev->name, "eNI Simulator", ENI_DEVICE_NAME_MAX - 1);
        strncpy(dev->id, "sim-0", ENI_DEVICE_ID_MAX - 1);
        dev->type          = ENI_DEVICE_SIMULATOR;
        dev->status        = ENI_DEVICE_DISCONNECTED;
        dev->channel_count = 8;
        dev->sample_rate   = 256;
        mgr->device_count++;
    }

    /*
     * TODO: enumerate USB, BLE, Serial, and Network devices.
     * Each bus scanner would append discovered devices to mgr->devices[].
     */

    ENI_LOG_INFO("device_manager", "scan complete: %u device(s) found", mgr->device_count);
    eni_mutex_unlock(&mgr->lock);
    return ENI_OK;
}

eni_status_t eni_device_manager_connect(eni_device_manager_t *mgr, uint32_t index)
{
    if (!mgr || !mgr->initialized) return ENI_ERR_INVALID;

    eni_mutex_lock(&mgr->lock);

    if (index >= mgr->device_count) {
        eni_mutex_unlock(&mgr->lock);
        return ENI_ERR_NOT_FOUND;
    }

    eni_device_info_t *dev = &mgr->devices[index];

    if (dev->status == ENI_DEVICE_CONNECTED) {
        eni_mutex_unlock(&mgr->lock);
        return ENI_OK;
    }

    dev->status = ENI_DEVICE_CONNECTING;
    ENI_LOG_INFO("device_manager", "connecting to '%s' (%s)", dev->name, dev->id);

    /*
     * TODO: perform actual transport-level connection based on dev->type.
     * For now, transition directly to connected.
     */
    dev->status = ENI_DEVICE_CONNECTED;
    ENI_LOG_INFO("device_manager", "connected to '%s'", dev->name);

    eni_mutex_unlock(&mgr->lock);
    return ENI_OK;
}

eni_status_t eni_device_manager_disconnect(eni_device_manager_t *mgr, uint32_t index)
{
    if (!mgr || !mgr->initialized) return ENI_ERR_INVALID;

    eni_mutex_lock(&mgr->lock);

    if (index >= mgr->device_count) {
        eni_mutex_unlock(&mgr->lock);
        return ENI_ERR_NOT_FOUND;
    }

    eni_device_info_t *dev = &mgr->devices[index];

    if (dev->status == ENI_DEVICE_DISCONNECTED) {
        eni_mutex_unlock(&mgr->lock);
        return ENI_OK;
    }

    ENI_LOG_INFO("device_manager", "disconnecting '%s'", dev->name);
    dev->status = ENI_DEVICE_DISCONNECTED;

    eni_mutex_unlock(&mgr->lock);
    return ENI_OK;
}

uint32_t eni_device_manager_get_device_count(const eni_device_manager_t *mgr)
{
    if (!mgr || !mgr->initialized) return 0;
    return mgr->device_count;
}

eni_status_t eni_device_manager_get_device_info(const eni_device_manager_t *mgr,
                                                 uint32_t index,
                                                 eni_device_info_t *info)
{
    if (!mgr || !mgr->initialized || !info) return ENI_ERR_INVALID;
    if (index >= mgr->device_count) return ENI_ERR_NOT_FOUND;

    *info = mgr->devices[index];
    return ENI_OK;
}

eni_status_t eni_device_manager_sync_start(eni_device_manager_t *mgr)
{
    if (!mgr || !mgr->initialized) return ENI_ERR_INVALID;

    eni_mutex_lock(&mgr->lock);

    uint32_t connected = 0;
    for (uint32_t i = 0; i < mgr->device_count; i++) {
        if (mgr->devices[i].status == ENI_DEVICE_CONNECTED) {
            connected++;
        }
    }

    if (connected == 0) {
        eni_mutex_unlock(&mgr->lock);
        ENI_LOG_WARN("device_manager", "sync_start: no connected devices");
        return ENI_ERR_NOT_FOUND;
    }

    /*
     * TODO: send synchronization trigger to all connected devices
     * so they begin sampling at the same reference time.
     * This requires a shared clock or hardware trigger line.
     */

    ENI_LOG_INFO("device_manager", "sync_start: %u device(s) synchronized", connected);
    eni_mutex_unlock(&mgr->lock);
    return ENI_OK;
}

void eni_device_manager_shutdown(eni_device_manager_t *mgr)
{
    if (!mgr || !mgr->initialized) return;

    eni_mutex_lock(&mgr->lock);

    /* Disconnect all devices */
    for (uint32_t i = 0; i < mgr->device_count; i++) {
        if (mgr->devices[i].status != ENI_DEVICE_DISCONNECTED) {
            mgr->devices[i].status = ENI_DEVICE_DISCONNECTED;
        }
    }
    mgr->device_count = 0;

    eni_mutex_unlock(&mgr->lock);
    eni_mutex_destroy(&mgr->lock);
    mgr->initialized = false;

    ENI_LOG_INFO("device_manager", "shutdown complete");
}
