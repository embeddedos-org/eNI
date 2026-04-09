// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Blockchain Types for Neural Interface Security

#ifndef ENI_BC_TYPES_H
#define ENI_BC_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define ENI_BC_HASH_SIZE        32
#define ENI_BC_ADDRESS_SIZE     20
#define ENI_BC_SIGNATURE_SIZE   65
#define ENI_BC_PUBKEY_SIZE      64
#define ENI_BC_PRIVKEY_SIZE     32
#define ENI_BC_MAX_DATA_SIZE    4096
#define ENI_BC_MAX_METADATA     256

typedef uint8_t eni_bc_hash_t[ENI_BC_HASH_SIZE];
typedef uint8_t eni_bc_address_t[ENI_BC_ADDRESS_SIZE];
typedef uint8_t eni_bc_signature_t[ENI_BC_SIGNATURE_SIZE];
typedef uint8_t eni_bc_pubkey_t[ENI_BC_PUBKEY_SIZE];
typedef uint8_t eni_bc_privkey_t[ENI_BC_PRIVKEY_SIZE];

typedef enum {
    ENI_BC_OK = 0,
    ENI_BC_ERR_INVALID,
    ENI_BC_ERR_NOMEM,
    ENI_BC_ERR_CONNECT,
    ENI_BC_ERR_TIMEOUT,
    ENI_BC_ERR_TX_FAILED,
    ENI_BC_ERR_NO_CONSENT,
    ENI_BC_ERR_DEVICE_UNAUTH,
    ENI_BC_ERR_SIGNATURE,
    ENI_BC_ERR_NOT_FOUND,
    ENI_BC_ERR_CONTRACT,
} eni_bc_status_t;

typedef enum {
    ENI_BC_NETWORK_MAINNET,
    ENI_BC_NETWORK_TESTNET,
    ENI_BC_NETWORK_LOCAL,
    ENI_BC_NETWORK_PRIVATE,
} eni_bc_network_t;

typedef enum {
    ENI_BC_EVENT_DATA_RECORDED,
    ENI_BC_EVENT_DATA_ACCESSED,
    ENI_BC_EVENT_DATA_SHARED,
    ENI_BC_EVENT_DATA_DELETED,
    ENI_BC_EVENT_CONSENT_GRANTED,
    ENI_BC_EVENT_CONSENT_REVOKED,
    ENI_BC_EVENT_DEVICE_REGISTERED,
    ENI_BC_EVENT_DEVICE_REVOKED,
} eni_bc_event_type_t;

typedef enum {
    ENI_BC_CONSENT_NONE       = 0,
    ENI_BC_CONSENT_COLLECT    = (1 << 0),
    ENI_BC_CONSENT_STORE      = (1 << 1),
    ENI_BC_CONSENT_PROCESS    = (1 << 2),
    ENI_BC_CONSENT_SHARE      = (1 << 3),
    ENI_BC_CONSENT_RESEARCH   = (1 << 4),
    ENI_BC_CONSENT_COMMERCIAL = (1 << 5),
    ENI_BC_CONSENT_ALL        = 0x3F,
} eni_bc_consent_flags_t;

typedef enum {
    ENI_BC_DATA_RAW_SIGNAL,
    ENI_BC_DATA_FEATURES,
    ENI_BC_DATA_INTENT,
    ENI_BC_DATA_MODEL,
    ENI_BC_DATA_FEEDBACK,
} eni_bc_data_type_t;

typedef struct {
    eni_bc_hash_t     tx_hash;
    uint64_t          block_number;
    uint64_t          timestamp;
    eni_bc_address_t  from;
    eni_bc_address_t  to;
    bool              confirmed;
} eni_bc_tx_receipt_t;

typedef struct {
    eni_bc_hash_t        data_hash;
    eni_bc_data_type_t   data_type;
    uint64_t             timestamp;
    eni_bc_address_t     owner;
    eni_bc_address_t     device;
    uint32_t             size;
    char                 metadata[ENI_BC_MAX_METADATA];
} eni_bc_data_record_t;

typedef struct {
    eni_bc_address_t      user;
    eni_bc_address_t      grantee;
    eni_bc_consent_flags_t flags;
    uint64_t              granted_at;
    uint64_t              expires_at;
    bool                  revoked;
} eni_bc_consent_record_t;

typedef struct {
    eni_bc_address_t  device_id;
    eni_bc_pubkey_t   public_key;
    eni_bc_address_t  owner;
    char              model[64];
    char              firmware_version[32];
    uint64_t          registered_at;
    bool              active;
} eni_bc_device_record_t;

typedef struct {
    eni_bc_event_type_t type;
    eni_bc_hash_t       tx_hash;
    uint64_t            block_number;
    uint64_t            timestamp;
    eni_bc_address_t    actor;
    eni_bc_hash_t       data_ref;
    char                details[256];
} eni_bc_audit_entry_t;

const char *eni_bc_status_str(eni_bc_status_t status);
const char *eni_bc_event_type_str(eni_bc_event_type_t type);
const char *eni_bc_data_type_str(eni_bc_data_type_t type);

#endif /* ENI_BC_TYPES_H */
