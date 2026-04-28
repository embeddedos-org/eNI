// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Data playback/replay engine

#ifndef ENI_PLAYER_H
#define ENI_PLAYER_H

#include "eni/data_format.h"
#include "eni_platform/platform.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ENI_PLAYER_IDLE,
    ENI_PLAYER_PLAYING,
    ENI_PLAYER_PAUSED,
    ENI_PLAYER_STOPPED,
} eni_player_state_t;

typedef void (*eni_player_callback_t)(const double *samples, int count, void *user_data);

typedef struct {
    eni_player_state_t  state;
    eni_data_format_t   format;
    eni_data_header_t   header;
    void               *file_handle;
    int64_t             current_record;
    float               speed;         /* 1.0 = real-time, 2.0 = 2x speed */
    bool                loop;

    eni_player_callback_t callback;
    void                 *user_data;

    eni_thread_t        playback_thread;
    eni_mutex_t         lock;
    bool                stop_requested;
} eni_player_t;

eni_status_t eni_player_open(eni_player_t *player, const char *path);
eni_status_t eni_player_set_callback(eni_player_t *player, eni_player_callback_t cb, void *user_data);
eni_status_t eni_player_play(eni_player_t *player);
eni_status_t eni_player_pause(eni_player_t *player);
eni_status_t eni_player_stop(eni_player_t *player);
eni_status_t eni_player_seek(eni_player_t *player, int64_t record_idx);
void         eni_player_set_speed(eni_player_t *player, float speed);
void         eni_player_set_loop(eni_player_t *player, bool loop);
void         eni_player_close(eni_player_t *player);

const eni_data_header_t *eni_player_get_header(const eni_player_t *player);

#ifdef __cplusplus
}
#endif

#endif /* ENI_PLAYER_H */
