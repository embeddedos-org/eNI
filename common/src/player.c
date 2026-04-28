// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

#include "eni/player.h"
#include "eni/edf.h"
#include "eni/log.h"
#include <string.h>
#include <stdlib.h>

static void *playback_thread_func(void *arg)
{
    eni_player_t *p = (eni_player_t *)arg;

    while (1) {
        eni_mutex_lock(&p->lock);
        if (p->stop_requested || p->state == ENI_PLAYER_STOPPED) {
            eni_mutex_unlock(&p->lock);
            break;
        }
        if (p->state == ENI_PLAYER_PAUSED) {
            eni_mutex_unlock(&p->lock);
            eni_platform_sleep_ms(10);
            continue;
        }

        int64_t rec = p->current_record;
        int64_t total = p->header.num_records;
        eni_mutex_unlock(&p->lock);

        if (rec >= total) {
            if (p->loop) {
                eni_mutex_lock(&p->lock);
                p->current_record = 0;
                eni_mutex_unlock(&p->lock);
                continue;
            }
            break;
        }

        /* Calculate samples per record */
        int samples_per_record = 0;
        for (int ch = 0; ch < p->header.num_channels; ch++)
            samples_per_record += p->header.channels[ch].samples_per_record;

        double *buf = (double *)malloc(sizeof(double) * (size_t)samples_per_record);
        if (!buf) break;

        eni_status_t rc = ENI_ERR_UNSUPPORTED;
        if (p->format == ENI_FORMAT_EDF && p->file_handle) {
            rc = eni_edf_read_record((eni_edf_file_t *)p->file_handle, rec, buf, samples_per_record);
        }

        if (rc == ENI_OK && p->callback) {
            p->callback(buf, samples_per_record, p->user_data);
        }

        free(buf);

        eni_mutex_lock(&p->lock);
        p->current_record++;
        eni_mutex_unlock(&p->lock);

        /* Simulate real-time playback */
        if (p->header.record_duration > 0.0 && p->speed > 0.0f) {
            uint32_t delay_ms = (uint32_t)(p->header.record_duration * 1000.0 / p->speed);
            eni_platform_sleep_ms(delay_ms);
        }
    }

    eni_mutex_lock(&p->lock);
    p->state = ENI_PLAYER_STOPPED;
    eni_mutex_unlock(&p->lock);
    return NULL;
}

eni_status_t eni_player_open(eni_player_t *player, const char *path)
{
    if (!player || !path) return ENI_ERR_INVALID;
    memset(player, 0, sizeof(*player));

    player->speed = 1.0f;
    player->state = ENI_PLAYER_IDLE;

    /* Auto-detect format */
    player->format = eni_data_format_detect_file(path);

    eni_status_t rc;
    if (player->format == ENI_FORMAT_EDF) {
        eni_edf_file_t *edf = (eni_edf_file_t *)calloc(1, sizeof(eni_edf_file_t));
        if (!edf) return ENI_ERR_NOMEM;
        rc = eni_edf_open(edf, path);
        if (rc != ENI_OK) { free(edf); return rc; }
        player->file_handle = edf;
        player->header = edf->header;
    } else {
        return ENI_ERR_UNSUPPORTED;
    }

    rc = eni_mutex_init(&player->lock);
    if (rc != ENI_OK) return rc;

    return ENI_OK;
}

eni_status_t eni_player_set_callback(eni_player_t *player, eni_player_callback_t cb, void *user_data)
{
    if (!player) return ENI_ERR_INVALID;
    player->callback = cb;
    player->user_data = user_data;
    return ENI_OK;
}

eni_status_t eni_player_play(eni_player_t *player)
{
    if (!player) return ENI_ERR_INVALID;
    if (player->state == ENI_PLAYER_PLAYING) return ENI_OK;

    /* Resume from pause — thread is still running, just change state */
    if (player->state == ENI_PLAYER_PAUSED) {
        eni_mutex_lock(&player->lock);
        player->state = ENI_PLAYER_PLAYING;
        eni_mutex_unlock(&player->lock);
        return ENI_OK;
    }

    /* Fresh start — create playback thread */
    player->state = ENI_PLAYER_PLAYING;
    player->stop_requested = false;

    return eni_thread_create(&player->playback_thread, playback_thread_func, player);
}

eni_status_t eni_player_pause(eni_player_t *player)
{
    if (!player) return ENI_ERR_INVALID;
    eni_mutex_lock(&player->lock);
    player->state = ENI_PLAYER_PAUSED;
    eni_mutex_unlock(&player->lock);
    return ENI_OK;
}

eni_status_t eni_player_stop(eni_player_t *player)
{
    if (!player) return ENI_ERR_INVALID;
    eni_mutex_lock(&player->lock);
    player->stop_requested = true;
    eni_mutex_unlock(&player->lock);
    eni_thread_join(&player->playback_thread, NULL);
    player->state = ENI_PLAYER_STOPPED;
    return ENI_OK;
}

eni_status_t eni_player_seek(eni_player_t *player, int64_t record_idx)
{
    if (!player) return ENI_ERR_INVALID;
    eni_mutex_lock(&player->lock);
    if (record_idx >= 0 && record_idx < player->header.num_records)
        player->current_record = record_idx;
    eni_mutex_unlock(&player->lock);
    return ENI_OK;
}

void eni_player_set_speed(eni_player_t *player, float speed)
{
    if (player && speed > 0.0f) player->speed = speed;
}

void eni_player_set_loop(eni_player_t *player, bool loop)
{
    if (player) player->loop = loop;
}

void eni_player_close(eni_player_t *player)
{
    if (!player) return;
    if (player->state == ENI_PLAYER_PLAYING) eni_player_stop(player);
    if (player->format == ENI_FORMAT_EDF && player->file_handle) {
        eni_edf_close((eni_edf_file_t *)player->file_handle);
        free(player->file_handle);
    }
    eni_mutex_destroy(&player->lock);
}

const eni_data_header_t *eni_player_get_header(const eni_player_t *player)
{
    return player ? &player->header : NULL;
}
