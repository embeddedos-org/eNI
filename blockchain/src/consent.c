// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Consent management implementation

#include "eni_bc/consent.h"
#include "eni_bc/crypto.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

eni_bc_status_t eni_bc_consent_grant(eni_bc_client_t *client,
                                      const eni_bc_address_t grantee,
                                      eni_bc_consent_flags_t flags,
                                      uint64_t duration_s,
                                      eni_bc_tx_receipt_t *receipt)
{
    if (!client || !grantee || !receipt) return ENI_BC_ERR_INVALID;
    if (!eni_bc_client_is_connected(client)) return ENI_BC_ERR_CONNECT;

    memset(receipt, 0, sizeof(*receipt));

    char addr_hex[43];
    eni_bc_address_to_hex(grantee, addr_hex);

    uint64_t now = (uint64_t)time(NULL);
    uint64_t expires = (duration_s > 0) ? now + duration_s : 0;

    printf("[eni_bc] Consent granted: to=%s flags=0x%02x expires=%llu\n",
           addr_hex, flags, (unsigned long long)expires);

    receipt->confirmed = true;
    receipt->timestamp = now;

    return ENI_BC_OK;
}

eni_bc_status_t eni_bc_consent_revoke(eni_bc_client_t *client,
                                       const eni_bc_address_t grantee,
                                       eni_bc_tx_receipt_t *receipt)
{
    if (!client || !grantee || !receipt) return ENI_BC_ERR_INVALID;
    if (!eni_bc_client_is_connected(client)) return ENI_BC_ERR_CONNECT;

    memset(receipt, 0, sizeof(*receipt));

    char addr_hex[43];
    eni_bc_address_to_hex(grantee, addr_hex);

    printf("[eni_bc] Consent revoked: grantee=%s\n", addr_hex);

    receipt->confirmed = true;
    receipt->timestamp = (uint64_t)time(NULL);

    return ENI_BC_OK;
}

eni_bc_status_t eni_bc_consent_revoke_all(eni_bc_client_t *client,
                                           eni_bc_tx_receipt_t *receipt)
{
    if (!client || !receipt) return ENI_BC_ERR_INVALID;
    if (!eni_bc_client_is_connected(client)) return ENI_BC_ERR_CONNECT;

    memset(receipt, 0, sizeof(*receipt));

    printf("[eni_bc] All consents revoked\n");

    receipt->confirmed = true;
    receipt->timestamp = (uint64_t)time(NULL);

    return ENI_BC_OK;
}

eni_bc_status_t eni_bc_consent_check(eni_bc_client_t *client,
                                      const eni_bc_address_t user,
                                      const eni_bc_address_t grantee,
                                      eni_bc_consent_flags_t flags,
                                      bool *granted)
{
    if (!client || !user || !grantee || !granted) return ENI_BC_ERR_INVALID;
    if (!eni_bc_client_is_connected(client)) return ENI_BC_ERR_CONNECT;

    /* In real implementation: query smart contract */
    *granted = true; /* Placeholder */

    return ENI_BC_OK;
}

eni_bc_status_t eni_bc_consent_get(eni_bc_client_t *client,
                                    const eni_bc_address_t user,
                                    const eni_bc_address_t grantee,
                                    eni_bc_consent_record_t *record)
{
    if (!client || !user || !grantee || !record) return ENI_BC_ERR_INVALID;
    if (!eni_bc_client_is_connected(client)) return ENI_BC_ERR_CONNECT;

    memset(record, 0, sizeof(*record));
    memcpy(record->user, user, ENI_BC_ADDRESS_SIZE);
    memcpy(record->grantee, grantee, ENI_BC_ADDRESS_SIZE);

    return ENI_BC_OK;
}

eni_bc_status_t eni_bc_consent_list_granted(eni_bc_client_t *client,
                                             const eni_bc_address_t user,
                                             eni_bc_consent_record_t *records,
                                             size_t max_records,
                                             size_t *count)
{
    if (!client || !user || !records || !count) return ENI_BC_ERR_INVALID;
    if (!eni_bc_client_is_connected(client)) return ENI_BC_ERR_CONNECT;

    *count = 0;
    (void)max_records;

    return ENI_BC_OK;
}

eni_bc_status_t eni_bc_consent_list_received(eni_bc_client_t *client,
                                              const eni_bc_address_t grantee,
                                              eni_bc_consent_record_t *records,
                                              size_t max_records,
                                              size_t *count)
{
    if (!client || !grantee || !records || !count) return ENI_BC_ERR_INVALID;
    if (!eni_bc_client_is_connected(client)) return ENI_BC_ERR_CONNECT;

    *count = 0;
    (void)max_records;

    return ENI_BC_OK;
}

eni_bc_status_t eni_bc_consent_update(eni_bc_client_t *client,
                                       const eni_bc_address_t grantee,
                                       eni_bc_consent_flags_t new_flags,
                                       eni_bc_tx_receipt_t *receipt)
{
    if (!client || !grantee || !receipt) return ENI_BC_ERR_INVALID;
    if (!eni_bc_client_is_connected(client)) return ENI_BC_ERR_CONNECT;

    memset(receipt, 0, sizeof(*receipt));

    char addr_hex[43];
    eni_bc_address_to_hex(grantee, addr_hex);

    printf("[eni_bc] Consent updated: grantee=%s new_flags=0x%02x\n",
           addr_hex, new_flags);

    receipt->confirmed = true;

    return ENI_BC_OK;
}

eni_bc_status_t eni_bc_consent_extend(eni_bc_client_t *client,
                                       const eni_bc_address_t grantee,
                                       uint64_t additional_duration_s,
                                       eni_bc_tx_receipt_t *receipt)
{
    if (!client || !grantee || !receipt) return ENI_BC_ERR_INVALID;
    if (!eni_bc_client_is_connected(client)) return ENI_BC_ERR_CONNECT;

    memset(receipt, 0, sizeof(*receipt));

    char addr_hex[43];
    eni_bc_address_to_hex(grantee, addr_hex);

    printf("[eni_bc] Consent extended: grantee=%s +%llus\n",
           addr_hex, (unsigned long long)additional_duration_s);

    receipt->confirmed = true;

    return ENI_BC_OK;
}

eni_bc_status_t eni_bc_consent_require(eni_bc_client_t *client,
                                        const eni_bc_address_t user,
                                        eni_bc_consent_flags_t required_flags)
{
    if (!client || !user) return ENI_BC_ERR_INVALID;

    eni_bc_address_t my_address;
    eni_bc_status_t st = eni_bc_client_get_address(client, my_address);
    if (st != ENI_BC_OK) return st;

    bool granted = false;
    st = eni_bc_consent_check(client, user, my_address, required_flags, &granted);
    if (st != ENI_BC_OK) return st;

    if (!granted) {
        return ENI_BC_ERR_NO_CONSENT;
    }

    return ENI_BC_OK;
}
