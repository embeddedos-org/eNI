// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Fixed-size block pool allocator for real-time paths

#ifndef ENI_MEMPOOL_H
#define ENI_MEMPOOL_H

#include "eni/types.h"
#include "eni_platform/platform.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct eni_mempool_block_s {
    struct eni_mempool_block_s *next;
} eni_mempool_block_t;

typedef struct {
    uint8_t             *base;
    size_t               buffer_size;
    size_t               block_size;
    size_t               num_blocks;
    size_t               allocated_count;
    eni_mempool_block_t *free_list;
    eni_mutex_t          lock;
    bool                 initialized;
} eni_mempool_t;

typedef struct {
    size_t block_size;
    size_t num_blocks;
    size_t allocated;
    size_t available;
} eni_mempool_stats_t;

eni_status_t eni_mempool_init(eni_mempool_t *pool, void *buffer,
                               size_t buffer_size, size_t block_size);
void        *eni_mempool_alloc(eni_mempool_t *pool);
void         eni_mempool_free(eni_mempool_t *pool, void *ptr);
void         eni_mempool_reset(eni_mempool_t *pool);
void         eni_mempool_stats(const eni_mempool_t *pool, eni_mempool_stats_t *stats);
void         eni_mempool_destroy(eni_mempool_t *pool);

#ifdef __cplusplus
}
#endif

#endif /* ENI_MEMPOOL_H */
