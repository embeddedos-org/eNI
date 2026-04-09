// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Data Sharing - Decentralized secure data exchange

#ifndef ENI_BC_SHARING_H
#define ENI_BC_SHARING_H

#include "eni_bc/types.h"
#include "eni_bc/client.h"

typedef enum {
    ENI_BC_SHARE_DIRECT,      /* Direct peer-to-peer sharing */
    ENI_BC_SHARE_RESEARCH,    /* Research collaboration pool */
    ENI_BC_SHARE_MARKETPLACE, /* Data marketplace listing */
} eni_bc_share_mode_t;

typedef struct {
    eni_bc_hash_t        share_id;
    eni_bc_hash_t        data_hash;
    eni_bc_address_t     owner;
    eni_bc_address_t     recipient;
    eni_bc_share_mode_t  mode;
    uint64_t             created_at;
    uint64_t             expires_at;
    uint64_t             access_count;
    bool                 encrypted;
    char                 terms[256];
} eni_bc_share_record_t;

typedef struct {
    eni_bc_hash_t        listing_id;
    eni_bc_hash_t        data_hash;
    eni_bc_address_t     seller;
    eni_bc_data_type_t   data_type;
    uint64_t             price_wei;
    uint64_t             created_at;
    uint64_t             sales_count;
    bool                 active;
    char                 description[256];
} eni_bc_listing_t;

/**
 * @brief Share data directly with a recipient
 *
 * @param client        Blockchain client
 * @param data_hash     Hash of data to share
 * @param recipient     Recipient address
 * @param encrypted_key Encrypted access key (optional, for encrypted data)
 * @param key_len       Length of encrypted key
 * @param duration_s    Access duration in seconds (0 = indefinite)
 * @param share_record  Output: share record
 * @return Status code
 */
eni_bc_status_t eni_bc_share_direct(eni_bc_client_t *client,
                                     const eni_bc_hash_t data_hash,
                                     const eni_bc_address_t recipient,
                                     const uint8_t *encrypted_key,
                                     size_t key_len,
                                     uint64_t duration_s,
                                     eni_bc_share_record_t *share_record);

/**
 * @brief Contribute data to a research pool
 */
eni_bc_status_t eni_bc_share_research(eni_bc_client_t *client,
                                       const eni_bc_hash_t data_hash,
                                       const eni_bc_address_t research_pool,
                                       const char *terms,
                                       eni_bc_share_record_t *share_record);

/**
 * @brief List data on marketplace
 */
eni_bc_status_t eni_bc_share_list_marketplace(eni_bc_client_t *client,
                                               const eni_bc_hash_t data_hash,
                                               eni_bc_data_type_t data_type,
                                               uint64_t price_wei,
                                               const char *description,
                                               eni_bc_listing_t *listing);

/**
 * @brief Purchase data from marketplace
 */
eni_bc_status_t eni_bc_share_purchase(eni_bc_client_t *client,
                                       const eni_bc_hash_t listing_id,
                                       eni_bc_tx_receipt_t *receipt);

/**
 * @brief Revoke a share
 */
eni_bc_status_t eni_bc_share_revoke(eni_bc_client_t *client,
                                     const eni_bc_hash_t share_id,
                                     eni_bc_tx_receipt_t *receipt);

/**
 * @brief Remove marketplace listing
 */
eni_bc_status_t eni_bc_share_unlist(eni_bc_client_t *client,
                                     const eni_bc_hash_t listing_id,
                                     eni_bc_tx_receipt_t *receipt);

/**
 * @brief Check if user has access to shared data
 */
eni_bc_status_t eni_bc_share_check_access(eni_bc_client_t *client,
                                           const eni_bc_hash_t data_hash,
                                           const eni_bc_address_t user,
                                           bool *has_access);

/**
 * @brief Get share record
 */
eni_bc_status_t eni_bc_share_get(eni_bc_client_t *client,
                                  const eni_bc_hash_t share_id,
                                  eni_bc_share_record_t *record);

/**
 * @brief List all shares from a user
 */
eni_bc_status_t eni_bc_share_list_outgoing(eni_bc_client_t *client,
                                            const eni_bc_address_t owner,
                                            eni_bc_share_record_t *records,
                                            size_t max_records,
                                            size_t *count);

/**
 * @brief List all shares received by a user
 */
eni_bc_status_t eni_bc_share_list_incoming(eni_bc_client_t *client,
                                            const eni_bc_address_t recipient,
                                            eni_bc_share_record_t *records,
                                            size_t max_records,
                                            size_t *count);

/**
 * @brief Browse marketplace listings
 */
eni_bc_status_t eni_bc_share_browse_marketplace(eni_bc_client_t *client,
                                                 eni_bc_data_type_t filter_type,
                                                 eni_bc_listing_t *listings,
                                                 size_t max_listings,
                                                 size_t *count);

#endif /* ENI_BC_SHARING_H */
