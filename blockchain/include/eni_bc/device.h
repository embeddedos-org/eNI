// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Device Authentication - BCI device registration and verification

#ifndef ENI_BC_DEVICE_H
#define ENI_BC_DEVICE_H

#include "eni_bc/types.h"
#include "eni_bc/client.h"

/**
 * @brief Register a new BCI device on the blockchain
 *
 * @param client           Blockchain client
 * @param device_pubkey    Device's public key
 * @param model            Device model name
 * @param firmware_version Firmware version string
 * @param device_id        Output: generated device ID (address)
 * @param receipt          Output: transaction receipt
 * @return Status code
 */
eni_bc_status_t eni_bc_device_register(eni_bc_client_t *client,
                                        const eni_bc_pubkey_t device_pubkey,
                                        const char *model,
                                        const char *firmware_version,
                                        eni_bc_address_t device_id,
                                        eni_bc_tx_receipt_t *receipt);

/**
 * @brief Verify a device is registered and active
 */
eni_bc_status_t eni_bc_device_verify(eni_bc_client_t *client,
                                      const eni_bc_address_t device_id,
                                      bool *is_valid);

/**
 * @brief Authenticate device with signature challenge
 *
 * @param client     Blockchain client
 * @param device_id  Device address
 * @param challenge  Random challenge bytes
 * @param challenge_len Challenge length
 * @param signature  Device's signature of the challenge
 * @param authenticated Output: true if signature is valid
 * @return Status code
 */
eni_bc_status_t eni_bc_device_authenticate(eni_bc_client_t *client,
                                            const eni_bc_address_t device_id,
                                            const uint8_t *challenge,
                                            size_t challenge_len,
                                            const eni_bc_signature_t signature,
                                            bool *authenticated);

/**
 * @brief Get device record from blockchain
 */
eni_bc_status_t eni_bc_device_get(eni_bc_client_t *client,
                                   const eni_bc_address_t device_id,
                                   eni_bc_device_record_t *record);

/**
 * @brief Update device firmware version (owner only)
 */
eni_bc_status_t eni_bc_device_update_firmware(eni_bc_client_t *client,
                                               const eni_bc_address_t device_id,
                                               const char *new_version,
                                               eni_bc_tx_receipt_t *receipt);

/**
 * @brief Revoke/deactivate a device (owner only)
 */
eni_bc_status_t eni_bc_device_revoke(eni_bc_client_t *client,
                                      const eni_bc_address_t device_id,
                                      eni_bc_tx_receipt_t *receipt);

/**
 * @brief Reactivate a previously revoked device
 */
eni_bc_status_t eni_bc_device_reactivate(eni_bc_client_t *client,
                                          const eni_bc_address_t device_id,
                                          eni_bc_tx_receipt_t *receipt);

/**
 * @brief Transfer device ownership
 */
eni_bc_status_t eni_bc_device_transfer(eni_bc_client_t *client,
                                        const eni_bc_address_t device_id,
                                        const eni_bc_address_t new_owner,
                                        eni_bc_tx_receipt_t *receipt);

/**
 * @brief List all devices owned by a user
 */
eni_bc_status_t eni_bc_device_list(eni_bc_client_t *client,
                                    const eni_bc_address_t owner,
                                    eni_bc_device_record_t *devices,
                                    size_t max_devices,
                                    size_t *count);

/**
 * @brief Generate a random challenge for device authentication
 */
eni_bc_status_t eni_bc_device_gen_challenge(uint8_t *challenge, size_t len);

#endif /* ENI_BC_DEVICE_H */
