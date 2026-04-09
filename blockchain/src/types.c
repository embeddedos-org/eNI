// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Blockchain types implementation

#include "eni_bc/types.h"

const char *eni_bc_status_str(eni_bc_status_t status)
{
    switch (status) {
    case ENI_BC_OK:             return "OK";
    case ENI_BC_ERR_INVALID:    return "ERR_INVALID";
    case ENI_BC_ERR_NOMEM:      return "ERR_NOMEM";
    case ENI_BC_ERR_CONNECT:    return "ERR_CONNECT";
    case ENI_BC_ERR_TIMEOUT:    return "ERR_TIMEOUT";
    case ENI_BC_ERR_TX_FAILED:  return "ERR_TX_FAILED";
    case ENI_BC_ERR_NO_CONSENT: return "ERR_NO_CONSENT";
    case ENI_BC_ERR_DEVICE_UNAUTH: return "ERR_DEVICE_UNAUTH";
    case ENI_BC_ERR_SIGNATURE:  return "ERR_SIGNATURE";
    case ENI_BC_ERR_NOT_FOUND:  return "ERR_NOT_FOUND";
    case ENI_BC_ERR_CONTRACT:   return "ERR_CONTRACT";
    default:                    return "UNKNOWN";
    }
}

const char *eni_bc_event_type_str(eni_bc_event_type_t type)
{
    switch (type) {
    case ENI_BC_EVENT_DATA_RECORDED:    return "DATA_RECORDED";
    case ENI_BC_EVENT_DATA_ACCESSED:    return "DATA_ACCESSED";
    case ENI_BC_EVENT_DATA_SHARED:      return "DATA_SHARED";
    case ENI_BC_EVENT_DATA_DELETED:     return "DATA_DELETED";
    case ENI_BC_EVENT_CONSENT_GRANTED:  return "CONSENT_GRANTED";
    case ENI_BC_EVENT_CONSENT_REVOKED:  return "CONSENT_REVOKED";
    case ENI_BC_EVENT_DEVICE_REGISTERED: return "DEVICE_REGISTERED";
    case ENI_BC_EVENT_DEVICE_REVOKED:   return "DEVICE_REVOKED";
    default:                            return "UNKNOWN";
    }
}

const char *eni_bc_data_type_str(eni_bc_data_type_t type)
{
    switch (type) {
    case ENI_BC_DATA_RAW_SIGNAL: return "RAW_SIGNAL";
    case ENI_BC_DATA_FEATURES:   return "FEATURES";
    case ENI_BC_DATA_INTENT:     return "INTENT";
    case ENI_BC_DATA_MODEL:      return "MODEL";
    case ENI_BC_DATA_FEEDBACK:   return "FEEDBACK";
    default:                     return "UNKNOWN";
    }
}
