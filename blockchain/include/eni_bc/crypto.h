// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Cryptographic utilities for blockchain operations

#ifndef ENI_BC_CRYPTO_H
#define ENI_BC_CRYPTO_H

#include "eni_bc/types.h"

/**
 * @brief Compute Keccak-256 hash (Ethereum standard)
 */
eni_bc_status_t eni_bc_keccak256(const uint8_t *data, size_t len,
                                  eni_bc_hash_t hash);

/**
 * @brief Compute SHA-256 hash
 */
eni_bc_status_t eni_bc_sha256(const uint8_t *data, size_t len,
                               eni_bc_hash_t hash);

/**
 * @brief Sign data with private key (secp256k1 ECDSA)
 */
eni_bc_status_t eni_bc_sign(const eni_bc_privkey_t privkey,
                             const eni_bc_hash_t message_hash,
                             eni_bc_signature_t signature);

/**
 * @brief Verify signature
 */
eni_bc_status_t eni_bc_verify(const eni_bc_pubkey_t pubkey,
                               const eni_bc_hash_t message_hash,
                               const eni_bc_signature_t signature,
                               bool *valid);

/**
 * @brief Recover public key from signature
 */
eni_bc_status_t eni_bc_recover_pubkey(const eni_bc_hash_t message_hash,
                                       const eni_bc_signature_t signature,
                                       eni_bc_pubkey_t pubkey);

/**
 * @brief Derive Ethereum address from public key
 */
eni_bc_status_t eni_bc_pubkey_to_address(const eni_bc_pubkey_t pubkey,
                                          eni_bc_address_t address);

/**
 * @brief Generate new keypair
 */
eni_bc_status_t eni_bc_generate_keypair(eni_bc_privkey_t privkey,
                                         eni_bc_pubkey_t pubkey);

/**
 * @brief Derive public key from private key
 */
eni_bc_status_t eni_bc_privkey_to_pubkey(const eni_bc_privkey_t privkey,
                                          eni_bc_pubkey_t pubkey);

/**
 * @brief Generate random bytes (cryptographically secure)
 */
eni_bc_status_t eni_bc_random_bytes(uint8_t *buf, size_t len);

/**
 * @brief Convert hash to hex string
 */
void eni_bc_hash_to_hex(const eni_bc_hash_t hash, char *hex_out);

/**
 * @brief Convert hex string to hash
 */
eni_bc_status_t eni_bc_hex_to_hash(const char *hex, eni_bc_hash_t hash);

/**
 * @brief Convert address to hex string (with 0x prefix)
 */
void eni_bc_address_to_hex(const eni_bc_address_t addr, char *hex_out);

/**
 * @brief Convert hex string to address
 */
eni_bc_status_t eni_bc_hex_to_address(const char *hex, eni_bc_address_t addr);

#endif /* ENI_BC_CRYPTO_H */
