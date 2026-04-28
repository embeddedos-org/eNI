// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Session management — state machine for BCI sessions

#ifndef ENI_SESSION_H
#define ENI_SESSION_H

#include "eni/types.h"
#include "eni_platform/platform.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ENI_SESSION_MAX_META 32

typedef enum {
    ENI_SESSION_IDLE,
    ENI_SESSION_CALIBRATING,
    ENI_SESSION_RUNNING,
    ENI_SESSION_PAUSED,
    ENI_SESSION_STOPPED,
    ENI_SESSION_ERROR,
} eni_session_state_t;

typedef void (*eni_session_state_cb_t)(eni_session_state_t old_state,
                                        eni_session_state_t new_state,
                                        void *user_data);

typedef struct {
    char key[64];
    char value[256];
} eni_session_meta_t;

typedef struct {
    uint32_t            id;
    eni_session_state_t state;
    uint64_t            start_time_ms;
    uint64_t            elapsed_ms;
    char                subject_id[64];
    char                description[256];

    eni_session_meta_t  metadata[ENI_SESSION_MAX_META];
    int                 meta_count;

    bool                auto_record;
    char                record_path[256];

    eni_session_state_cb_t state_callback;
    void                  *callback_data;

    eni_mutex_t         lock;
    bool                initialized;
} eni_session_t;

eni_status_t eni_session_init(eni_session_t *session);
eni_status_t eni_session_set_subject(eni_session_t *session, const char *subject_id);
eni_status_t eni_session_set_description(eni_session_t *session, const char *desc);
eni_status_t eni_session_set_meta(eni_session_t *session, const char *key, const char *value);
const char  *eni_session_get_meta(const eni_session_t *session, const char *key);
eni_status_t eni_session_set_callback(eni_session_t *session, eni_session_state_cb_t cb, void *data);
eni_status_t eni_session_start_calibration(eni_session_t *session);
eni_status_t eni_session_start(eni_session_t *session);
eni_status_t eni_session_pause(eni_session_t *session);
eni_status_t eni_session_resume(eni_session_t *session);
eni_status_t eni_session_stop(eni_session_t *session);
eni_session_state_t eni_session_get_state(const eni_session_t *session);
uint64_t     eni_session_elapsed_ms(const eni_session_t *session);
void         eni_session_destroy(eni_session_t *session);

#ifdef __cplusplus
}
#endif

#endif /* ENI_SESSION_H */
