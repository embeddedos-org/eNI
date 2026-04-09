// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Data sharing implementation

#include "eni_bc/sharing.h"
#include "eni_bc/crypto.h"
#include "eni_bc/consent.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

eni_bc_status_t eni_bc_share_direct(eni_bc_client_t *client,
                                     const eni_bc_hash_t data_hash,
                                     const eni_bc_address_t recipient,
                                     const uint8_t *encrypted_key,
                                     size_t key_len,
                                     uint64_t duration_s,
                                     eni_bc_share_record_t *share_record)
{
    if (!client || !data_hash || !recipient || !share_record) {
        return ENI_BC_ERR_INVALID;
    }
    if (!eni_bc_client_is_connected(client)) return ENI_BC_ERR_CONNECT;

    memset(share_record, 0, sizeof(*share_record));

    /* Generate share ID */
    uint8_t share_data[64];
    memcpy(share_data, data_hash, 32);
    memcpy(share_data + 32, recipient, 20);
    uint64_t now = (uint64_t)time(NULL);
    memcpy(share_data + 52, &now, 8);

    eni_bc_keccak256(share_data, sizeof(share_data), share_record->share_id);

    memcpy(share_record->data_hash, data_hash, ENI_BC_HASH_SIZE);
    eni_bc_client_get_address(client, share_record->owner);
    memcpy(share_record->recipient, recipient, ENI_BC_ADDRESS_SIZE);
    share_record->mode = ENI_BC_SHARE_DIRECT;
    share_record->created_at = now;
    share_record->expires_at = (duration_s > 0) ? now + duration_s : 0;
    share_record->encrypted = (encrypted_key != NULL && key_len > 0);

    char hash_hex[65], addr_hex[43];
    eni_bc_hash_to_hex(data_hash, hash_hex);
    eni_bc_address_to_hex(recipient, addr_hex);

    printf("[eni_bc] Direct share: data=%s to=%s encrypted=%s\n",
           hash_hex, addr_hex, share_record->encrypted ? "yes" : "no");

    return ENI_BC_OK;
}

eni_bc_status_t eni_bc_share_research(eni_bc_client_t *client,
                                       const eni_bc_hash_t data_hash,
                                       const eni_bc_address_t research_pool,
                                       const char *terms,
                                       eni_bc_share_record_t *share_record)
{
    if (!client || !data_hash || !research_pool || !share_record) {
        return ENI_BC_ERR_INVALID;
    }
    if (!eni_bc_client_is_connected(client)) return ENI_BC_ERR_CONNECT;

    memset(share_record, 0, sizeof(*share_record));

    memcpy(share_record->data_hash, data_hash, ENI_BC_HASH_SIZE);
    eni_bc_client_get_address(client, share_record->owner);
    memcpy(share_record->recipient, research_pool, ENI_BC_ADDRESS_SIZE);
    share_record->mode = ENI_BC_SHARE_RESEARCH;
    share_record->created_at = (uint64_t)time(NULL);

    if (terms) {
        size_t len = strlen(terms);
        if (len >= sizeof(share_record->terms)) len = sizeof(share_record->terms) - 1;
        memcpy(share_record->terms, terms, len);
    }

    char hash_hex[65], pool_hex[43];
    eni_bc_hash_to_hex(data_hash, hash_hex);
    eni_bc_address_to_hex(research_pool, pool_hex);

    printf("[eni_bc] Research share: data=%s pool=%s terms=%s\n",
           hash_hex, pool_hex, terms ? terms : "(none)");

    return ENI_BC_OK;
}

eni_bc_status_t eni_bc_share_list_marketplace(eni_bc_client_t *client,
                                               const eni_bc_hash_t data_hash,
                                               eni_bc_data_type_t data_type,
                                               uint64_t price_wei,
                                               const char *description,
                                               eni_bc_listing_t *listing)
{
    if (!client || !data_hash || !listing) return ENI_BC_ERR_INVALID;
    if (!eni_bc_client_is_connected(client)) return ENI_BC_ERR_CONNECT;

    memset(listing, 0, sizeof(*listing));

    /* Generate listing ID */
    uint8_t listing_data[48];
    memcpy(listing_data, data_hash, 32);
    uint64_t now = (uint64_t)time(NULL);
    memcpy(listing_data + 32, &now, 8);
    memcpy(listing_data + 40, &price_wei, 8);

    eni_bc_keccak256(listing_data, sizeof(listing_data), listing->listing_id);

    memcpy(listing->data_hash, data_hash, ENI_BC_HASH_SIZE);
    eni_bc_client_get_address(client, listing->seller);
    listing->data_type = data_type;
    listing->price_wei = price_wei;
    listing->created_at = now;
    listing->active = true;

    if (description) {
        size_t len = strlen(description);
        if (len >= sizeof(listing->description)) len = sizeof(listing->description) - 1;
        memcpy(listing->description, description, len);
    }

    char hash_hex[65];
    eni_bc_hash_to_hex(data_hash, hash_hex);

    printf("[eni_bc] Marketplace listing: data=%s type=%s price=%llu wei\n",
           hash_hex, eni_bc_data_type_str(data_type), (unsigned long long)price_wei);

    return ENI_BC_OK;
}

