// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Main blockchain header for eNI

#ifndef ENI_BLOCKCHAIN_H
#define ENI_BLOCKCHAIN_H

#include "eni_bc/types.h"
#include "eni_bc/crypto.h"
#include "eni_bc/client.h"
#include "eni_bc/provenance.h"
#include "eni_bc/consent.h"
#include "eni_bc/device.h"
#include "eni_bc/sharing.h"

#define ENI_BC_VERSION_MAJOR 0
#define ENI_BC_VERSION_MINOR 1
#define ENI_BC_VERSION_PATCH 0
#define ENI_BC_VERSION_STRING "0.1.0"

/**
 * @brief Initialize the blockchain module
 *
 * Must be called before any other blockchain functions.
 * Initializes cryptographic libraries and checks system requirements.
 *
 * @return ENI_BC_OK on success
 */
eni_bc_status_t eni_bc_init(void);

/**
 * @brief Shutdown the blockchain module
 *
 * Releases all resources. After this call, eni_bc_init() must be
 * called again before using other blockchain functions.
 */
void eni_bc_shutdown(void);

/**
 * @brief Get blockchain module version string
 */
const char *eni_bc_version(void);

/**
 * @brief Check if blockchain module is initialized
 */
bool eni_bc_is_initialized(void);

#endif /* ENI_BLOCKCHAIN_H */
