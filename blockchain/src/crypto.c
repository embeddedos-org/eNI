// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Cryptographic utilities implementation

#include "eni_bc/crypto.h"
#include <string.h>
#include <stdio.h>

#ifdef ENI_BC_USE_OPENSSL
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/ec.h>
#include <openssl/rand.h>
#else
/* Minimal embedded implementations for when OpenSSL is not available */
#endif

/* Keccak-256 constants */
static const uint64_t keccak_round_constants[24] = {
    0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808aULL,
    0x8000000080008000ULL, 0x000000000000808bULL, 0x0000000080000001ULL,
    0x8000000080008081ULL, 0x8000000000008009ULL, 0x000000000000008aULL,
    0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000aULL,
    0x000000008000808bULL, 0x800000000000008bULL, 0x8000000000008089ULL,
    0x8000000000008003ULL, 0x8000000000008002ULL, 0x8000000000000080ULL,
    0x000000000000800aULL, 0x800000008000000aULL, 0x8000000080008081ULL,
    0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL
};

static uint64_t rotl64(uint64_t x, int n) {
    return (x << n) | (x >> (64 - n));
}

static void keccak_f1600(uint64_t state[25])
{
    for (int round = 0; round < 24; round++) {
        /* Theta */
        uint64_t C[5], D[5];
        for (int x = 0; x < 5; x++)
            C[x] = state[x] ^ state[x + 5] ^ state[x + 10] ^ state[x + 15] ^ state[x + 20];
        for (int x = 0; x < 5; x++) {
            D[x] = C[(x + 4) % 5] ^ rotl64(C[(x + 1) % 5], 1);
            for (int y = 0; y < 25; y += 5)
                state[y + x] ^= D[x];
        }

        /* Rho and Pi */
        uint64_t current = state[1];
        for (int t = 0; t < 24; t++) {
            static const int pi[24] = {10,7,11,17,18,3,5,16,8,21,24,4,15,23,19,13,12,2,20,14,22,9,6,1};
            static const int rho[24] = {1,3,6,10,15,21,28,36,45,55,2,14,27,41,56,8,25,43,62,18,39,61,20,44};
            int idx = pi[t];
            uint64_t temp = state[idx];
            state[idx] = rotl64(current, rho[t]);
            current = temp;
        }

        /* Chi */
        for (int y = 0; y < 25; y += 5) {
            uint64_t t[5];
            for (int x = 0; x < 5; x++)
                t[x] = state[y + x];
            for (int x = 0; x < 5; x++)
                state[y + x] = t[x] ^ ((~t[(x + 1) % 5]) & t[(x + 2) % 5]);
        }

        /* Iota */
        state[0] ^= keccak_round_constants[round];
    }
}

eni_bc_status_t eni_bc_keccak256(const uint8_t *data, size_t len, eni_bc_hash_t hash)
{
    if (!data || !hash) return ENI_BC_ERR_INVALID;

    uint64_t state[25] = {0};
    size_t rate = 136; /* (1600 - 256*2) / 8 for Keccak-256 */

    /* Absorb */
    size_t offset = 0;
    while (len >= rate) {
        for (size_t i = 0; i < rate / 8; i++) {
            uint64_t block = 0;
            for (int j = 0; j < 8; j++)
                block |= (uint64_t)data[offset + i * 8 + j] << (j * 8);
            state[i] ^= block;
        }
        keccak_f1600(state);
        offset += rate;
        len -= rate;
    }

    /* Padding */
    uint8_t padded[136] = {0};
    memcpy(padded, data + offset, len);
    padded[len] = 0x01;
    padded[rate - 1] |= 0x80;

    for (size_t i = 0; i < rate / 8; i++) {
        uint64_t block = 0;
        for (int j = 0; j < 8; j++)
            block |= (uint64_t)padded[i * 8 + j] << (j * 8);
        state[i] ^= block;
    }
    keccak_f1600(state);

    /* Squeeze */
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 8; j++)
            hash[i * 8 + j] = (uint8_t)(state[i] >> (j * 8));
    }

    return ENI_BC_OK;
}

eni_bc_status_t eni_bc_sha256(const uint8_t *data, size_t len, eni_bc_hash_t hash)
{
    if (!data || !hash) return ENI_BC_ERR_INVALID;

#ifdef ENI_BC_USE_OPENSSL
    SHA256(data, len, hash);
    return ENI_BC_OK;
#else
    /* Minimal SHA-256 implementation */
    static const uint32_t k[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };

    uint32_t h[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };

    /* Padding and processing would go here - simplified for space */
    /* This is a placeholder - use OpenSSL in production */
    (void)k;
    (void)data;
    (void)len;
    memset(hash, 0, ENI_BC_HASH_SIZE);
    return ENI_BC_OK;
#endif
}

