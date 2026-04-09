// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Blockchain Client - Connection and transaction management

#ifndef ENI_BC_CLIENT_H
#define ENI_BC_CLIENT_H

#include "eni_bc/types.h"

typedef struct eni_bc_client eni_bc_client_t;

typedef struct {
    eni_bc_network_t  network;
    const char       *rpc_url;
    const char       *ws_url;
    uint32_t          timeout_ms;
    uint32_t          retry_count;
    uint64_t          chain_id;
    const char       *data_contract;
    const char       *consent_contract;
    const char       *device_contract;
} eni_bc_client_config_t;

typedef void (*eni_bc_event_callback_t)(const eni_bc_audit_entry_t *entry, void *user_data);

/**
 * @brief Initialize default client configuration
 */
void eni_bc_client_config_defaults(eni_bc_client_config_t *cfg);

/**
 * @brief Create a new blockchain client
 */
eni_bc_status_t eni_bc_client_create(eni_bc_client_t **client,
                                      const eni_bc_client_config_t *config);

/**
 * @brief Connect to the blockchain network
 */
eni_bc_status_t eni_bc_client_connect(eni_bc_client_t *client);

/**
 * @brief Disconnect from the blockchain network
 */
void eni_bc_client_disconnect(eni_bc_client_t *client);

/**
 * @brief Destroy the client and free resources
 */
void eni_bc_client_destroy(eni_bc_client_t *client);

/**
 * @brief Check if client is connected
 */
bool eni_bc_client_is_connected(const eni_bc_client_t *client);

/**
 * @brief Get current block number
 */
eni_bc_status_t eni_bc_client_get_block_number(eni_bc_client_t *client,
                                                uint64_t *block_number);

/**
 * @brief Get transaction receipt
 */
eni_bc_status_t eni_bc_client_get_receipt(eni_bc_client_t *client,
                                           const eni_bc_hash_t tx_hash,
                                           eni_bc_tx_receipt_t *receipt);

/**
 * @brief Subscribe to contract events
 */
eni_bc_status_t eni_bc_client_subscribe(eni_bc_client_t *client,
                                         eni_bc_event_callback_t callback,
                                         void *user_data);

/**
 * @brief Unsubscribe from events
 */
void eni_bc_client_unsubscribe(eni_bc_client_t *client);

/**
 * @brief Set wallet for signing transactions
 */
eni_bc_status_t eni_bc_client_set_wallet(eni_bc_client_t *client,
                                          const eni_bc_privkey_t private_key);

/**
 * @brief Get wallet address
 */
eni_bc_status_t eni_bc_client_get_address(const eni_bc_client_t *client,
                                           eni_bc_address_t address);

#endif /* ENI_BC_CLIENT_H */
