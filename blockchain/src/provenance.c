// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Data provenance implementation

#include "eni_bc/provenance.h"
#include "eni_bc/crypto.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

eni_bc_status_t eni_bc_record_data(eni_bc_client_t *client,
                                    const uint8_t *data,
                                    size_t data_len,
                                    eni_bc_data_type_t data_type,
                                    const eni_bc_address_t device,
                                    const char *metadata,
                                    eni_bc_data_record_t *record)
{
    if (!client || !data || data_len == 0 || !record) {
        return ENI_BC_ERR_INVALID;
    }
    if (!eni_bc_client_is_connected(client)) {
        return ENI_BC_ERR_CONNECT;
    }

    memset(record, 0, sizeof(*record));

    /* Compute hash of the data (not storing actual data on-chain) */
    eni_bc_status_t st = eni_bc_keccak256(data, data_len, record->data_hash);
    if (st != ENI_BC_OK) return st;

    record->data_type = data_type;
    record->timestamp = (uint64_t)time(NULL);
    record->size = (uint32_t)data_len;

    /* Get owner address from client wallet */
    st = eni_bc_client_get_address(client, record->owner);
    if (st != ENI_BC_OK) return st;

    if (device) {
        memcpy(record->device, device, ENI_BC_ADDRESS_SIZE);
    }

    if (metadata) {
        size_t len = strlen(metadata);
        if (len >= ENI_BC_MAX_METADATA) len = ENI_BC_MAX_METADATA - 1;
        memcpy(record->metadata, metadata, len);
        record->metadata[len] = '\0';
    }

    /* In real implementation: call smart contract recordData() */
    char hash_hex[65];
    eni_bc_hash_to_hex(record->data_hash, hash_hex);
    printf("[eni_bc] Data recorded: %s (type=%s, size=%u)\n",
           hash_hex, eni_bc_data_type_str(data_type), record->size);

    return ENI_BC_OK;
}

eni_bc_status_t eni_bc_log_access(eni_bc_client_t *client,
                                   const eni_bc_hash_t data_hash,
                                   const eni_bc_address_t accessor,
                                   const char *purpose)
{
    if (!client || !data_hash || !accessor) return ENI_BC_ERR_INVALID;
    if (!eni_bc_client_is_connected(client)) return ENI_BC_ERR_CONNECT;

    char hash_hex[65], addr_hex[43];
    eni_bc_hash_to_hex(data_hash, hash_hex);
    eni_bc_address_to_hex(accessor, addr_hex);

    printf("[eni_bc] Access logged: data=%s by=%s purpose=%s\n",
           hash_hex, addr_hex, purpose ? purpose : "(none)");

    return ENI_BC_OK;
}

eni_bc_status_t eni_bc_log_share(eni_bc_client_t *client,
                                  const eni_bc_hash_t data_hash,
                                  const eni_bc_address_t recipient,
                                  const char *terms)
{
    if (!client || !data_hash || !recipient) return ENI_BC_ERR_INVALID;
    if (!eni_bc_client_is_connected(client)) return ENI_BC_ERR_CONNECT;

    char hash_hex[65], addr_hex[43];
    eni_bc_hash_to_hex(data_hash, hash_hex);
    eni_bc_address_to_hex(recipient, addr_hex);

    printf("[eni_bc] Share logged: data=%s to=%s terms=%s\n",
           hash_hex, addr_hex, terms ? terms : "(none)");

    return ENI_BC_OK;
}

eni_bc_status_t eni_bc_log_deletion(eni_bc_client_t *client,
                                     const eni_bc_hash_t data_hash,
                                     const char *reason)
{
    if (!client || !data_hash) return ENI_BC_ERR_INVALID;
    if (!eni_bc_client_is_connected(client)) return ENI_BC_ERR_CONNECT;

    char hash_hex[65];
    eni_bc_hash_to_hex(data_hash, hash_hex);

    printf("[eni_bc] Deletion logged: data=%s reason=%s\n",
           hash_hex, reason ? reason : "(none)");

    return ENI_BC_OK;
}

eni_bc_status_t eni_bc_verify_data(eni_bc_client_t *client,
                                    const uint8_t *data,
                                    size_t data_len,
                                    const eni_bc_hash_t expected_hash,
                                    bool *is_valid)
{
    if (!client || !data || data_len == 0 || !expected_hash || !is_valid) {
        return ENI_BC_ERR_INVALID;
    }

    eni_bc_hash_t computed_hash;
    eni_bc_status_t st = eni_bc_keccak256(data, data_len, computed_hash);
    if (st != ENI_BC_OK) return st;

    *is_valid = (memcmp(computed_hash, expected_hash, ENI_BC_HASH_SIZE) == 0);

    return ENI_BC_OK;
}

eni_bc_status_t eni_bc_get_data_record(eni_bc_client_t *client,
                                        const eni_bc_hash_t data_hash,
                                        eni_bc_data_record_t *record)
{
    if (!client || !data_hash || !record) return ENI_BC_ERR_INVALID;
    if (!eni_bc_client_is_connected(client)) return ENI_BC_ERR_CONNECT;

    /* In real implementation: query smart contract */
    memset(record, 0, sizeof(*record));
    memcpy(record->data_hash, data_hash, ENI_BC_HASH_SIZE);

    return ENI_BC_OK;
}

eni_bc_status_t eni_bc_get_audit_trail(eni_bc_client_t *client,
                                        const eni_bc_hash_t data_hash,
                                        eni_bc_audit_entry_t *entries,
                                        size_t max_entries,
                                        size_t *count)
{
    if (!client || !data_hash || !entries || !count) return ENI_BC_ERR_INVALID;
    if (!eni_bc_client_is_connected(client)) return ENI_BC_ERR_CONNECT;

    /* In real implementation: query contract events */
    *count = 0;
    (void)max_entries;

    return ENI_BC_OK;
}

eni_bc_status_t eni_bc_get_user_data(eni_bc_client_t *client,
                                      const eni_bc_address_t user,
                                      eni_bc_data_record_t *records,
                                      size_t max_records,
                                      size_t *count)
{
    if (!client || !user || !records || !count) return ENI_BC_ERR_INVALID;
    if (!eni_bc_client_is_connected(client)) return ENI_BC_ERR_CONNECT;

    /* In real implementation: query contract */
    *count = 0;
    (void)max_records;

    return ENI_BC_OK;
}
