// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

#include "eni/mempool.h"
#include <string.h>

eni_status_t eni_mempool_init(eni_mempool_t *pool, void *buffer,
                               size_t buffer_size, size_t block_size)
{
    if (!pool || !buffer || buffer_size == 0 || block_size == 0)
        return ENI_ERR_INVALID;

    /* Align block size to pointer size */
    size_t align = sizeof(void *);
    if (block_size < sizeof(eni_mempool_block_t))
        block_size = sizeof(eni_mempool_block_t);
    block_size = (block_size + align - 1) & ~(align - 1);

    memset(pool, 0, sizeof(*pool));
    pool->base = (uint8_t *)buffer;
    pool->buffer_size = buffer_size;
    pool->block_size = block_size;
    pool->num_blocks = buffer_size / block_size;

    if (pool->num_blocks == 0) return ENI_ERR_INVALID;

    /* Build free list */
    pool->free_list = NULL;
    for (size_t i = 0; i < pool->num_blocks; i++) {
        eni_mempool_block_t *block = (eni_mempool_block_t *)(pool->base + i * block_size);
        block->next = pool->free_list;
        pool->free_list = block;
    }

    eni_status_t rc = eni_mutex_init(&pool->lock);
    if (rc != ENI_OK) return rc;

    pool->initialized = true;
    return ENI_OK;
}

void *eni_mempool_alloc(eni_mempool_t *pool)
{
    if (!pool || !pool->initialized) return NULL;

    eni_mutex_lock(&pool->lock);

    if (!pool->free_list) {
        eni_mutex_unlock(&pool->lock);
        return NULL;
    }

    eni_mempool_block_t *block = pool->free_list;
    pool->free_list = block->next;
    pool->allocated_count++;

    eni_mutex_unlock(&pool->lock);
    return (void *)block;
}

void eni_mempool_free(eni_mempool_t *pool, void *ptr)
{
    if (!pool || !pool->initialized || !ptr) return;

    /* Validate pointer is within buffer */
    uint8_t *p = (uint8_t *)ptr;
    if (p < pool->base || p >= pool->base + pool->buffer_size) return;

    eni_mutex_lock(&pool->lock);

    eni_mempool_block_t *block = (eni_mempool_block_t *)ptr;
    block->next = pool->free_list;
    pool->free_list = block;
    pool->allocated_count--;

    eni_mutex_unlock(&pool->lock);
}

void eni_mempool_reset(eni_mempool_t *pool)
{
    if (!pool || !pool->initialized) return;

    eni_mutex_lock(&pool->lock);

    pool->free_list = NULL;
    pool->allocated_count = 0;

    for (size_t i = 0; i < pool->num_blocks; i++) {
        eni_mempool_block_t *block = (eni_mempool_block_t *)(pool->base + i * pool->block_size);
        block->next = pool->free_list;
        pool->free_list = block;
    }

    eni_mutex_unlock(&pool->lock);
}

void eni_mempool_stats(const eni_mempool_t *pool, eni_mempool_stats_t *stats)
{
    if (!pool || !pool->initialized || !stats) return;
    stats->block_size = pool->block_size;
    stats->num_blocks = pool->num_blocks;
    stats->allocated = pool->allocated_count;
    stats->available = pool->num_blocks - pool->allocated_count;
}

void eni_mempool_destroy(eni_mempool_t *pool)
{
    if (!pool || !pool->initialized) return;
    eni_mutex_destroy(&pool->lock);
    pool->initialized = false;
}
