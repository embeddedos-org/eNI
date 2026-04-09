// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Blockchain module initialization

#include "eni_bc/blockchain.h"
#include <stdio.h>
#include <stdbool.h>

static bool g_initialized = false;

eni_bc_status_t eni_bc_init(void)
{
    if (g_initialized) {
        return ENI_BC_OK;
    }

    printf("[eni_bc] Initializing blockchain module v%s\n", ENI_BC_VERSION_STRING);

    /* Initialize cryptographic libraries if needed */
#ifdef ENI_BC_USE_OPENSSL
    /* OpenSSL initialization would go here */
#endif

    g_initialized = true;
    printf("[eni_bc] Blockchain module initialized\n");

    return ENI_BC_OK;
}

void eni_bc_shutdown(void)
{
    if (!g_initialized) {
        return;
    }

    printf("[eni_bc] Shutting down blockchain module\n");

#ifdef ENI_BC_USE_OPENSSL
    /* OpenSSL cleanup would go here */
#endif

    g_initialized = false;
}

const char *eni_bc_version(void)
{
    return ENI_BC_VERSION_STRING;
}

bool eni_bc_is_initialized(void)
{
    return g_initialized;
}
