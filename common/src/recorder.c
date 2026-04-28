// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

#include "eni/recorder.h"
#include "eni/edf.h"
#include "eni/log.h"
#include <string.h>
#include <stdlib.h>

static void *recorder_flush_thread(void *arg)
{
    eni_recorder_t *rec = (eni_recorder_t *)arg;

    while (1) {
        eni_mutex_lock(&rec->lock);

        while (rec->ring_count == 0 && !rec->stop_requested) {
            eni_condvar_timedwait(&rec->data_ready, &rec->lock, 100);
        }

        if (rec->stop_requested && rec->ring_count == 0) {
            eni_mutex_unlock(&rec->lock);
            break;
        }

        /* Calculate how many samples make a complete record */
        int samples_per_record = 0;
        for (int ch = 0; ch < rec->header.num_channels; ch++)
            samples_per_record += rec->header.channels[ch].samples_per_record;

        /* Guard against zero-length records (would infinite loop) */
        if (samples_per_record <= 0) {
            eni_mutex_unlock(&rec->lock);
            break;
        }

        while (rec->ring_count >= samples_per_record) {
            double *buf = (double *)malloc(sizeof(double) * (size_t)samples_per_record);
            if (!buf) break;

            for (int i = 0; i < samples_per_record; i++) {
                buf[i] = rec->ring[rec->ring_head];
                rec->ring_head = (rec->ring_head + 1) % ENI_RECORDER_RING_SIZE;
                rec->ring_count--;
            }

            eni_mutex_unlock(&rec->lock);

            /* Write record based on format */
            if (rec->format == ENI_FORMAT_EDF && rec->file_handle) {
                eni_edf_write_record((eni_edf_file_t *)rec->file_handle, buf, samples_per_record);
                rec->records_written++;
            }
            rec->samples_written += (uint64_t)samples_per_record;

            free(buf);
            eni_mutex_lock(&rec->lock);
        }

        eni_mutex_unlock(&rec->lock);
    }

    return NULL;
}

eni_status_t eni_recorder_init(eni_recorder_t *rec, eni_data_format_t format,
                                const eni_data_header_t *header)
{
    if (!rec || !header) return ENI_ERR_INVALID;
    memset(rec, 0, sizeof(*rec));

    rec->format = format;
    rec->header = *header;
    rec->state = ENI_RECORDER_IDLE;

    eni_status_t rc = eni_mutex_init(&rec->lock);
    if (rc != ENI_OK) return rc;

    rc = eni_condvar_init(&rec->data_ready);
    if (rc != ENI_OK) { eni_mutex_destroy(&rec->lock); return rc; }

    return ENI_OK;
}

eni_status_t eni_recorder_start(eni_recorder_t *rec, const char *path)
{
    if (!rec || !path) return ENI_ERR_INVALID;
    if (rec->state != ENI_RECORDER_IDLE) return ENI_ERR_RUNTIME;

    strncpy(rec->path, path, sizeof(rec->path) - 1);

    /* Open output file */
    if (rec->format == ENI_FORMAT_EDF) {
        eni_edf_file_t *edf = (eni_edf_file_t *)calloc(1, sizeof(eni_edf_file_t));
        if (!edf) return ENI_ERR_NOMEM;
        eni_status_t rc = eni_edf_create(edf, path, &rec->header);
        if (rc != ENI_OK) { free(edf); return rc; }
        rec->file_handle = edf;
    } else {
        return ENI_ERR_UNSUPPORTED;
    }

    rec->stop_requested = false;
    rec->state = ENI_RECORDER_RECORDING;

    eni_status_t rc = eni_thread_create(&rec->flush_thread, recorder_flush_thread, rec);
    if (rc != ENI_OK) {
        rec->state = ENI_RECORDER_IDLE;
        return rc;
    }

    ENI_LOG_INFO("recorder", "started recording to %s", path);
    return ENI_OK;
}

eni_status_t eni_recorder_push_samples(eni_recorder_t *rec, const double *samples, int count)
{
    if (!rec || !samples) return ENI_ERR_INVALID;
    if (rec->state != ENI_RECORDER_RECORDING) return ENI_ERR_RUNTIME;

    eni_mutex_lock(&rec->lock);

    for (int i = 0; i < count; i++) {
        if (rec->ring_count >= ENI_RECORDER_RING_SIZE) {
            eni_mutex_unlock(&rec->lock);
            ENI_LOG_WARN("recorder", "ring buffer full, dropping samples");
            return ENI_ERR_OVERFLOW;
        }
        rec->ring[rec->ring_tail] = samples[i];
        rec->ring_tail = (rec->ring_tail + 1) % ENI_RECORDER_RING_SIZE;
        rec->ring_count++;
    }

    eni_condvar_signal(&rec->data_ready);
    eni_mutex_unlock(&rec->lock);
    return ENI_OK;
}

eni_status_t eni_recorder_pause(eni_recorder_t *rec)
{
    if (!rec || rec->state != ENI_RECORDER_RECORDING) return ENI_ERR_INVALID;
    rec->state = ENI_RECORDER_PAUSED;
    return ENI_OK;
}

eni_status_t eni_recorder_resume(eni_recorder_t *rec)
{
    if (!rec || rec->state != ENI_RECORDER_PAUSED) return ENI_ERR_INVALID;
    rec->state = ENI_RECORDER_RECORDING;
    return ENI_OK;
}

eni_status_t eni_recorder_stop(eni_recorder_t *rec)
{
    if (!rec) return ENI_ERR_INVALID;
    if (rec->state == ENI_RECORDER_IDLE || rec->state == ENI_RECORDER_STOPPED)
        return ENI_OK;

    eni_mutex_lock(&rec->lock);
    rec->stop_requested = true;
    eni_condvar_signal(&rec->data_ready);
    eni_mutex_unlock(&rec->lock);

    eni_thread_join(&rec->flush_thread, NULL);

    /* Finalize file */
    if (rec->format == ENI_FORMAT_EDF && rec->file_handle) {
        eni_edf_finalize((eni_edf_file_t *)rec->file_handle);
        free(rec->file_handle);
        rec->file_handle = NULL;
    }

    rec->state = ENI_RECORDER_STOPPED;
    ENI_LOG_INFO("recorder", "stopped: %llu samples, %llu records written",
                 (unsigned long long)rec->samples_written,
                 (unsigned long long)rec->records_written);
    return ENI_OK;
}

void eni_recorder_destroy(eni_recorder_t *rec)
{
    if (!rec) return;
    if (rec->state == ENI_RECORDER_RECORDING || rec->state == ENI_RECORDER_PAUSED)
        eni_recorder_stop(rec);
    eni_condvar_destroy(&rec->data_ready);
    eni_mutex_destroy(&rec->lock);
}
