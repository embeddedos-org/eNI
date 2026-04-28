// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef ENI_DEVICE_MANAGER_H
#define ENI_DEVICE_MANAGER_H

#include "eni/types.h"
#include "eni_platform/platform.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ENI_DEVICE_NAME_MAX  64
#define ENI_DEVICE_ID_MAX    64
#define ENI_DEVICE_MAX       16

typedef enum {
    ENI_DEVICE_USB,
    ENI_DEVICE_BLE,
    ENI_DEVICE_SERIAL,
    ENI_DEVICE_NETWORK,
    ENI_DEVICE_SIMULATOR,
} eni_device_type_t;

typedef enum {
    ENI_DEVICE_DISCONNECTED,
    ENI_DEVICE_CONNECTING,
    ENI_DEVICE_CONNECTED,
    ENI_DEVICE_ERROR,
} eni_device_status_t;

typedef struct {
    char                 name[ENI_DEVICE_NAME_MAX];
    char                 id[ENI_DEVICE_ID_MAX];
    eni_device_type_t    type;
    eni_device_status_t  status;
    uint32_t             channel_count;
    uint32_t             sample_rate;
} eni_device_info_t;

typedef struct {
    eni_device_info_t    devices[ENI_DEVICE_MAX];
    uint32_t             device_count;
    eni_mutex_t          lock;
    bool                 initialized;
} eni_device_manager_t;

eni_status_t eni_device_manager_init(eni_device_manager_t *mgr);
eni_status_t eni_device_manager_scan(eni_device_manager_t *mgr);
eni_status_t eni_device_manager_connect(eni_device_manager_t *mgr, uint32_t index);
eni_status_t eni_device_manager_disconnect(eni_device_manager_t *mgr, uint32_t index);
uint32_t     eni_device_manager_get_device_count(const eni_device_manager_t *mgr);
eni_status_t eni_device_manager_get_device_info(const eni_device_manager_t *mgr,
                                                 uint32_t index,
                                                 eni_device_info_t *info);
eni_status_t eni_device_manager_sync_start(eni_device_manager_t *mgr);
void         eni_device_manager_shutdown(eni_device_manager_t *mgr);

#ifdef __cplusplus
}
#endif

#endif /* ENI_DEVICE_MANAGER_H */
