// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Data Provenance - Immutable record of neural data operations

#ifndef ENI_BC_PROVENANCE_H
#define ENI_BC_PROVENANCE_H

#include "eni_bc/types.h"
#include "eni_bc/client.h"

/**
 * @brief Record neural data to blockchain (stores hash, not actual data)
 *
 * @param client     Blockchain client
 * @param data       Pointer to neural data
 * @param data_len   Length of data
 * @param data_type  Type of neural data
 * @param device     Device that captured the data
 * @param metadata   Optional metadata string
 * @param record     Output: record with transaction details
 * @return Status code
 */
eni_bc_status_t eni_bc_record_data(eni_bc_client_t *client,
                                    const uint8_t *data,
                                    size_t data_len,
                                    eni_bc_data_type_t data_type,
                                    const eni_bc_address_t device,
                                    const char *metadata,
                                    eni_bc_data_record_t *record);

/**
 * @brief Log data access event to blockchain
 */
eni_bc_status_t eni_bc_log_access(eni_bc_client_t *client,
                                   const eni_bc_hash_t data_hash,
                                   const eni_bc_address_t accessor,
                                   const char *purpose);

/**
 * @brief Log data sharing event
 */
eni_bc_status_t eni_bc_log_share(eni_bc_client_t *client,
                                  const eni_bc_hash_t data_hash,
                                  const eni_bc_address_t recipient,
                                  const char *terms);

/**
 * @brief Log data deletion event
 */
eni_bc_status_t eni_bc_log_deletion(eni_bc_client_t *client,
                                     const eni_bc_hash_t data_hash,
                                     const char *reason);

/**
 * @brief Verify data integrity against blockchain record
 */
eni_bc_status_t eni_bc_verify_data(eni_bc_client_t *client,
                                    const uint8_t *data,
                                    size_t data_len,
                                    const eni_bc_hash_t expected_hash,
                                    bool *is_valid);

/**
 * @brief Get data record from blockchain
 */
eni_bc_status_t eni_bc_get_data_record(eni_bc_client_t *client,
                                        const eni_bc_hash_t data_hash,
                                        eni_bc_data_record_t *record);

/**
 * @brief Get audit trail for specific data
 */
eni_bc_status_t eni_bc_get_audit_trail(eni_bc_client_t *client,
                                        const eni_bc_hash_t data_hash,
                                        eni_bc_audit_entry_t *entries,
                                        size_t max_entries,
                                        size_t *count);

/**
 * @brief Get all data records for a user
 */
eni_bc_status_t eni_bc_get_user_data(eni_bc_client_t *client,
                                      const eni_bc_address_t user,
                                      eni_bc_data_record_t *records,
                                      size_t max_records,
                                      size_t *count);

#endif /* ENI_BC_PROVENANCE_H */
