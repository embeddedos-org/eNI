// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Safe string utilities to prevent buffer overflows and ensure null termination

#ifndef ENI_SAFE_STRING_H
#define ENI_SAFE_STRING_H

#include <stddef.h>
#include <string.h>

/**
 * @brief Safe string copy with guaranteed null termination.
 *
 * Copies at most (dest_size - 1) characters from src to dest,
 * always ensuring dest is null-terminated.
 *
 * @param dest      Destination buffer
 * @param src       Source string
 * @param dest_size Total size of destination buffer (including null terminator)
 * @return          Number of characters that would have been copied if dest_size
 *                  was unlimited (like snprintf). If >= dest_size, truncation occurred.
 */
static inline size_t eni_strlcpy(char *dest, const char *src, size_t dest_size)
{
    if (dest_size == 0) return strlen(src);

    size_t src_len = strlen(src);
    size_t copy_len = (src_len < dest_size - 1) ? src_len : dest_size - 1;

    memcpy(dest, src, copy_len);
    dest[copy_len] = '\0';

    return src_len;
}

/**
 * @brief Safe string concatenation with guaranteed null termination.
 *
 * Appends src to dest, writing at most (dest_size - strlen(dest) - 1) characters.
 * Always ensures dest is null-terminated.
 *
 * @param dest      Destination buffer (must already be null-terminated)
 * @param src       Source string to append
 * @param dest_size Total size of destination buffer
 * @return          Total length of string it tried to create (like snprintf).
 *                  If >= dest_size, truncation occurred.
 */
static inline size_t eni_strlcat(char *dest, const char *src, size_t dest_size)
{
    size_t dest_len = strlen(dest);

    if (dest_len >= dest_size) return dest_size + strlen(src);

    return dest_len + eni_strlcpy(dest + dest_len, src, dest_size - dest_len);
}

/**
 * @brief Zero a buffer securely (prevents compiler optimization)
 *
 * @param ptr  Pointer to memory to zero
 * @param size Size in bytes
 */
static inline void eni_secure_zero(void *ptr, size_t size)
{
    volatile unsigned char *p = (volatile unsigned char *)ptr;
    while (size--) {
        *p++ = 0;
    }
}

/**
 * @brief Safe snprintf wrapper that always null-terminates
 *
 * @param dest      Destination buffer
 * @param dest_size Size of destination buffer
 * @param fmt       Format string
 * @return          Number of characters written (excluding null terminator),
 *                  or negative on error
 */
#define eni_snprintf(dest, dest_size, fmt, ...) \
    do { \
        int _ret = snprintf((dest), (dest_size), (fmt), ##__VA_ARGS__); \
        if ((dest_size) > 0) (dest)[(dest_size) - 1] = '\0'; \
        (void)_ret; \
    } while (0)

/**
 * @brief Check if a pointer is likely valid (basic sanity check)
 *
 * Note: This is not a foolproof check, just catches obvious issues.
 *
 * @param ptr Pointer to check
 * @return    1 if pointer seems valid, 0 otherwise
 */
static inline int eni_ptr_valid(const void *ptr)
{
    return ptr != NULL;
}

/**
 * @brief Safe memory copy with bounds checking
 *
 * @param dest      Destination buffer
 * @param dest_size Size of destination buffer
 * @param src       Source buffer
 * @param count     Number of bytes to copy
 * @return          0 on success, -1 if count > dest_size
 */
static inline int eni_memcpy_s(void *dest, size_t dest_size,
                                const void *src, size_t count)
{
    if (count > dest_size) return -1;
    if (!dest || !src) return -1;
    memcpy(dest, src, count);
    return 0;
}

/**
 * @brief Safe memset with bounds awareness
 *
 * @param dest      Destination buffer
 * @param dest_size Size of destination buffer
 * @param ch        Character to fill with
 * @param count     Number of bytes to set
 * @return          0 on success, -1 if count > dest_size
 */
static inline int eni_memset_s(void *dest, size_t dest_size,
                                int ch, size_t count)
{
    if (count > dest_size) return -1;
    if (!dest) return -1;
    memset(dest, ch, count);
    return 0;
}

#endif /* ENI_SAFE_STRING_H */