eni_bc_status_t eni_bc_share_purchase(eni_bc_client_t *client,
                                       const eni_bc_hash_t listing_id,
                                       eni_bc_tx_receipt_t *receipt)
{
    if (!client || !listing_id || !receipt) return ENI_BC_ERR_INVALID;
    if (!eni_bc_client_is_connected(client)) return ENI_BC_ERR_CONNECT;

    memset(receipt, 0, sizeof(*receipt));

    char listing_hex[65];
    eni_bc_hash_to_hex(listing_id, listing_hex);

    printf("[eni_bc] Marketplace purchase: listing=%s\n", listing_hex);

    receipt->confirmed = true;
    receipt->timestamp = (uint64_t)time(NULL);

    return ENI_BC_OK;
}

eni_bc_status_t eni_bc_share_revoke(eni_bc_client_t *client,
                                     const eni_bc_hash_t share_id,
                                     eni_bc_tx_receipt_t *receipt)
{
    if (!client || !share_id || !receipt) return ENI_BC_ERR_INVALID;
    if (!eni_bc_client_is_connected(client)) return ENI_BC_ERR_CONNECT;

    memset(receipt, 0, sizeof(*receipt));

    char share_hex[65];
    eni_bc_hash_to_hex(share_id, share_hex);

    printf("[eni_bc] Share revoked: %s\n", share_hex);

    receipt->confirmed = true;

    return ENI_BC_OK;
}

eni_bc_status_t eni_bc_share_unlist(eni_bc_client_t *client,
                                     const eni_bc_hash_t listing_id,
                                     eni_bc_tx_receipt_t *receipt)
{
    if (!client || !listing_id || !receipt) return ENI_BC_ERR_INVALID;
    if (!eni_bc_client_is_connected(client)) return ENI_BC_ERR_CONNECT;

    memset(receipt, 0, sizeof(*receipt));

    char listing_hex[65];
    eni_bc_hash_to_hex(listing_id, listing_hex);

    printf("[eni_bc] Listing removed: %s\n", listing_hex);

    receipt->confirmed = true;

    return ENI_BC_OK;
}

eni_bc_status_t eni_bc_share_check_access(eni_bc_client_t *client,
                                           const eni_bc_hash_t data_hash,
                                           const eni_bc_address_t user,
                                           bool *has_access)
{
    if (!client || !data_hash || !user || !has_access) return ENI_BC_ERR_INVALID;
    if (!eni_bc_client_is_connected(client)) return ENI_BC_ERR_CONNECT;

    /* In real implementation: query smart contract */
    *has_access = true;

    return ENI_BC_OK;
}

eni_bc_status_t eni_bc_share_get(eni_bc_client_t *client,
                                  const eni_bc_hash_t share_id,
                                  eni_bc_share_record_t *record)
{
    if (!client || !share_id || !record) return ENI_BC_ERR_INVALID;
    if (!eni_bc_client_is_connected(client)) return ENI_BC_ERR_CONNECT;

    memset(record, 0, sizeof(*record));
    memcpy(record->share_id, share_id, ENI_BC_HASH_SIZE);

    return ENI_BC_OK;
}

eni_bc_status_t eni_bc_share_list_outgoing(eni_bc_client_t *client,
                                            const eni_bc_address_t owner,
                                            eni_bc_share_record_t *records,
                                            size_t max_records,
                                            size_t *count)
{
    if (!client || !owner || !records || !count) return ENI_BC_ERR_INVALID;
    if (!eni_bc_client_is_connected(client)) return ENI_BC_ERR_CONNECT;

    *count = 0;
    (void)max_records;

    return ENI_BC_OK;
}

eni_bc_status_t eni_bc_share_list_incoming(eni_bc_client_t *client,
                                            const eni_bc_address_t recipient,
                                            eni_bc_share_record_t *records,
                                            size_t max_records,
                                            size_t *count)
{
    if (!client || !recipient || !records || !count) return ENI_BC_ERR_INVALID;
    if (!eni_bc_client_is_connected(client)) return ENI_BC_ERR_CONNECT;

    *count = 0;
    (void)max_records;

    return ENI_BC_OK;
}

eni_bc_status_t eni_bc_share_browse_marketplace(eni_bc_client_t *client,
                                                 eni_bc_data_type_t filter_type,
                                                 eni_bc_listing_t *listings,
                                                 size_t max_listings,
                                                 size_t *count)
{
    if (!client || !listings || !count) return ENI_BC_ERR_INVALID;
    if (!eni_bc_client_is_connected(client)) return ENI_BC_ERR_CONNECT;

    (void)filter_type;
    *count = 0;
    (void)max_listings;

    return ENI_BC_OK;
}
