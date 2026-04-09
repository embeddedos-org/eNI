// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Blockchain client implementation

#include "eni_bc/client.h"
#include "eni_bc/crypto.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct eni_bc_client {
    eni_bc_client_config_t config;
    bool                   connected;
    bool                   has_wallet;
    eni_bc_privkey_t       privkey;
    eni_bc_address_t       address;
    eni_bc_event_callback_t event_callback;
    void                   *callback_data;
};

void eni_bc_client_config_defaults(eni_bc_client_config_t *cfg)
{
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    cfg->network = ENI_BC_NETWORK_LOCAL;
    cfg->rpc_url = "http://localhost:8545";
    cfg->ws_url = "ws://localhost:8546";
    cfg->timeout_ms = 30000;
    cfg->retry_count = 3;
    cfg->chain_id = 1337; /* Local dev chain */
}

eni_bc_status_t eni_bc_client_create(eni_bc_client_t **client,
                                      const eni_bc_client_config_t *config)
{
    if (!client) return ENI_BC_ERR_INVALID;

    eni_bc_client_t *c = (eni_bc_client_t *)calloc(1, sizeof(eni_bc_client_t));
    if (!c) return ENI_BC_ERR_NOMEM;

    if (config) {
        c->config = *config;
    } else {
        eni_bc_client_config_defaults(&c->config);
    }

    *client = c;
    return ENI_BC_OK;
}

eni_bc_status_t eni_bc_client_connect(eni_bc_client_t *client)
{
    if (!client) return ENI_BC_ERR_INVALID;
    if (client->connected) return ENI_BC_OK;

    /* In a real implementation, this would:
     * 1. Establish HTTP/WebSocket connection to RPC endpoint
     * 2. Verify chain ID matches expected
     * 3. Subscribe to relevant contract events
     */

    printf("[eni_bc] Connecting to %s (chain_id=%llu)...\n",
           client->config.rpc_url, (unsigned long long)client->config.chain_id);

    /* Simulate successful connection */
    client->connected = true;
    printf("[eni_bc] Connected successfully\n");

    return ENI_BC_OK;
}

void eni_bc_client_disconnect(eni_bc_client_t *client)
{
    if (!client) return;
    if (!client->connected) return;

    printf("[eni_bc] Disconnecting...\n");
    client->connected = false;
}

void eni_bc_client_destroy(eni_bc_client_t *client)
{
    if (!client) return;

    eni_bc_client_disconnect(client);

    /* Securely clear private key */
    memset(client->privkey, 0, sizeof(client->privkey));

    free(client);
}

bool eni_bc_client_is_connected(const eni_bc_client_t *client)
{
    return client && client->connected;
}

eni_bc_status_t eni_bc_client_get_block_number(eni_bc_client_t *client,
                                                uint64_t *block_number)
{
    if (!client || !block_number) return ENI_BC_ERR_INVALID;
    if (!client->connected) return ENI_BC_ERR_CONNECT;

    /* In real implementation: eth_blockNumber RPC call */
    *block_number = 12345678; /* Placeholder */
    return ENI_BC_OK;
}

eni_bc_status_t eni_bc_client_get_receipt(eni_bc_client_t *client,
                                           const eni_bc_hash_t tx_hash,
                                           eni_bc_tx_receipt_t *receipt)
{
    if (!client || !tx_hash || !receipt) return ENI_BC_ERR_INVALID;
    if (!client->connected) return ENI_BC_ERR_CONNECT;

    /* In real implementation: eth_getTransactionReceipt RPC call */
    memset(receipt, 0, sizeof(*receipt));
    memcpy(receipt->tx_hash, tx_hash, ENI_BC_HASH_SIZE);
    receipt->confirmed = true;

    return ENI_BC_OK;
}

eni_bc_status_t eni_bc_client_subscribe(eni_bc_client_t *client,
                                         eni_bc_event_callback_t callback,
                                         void *user_data)
{
    if (!client || !callback) return ENI_BC_ERR_INVALID;

    client->event_callback = callback;
    client->callback_data = user_data;

    return ENI_BC_OK;
}

void eni_bc_client_unsubscribe(eni_bc_client_t *client)
{
    if (!client) return;
    client->event_callback = NULL;
    client->callback_data = NULL;
}

eni_bc_status_t eni_bc_client_set_wallet(eni_bc_client_t *client,
                                          const eni_bc_privkey_t private_key)
{
    if (!client || !private_key) return ENI_BC_ERR_INVALID;

    memcpy(client->privkey, private_key, ENI_BC_PRIVKEY_SIZE);

    /* Derive address from private key */
    eni_bc_pubkey_t pubkey;
    eni_bc_status_t st = eni_bc_privkey_to_pubkey(private_key, pubkey);
    if (st != ENI_BC_OK) return st;

    st = eni_bc_pubkey_to_address(pubkey, client->address);
    if (st != ENI_BC_OK) return st;

    client->has_wallet = true;

    char addr_hex[43];
    eni_bc_address_to_hex(client->address, addr_hex);
    printf("[eni_bc] Wallet set: %s\n", addr_hex);

    return ENI_BC_OK;
}

eni_bc_status_t eni_bc_client_get_address(const eni_bc_client_t *client,
                                           eni_bc_address_t address)
{
    if (!client || !address) return ENI_BC_ERR_INVALID;
    if (!client->has_wallet) return ENI_BC_ERR_INVALID;

    memcpy(address, client->address, ENI_BC_ADDRESS_SIZE);
    return ENI_BC_OK;
}

/* Need to declare this - it's defined in crypto.c */
eni_bc_status_t eni_bc_privkey_to_pubkey(const eni_bc_privkey_t privkey,
                                          eni_bc_pubkey_t pubkey);
