// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Consent Management - Smart contract-based data permissions

#ifndef ENI_BC_CONSENT_H
#define ENI_BC_CONSENT_H

#include "eni_bc/types.h"
#include "eni_bc/client.h"

/**
 * @brief Grant consent to a grantee for specific data operations
 *
 * @param client      Blockchain client
 * @param grantee     Address of entity receiving consent
 * @param flags       Consent flags (bitwise OR of eni_bc_consent_flags_t)
 * @param duration_s  Duration in seconds (0 = indefinite)
 * @param receipt     Output: transaction receipt
 * @return Status code
 */
eni_bc_status_t eni_bc_consent_grant(eni_bc_client_t *client,
                                      const eni_bc_address_t grantee,
                                      eni_bc_consent_flags_t flags,
                                      uint64_t duration_s,
                                      eni_bc_tx_receipt_t *receipt);

/**
 * @brief Revoke previously granted consent
 */
eni_bc_status_t eni_bc_consent_revoke(eni_bc_client_t *client,
                                       const eni_bc_address_t grantee,
                                       eni_bc_tx_receipt_t *receipt);

/**
 * @brief Revoke all consents granted by the user
 */
eni_bc_status_t eni_bc_consent_revoke_all(eni_bc_client_t *client,
                                           eni_bc_tx_receipt_t *receipt);

/**
 * @brief Check if specific consent is granted
 *
 * @param client   Blockchain client
 * @param user     Data owner address
 * @param grantee  Entity requesting access
 * @param flags    Required consent flags
 * @param granted  Output: true if all flags are granted
 * @return Status code
 */
eni_bc_status_t eni_bc_consent_check(eni_bc_client_t *client,
                                      const eni_bc_address_t user,
                                      const eni_bc_address_t grantee,
                                      eni_bc_consent_flags_t flags,
                                      bool *granted);

/**
 * @brief Get detailed consent record
 */
eni_bc_status_t eni_bc_consent_get(eni_bc_client_t *client,
                                    const eni_bc_address_t user,
                                    const eni_bc_address_t grantee,
                                    eni_bc_consent_record_t *record);

/**
 * @brief List all consents granted by a user
 */
eni_bc_status_t eni_bc_consent_list_granted(eni_bc_client_t *client,
                                             const eni_bc_address_t user,
                                             eni_bc_consent_record_t *records,
                                             size_t max_records,
                                             size_t *count);

/**
 * @brief List all consents received by a grantee
 */
eni_bc_status_t eni_bc_consent_list_received(eni_bc_client_t *client,
                                              const eni_bc_address_t grantee,
                                              eni_bc_consent_record_t *records,
                                              size_t max_records,
                                              size_t *count);

/**
 * @brief Update consent flags (user only)
 */
eni_bc_status_t eni_bc_consent_update(eni_bc_client_t *client,
                                       const eni_bc_address_t grantee,
                                       eni_bc_consent_flags_t new_flags,
                                       eni_bc_tx_receipt_t *receipt);

/**
 * @brief Extend consent expiration
 */
eni_bc_status_t eni_bc_consent_extend(eni_bc_client_t *client,
                                       const eni_bc_address_t grantee,
                                       uint64_t additional_duration_s,
                                       eni_bc_tx_receipt_t *receipt);

/**
 * @brief Helper: Check consent before data operation (combines check + enforce)
 */
eni_bc_status_t eni_bc_consent_require(eni_bc_client_t *client,
                                        const eni_bc_address_t user,
                                        eni_bc_consent_flags_t required_flags);

#endif /* ENI_BC_CONSENT_H */