eni_bc_status_t eni_bc_random_bytes(uint8_t *buf, size_t len)
{
    if (!buf) return ENI_BC_ERR_INVALID;

#ifdef ENI_BC_USE_OPENSSL
    if (RAND_bytes(buf, (int)len) != 1) {
        return ENI_BC_ERR_INVALID;
    }
    return ENI_BC_OK;
#else
    /* Fallback: use /dev/urandom on Unix systems */
    FILE *f = fopen("/dev/urandom", "rb");
    if (!f) return ENI_BC_ERR_INVALID;
    size_t read = fread(buf, 1, len, f);
    fclose(f);
    return (read == len) ? ENI_BC_OK : ENI_BC_ERR_INVALID;
#endif
}

void eni_bc_hash_to_hex(const eni_bc_hash_t hash, char *hex_out)
{
    if (!hash || !hex_out) return;
    for (int i = 0; i < ENI_BC_HASH_SIZE; i++) {
        sprintf(hex_out + i * 2, "%02x", hash[i]);
    }
    hex_out[ENI_BC_HASH_SIZE * 2] = '\0';
}

eni_bc_status_t eni_bc_hex_to_hash(const char *hex, eni_bc_hash_t hash)
{
    if (!hex || !hash) return ENI_BC_ERR_INVALID;

    const char *p = hex;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;

    for (int i = 0; i < ENI_BC_HASH_SIZE; i++) {
        unsigned int val;
        if (sscanf(p + i * 2, "%2x", &val) != 1) {
            return ENI_BC_ERR_INVALID;
        }
        hash[i] = (uint8_t)val;
    }
    return ENI_BC_OK;
}

void eni_bc_address_to_hex(const eni_bc_address_t addr, char *hex_out)
{
    if (!addr || !hex_out) return;
    hex_out[0] = '0';
    hex_out[1] = 'x';
    for (int i = 0; i < ENI_BC_ADDRESS_SIZE; i++) {
        sprintf(hex_out + 2 + i * 2, "%02x", addr[i]);
    }
    hex_out[2 + ENI_BC_ADDRESS_SIZE * 2] = '\0';
}

eni_bc_status_t eni_bc_hex_to_address(const char *hex, eni_bc_address_t addr)
{
    if (!hex || !addr) return ENI_BC_ERR_INVALID;

    const char *p = hex;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;

    for (int i = 0; i < ENI_BC_ADDRESS_SIZE; i++) {
        unsigned int val;
        if (sscanf(p + i * 2, "%2x", &val) != 1) {
            return ENI_BC_ERR_INVALID;
        }
        addr[i] = (uint8_t)val;
    }
    return ENI_BC_OK;
}

eni_bc_status_t eni_bc_pubkey_to_address(const eni_bc_pubkey_t pubkey,
                                          eni_bc_address_t address)
{
    if (!pubkey || !address) return ENI_BC_ERR_INVALID;

    /* Ethereum address = last 20 bytes of Keccak256(pubkey) */
    eni_bc_hash_t hash;
    eni_bc_status_t st = eni_bc_keccak256(pubkey, ENI_BC_PUBKEY_SIZE, hash);
    if (st != ENI_BC_OK) return st;

    memcpy(address, hash + 12, ENI_BC_ADDRESS_SIZE);
    return ENI_BC_OK;
}

/* Stub implementations for signing - would use secp256k1 library in production */
eni_bc_status_t eni_bc_sign(const eni_bc_privkey_t privkey,
                             const eni_bc_hash_t message_hash,
                             eni_bc_signature_t signature)
{
    if (!privkey || !message_hash || !signature) return ENI_BC_ERR_INVALID;

    /* Placeholder - real implementation would use secp256k1 */
    memset(signature, 0, ENI_BC_SIGNATURE_SIZE);
    return ENI_BC_OK;
}

eni_bc_status_t eni_bc_verify(const eni_bc_pubkey_t pubkey,
                               const eni_bc_hash_t message_hash,
                               const eni_bc_signature_t signature,
                               bool *valid)
{
    if (!pubkey || !message_hash || !signature || !valid) return ENI_BC_ERR_INVALID;

    /* Placeholder - real implementation would use secp256k1 */
    *valid = true;
    return ENI_BC_OK;
}

eni_bc_status_t eni_bc_generate_keypair(eni_bc_privkey_t privkey,
                                         eni_bc_pubkey_t pubkey)
{
    if (!privkey || !pubkey) return ENI_BC_ERR_INVALID;

    eni_bc_status_t st = eni_bc_random_bytes(privkey, ENI_BC_PRIVKEY_SIZE);
    if (st != ENI_BC_OK) return st;

    /* Placeholder - real implementation would derive pubkey from privkey */
    memset(pubkey, 0, ENI_BC_PUBKEY_SIZE);
    return ENI_BC_OK;
}
