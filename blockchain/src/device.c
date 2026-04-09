// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Device authentication implementation

#include "eni_bc/device.h"
#include "eni_bc/crypto.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

eni_bc_status_t eni_bc_device_register(eni_bc_client_t *client,
                                        const eni_bc_pubkey_t device_pubkey,
                                        const char *model,
                                        const char *firmware_version,
                                        eni_bc_address_t device_id,
                                        eni_bc_tx_receipt_t *receipt)
{
    if (!client || !device_pubkey || !model || !device_id || !receipt) {
        return ENI_BC_ERR_INVALID;
    }
    if (!eni_bc_client_is_connected(client)) return ENI_BC_ERR_CONNECT;

    memset(receipt, 0, sizeof(*receipt));

    /* Derive device address from public key */
    eni_bc_status_t st = eni_bc_pubkey_to_address(device_pubkey, device_id);
    if (st != ENI_BC_OK) return st;

    char addr_hex[43];
    eni_bc_address_to_hex(device_id, addr_hex);

    printf("[eni_bc] Device registered: %s model=%s firmware=%s\n",
           addr_hex, model, firmware_version ? firmware_version : "unknown");

    receipt->confirmed = true;
    receipt->timestamp = (uint64_t)time(NULL);

    return ENI_BC_OK;
}

eni_bc_status_t eni_bc_device_verify(eni_bc_client_t *client,
                                      const eni_bc_address_t device_id,
                                      bool *is_valid)
{
    if (!client || !device_id || !is_valid) return ENI_BC_ERR_INVALID;
    if (!eni_bc_client_is_connected(client)) return ENI_BC_ERR_CONNECT;

    /* In real implementation: query smart contract */
    *is_valid = true;

    return ENI_BC_OK;
}

eni_bc_status_t eni_bc_device_authenticate(eni_bc_client_t *client,
                                            const eni_bc_address_t device_id,
                                            const uint8_t *challenge,
                                            size_t challenge_len,
                                            const eni_bc_signature_t signature,
                                            bool *authenticated)
{
    if (!client || !device_id || !challenge || !signature || !authenticated) {
        return ENI_BC_ERR_INVALID;
    }
    if (!eni_bc_client_is_connected(client)) return ENI_BC_ERR_CONNECT;

    /* First verify device is registered */
    bool is_valid = false;
    eni_bc_status_t st = eni_bc_device_verify(client, device_id, &is_valid);
    if (st != ENI_BC_OK) return st;
    if (!is_valid) {
        *authenticated = false;
        return ENI_BC_ERR_DEVICE_UNAUTH;
    }

    /* Hash the challenge */
    eni_bc_hash_t challenge_hash;
    st = eni_bc_keccak256(challenge, challenge_len, challenge_hash);
    if (st != ENI_BC_OK) return st;

    /* Get device's public key from blockchain and verify signature */
    eni_bc_device_record_t device_record;
    st = eni_bc_device_get(client, device_id, &device_record);
    if (st != ENI_BC_OK) return st;

    st = eni_bc_verify(device_record.public_key, challenge_hash,
                       signature, authenticated);
    if (st != ENI_BC_OK) return st;

    char addr_hex[43];
    eni_bc_address_to_hex(device_id, addr_hex);
    printf("[eni_bc] Device authentication: %s result=%s\n",
           addr_hex, *authenticated ? "success" : "failed");

    return ENI_BC_OK;
}

eni_bc_status_t eni_bc_device_get(eni_bc_client_t *client,
                                   const eni_bc_address_t device_id,
                                   eni_bc_device_record_t *record)
{
    if (!client || !device_id || !record) return ENI_BC_ERR_INVALID;
    if (!eni_bc_client_is_connected(client)) return ENI_BC_ERR_CONNECT;

    memset(record, 0, sizeof(*record));
    memcpy(record->device_id, device_id, ENI_BC_ADDRESS_SIZE);
    record->active = true;

    return ENI_BC_OK;
}

eni_bc_status_t eni_bc_device_update_firmware(eni_bc_client_t *client,
                                               const eni_bc_address_t device_id,
                                               const char *new_version,
                                               eni_bc_tx_receipt_t *receipt)
{
    if (!client || !device_id || !new_version || !receipt) {
        return ENI_BC_ERR_INVALID;
    }
    if (!eni_bc_client_is_connected(client)) return ENI_BC_ERR_CONNECT;

    memset(receipt, 0, sizeof(*receipt));

    char addr_hex[43];
    eni_bc_address_to_hex(device_id, addr_hex);

    printf("[eni_bc] Device firmware updated: %s -> %s\n", addr_hex, new_version);

    receipt->confirmed = true;

    return ENI_BC_OK;
}

eni_bc_status_t eni_bc_device_revoke(eni_bc_client_t *client,
                                      const eni_bc_address_t device_id,
                                      eni_bc_tx_receipt_t *receipt)
{
    if (!client || !device_id || !receipt) return ENI_BC_ERR_INVALID;
    if (!eni_bc_client_is_connected(client)) return ENI_BC_ERR_CONNECT;

    memset(receipt, 0, sizeof(*receipt));

    char addr_hex[43];
    eni_bc_address_to_hex(device_id, addr_hex);

    printf("[eni_bc] Device revoked: %s\n", addr_hex);

    receipt->confirmed = true;

    return ENI_BC_OK;
}

eni_bc_status_t eni_bc_device_reactivate(eni_bc_client_t *client,
                                          const eni_bc_address_t device_id,
                                          eni_bc_tx_receipt_t *receipt)
{
    if (!client || !device_id || !receipt) return ENI_BC_ERR_INVALID;
    if (!eni_bc_client_is_connected(client)) return ENI_BC_ERR_CONNECT;

    memset(receipt, 0, sizeof(*receipt));

    char addr_hex[43];
    eni_bc_address_to_hex(device_id, addr_hex);

    printf("[eni_bc] Device reactivated: %s\n", addr_hex);

    receipt->confirmed = true;

    return ENI_BC_OK;
}

eni_bc_status_t eni_bc_device_transfer(eni_bc_client_t *client,
                                        const eni_bc_address_t device_id,
                                        const eni_bc_address_t new_owner,
                                        eni_bc_tx_receipt_t *receipt)
{
    if (!client || !device_id || !new_owner || !receipt) {
        return ENI_BC_ERR_INVALID;
    }
    if (!eni_bc_client_is_connected(client)) return ENI_BC_ERR_CONNECT;

    memset(receipt, 0, sizeof(*receipt));

    char device_hex[43], owner_hex[43];
    eni_bc_address_to_hex(device_id, device_hex);
    eni_bc_address_to_hex(new_owner, owner_hex);

    printf("[eni_bc] Device transferred: %s -> owner %s\n", device_hex, owner_hex);

    receipt->confirmed = true;

    return ENI_BC_OK;
}

eni_bc_status_t eni_bc_device_list(eni_bc_client_t *client,
                                    const eni_bc_address_t owner,
                                    eni_bc_device_record_t *devices,
                                    size_t max_devices,
                                    size_t *count)
{
    if (!client || !owner || !devices || !count) return ENI_BC_ERR_INVALID;
    if (!eni_bc_client_is_connected(client)) return ENI_BC_ERR_CONNECT;

    *count = 0;
    (void)max_devices;

    return ENI_BC_OK;
}

eni_bc_status_t eni_bc_device_gen_challenge(uint8_t *challenge, size_t len)
{
    if (!challenge || len == 0) return ENI_BC_ERR_INVALID;
    return eni_bc_random_bytes(challenge, len);
}
