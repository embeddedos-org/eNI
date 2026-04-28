// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Thread-safe recording engine

#ifndef ENI_RECORDER_H
#define ENI_RECORDER_H

#include "eni/data_format.h"
#include "eni_platform/platform.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ENI_RECORDER_RING_SIZE 65536

typedef enum {
    ENI_RECORDER_IDLE,
    ENI_RECORDER_RECORDING,
    ENI_RECORDER_PAUSED,
    ENI_RECORDER_STOPPED,
} eni_recorder_state_t;

typedef struct {
    eni_recorder_state_t state;
    eni_data_format_t    format;
    eni_data_header_t    header;
    char                 path[256];

    /* Ring buffer for incoming samples */
    double               ring[ENI_RECORDER_RING_SIZE];
    int                  ring_head;
    int                  ring_tail;
    int                  ring_count;

    /* Background flush thread */
    eni_thread_t         flush_thread;
    eni_mutex_t          lock;
    eni_condvar_t        data_ready;
    bool                 stop_requested;

    /* File handle (opaque — format-specific) */
    void                *file_handle;

    /* Stats */
    uint64_t             samples_written;
    uint64_t             records_written;
} eni_recorder_t;

eni_status_t eni_recorder_init(eni_recorder_t *rec, eni_data_format_t format,
                                const eni_data_header_t *header);
eni_status_t eni_recorder_start(eni_recorder_t *rec, const char *path);
eni_status_t eni_recorder_push_samples(eni_recorder_t *rec, const double *samples, int count);
eni_status_t eni_recorder_pause(eni_recorder_t *rec);
eni_status_t eni_recorder_resume(eni_recorder_t *rec);
eni_status_t eni_recorder_stop(eni_recorder_t *rec);
void         eni_recorder_destroy(eni_recorder_t *rec);

#ifdef __cplusplus
}
#endif

#endif /* ENI_RECORDER_H */
