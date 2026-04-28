// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

#include "eni/session.h"
#include "eni/log.h"
#include <string.h>

static uint32_t s_session_counter = 0;

static void transition(eni_session_t *session, eni_session_state_t new_state)
{
    eni_session_state_t old = session->state;
    session->state = new_state;
    if (session->state_callback)
        session->state_callback(old, new_state, session->callback_data);
    ENI_LOG_INFO("session", "state: %d -> %d (session %u)", old, new_state, session->id);
}

eni_status_t eni_session_init(eni_session_t *session)
{
    if (!session) return ENI_ERR_INVALID;
    memset(session, 0, sizeof(*session));
    session->id = ++s_session_counter;
    session->state = ENI_SESSION_IDLE;

    eni_status_t rc = eni_mutex_init(&session->lock);
    if (rc != ENI_OK) return rc;

    session->initialized = true;
    return ENI_OK;
}

eni_status_t eni_session_set_subject(eni_session_t *session, const char *subject_id)
{
    if (!session || !session->initialized || !subject_id) return ENI_ERR_INVALID;
    eni_mutex_lock(&session->lock);
    strncpy(session->subject_id, subject_id, sizeof(session->subject_id) - 1);
    eni_mutex_unlock(&session->lock);
    return ENI_OK;
}

eni_status_t eni_session_set_description(eni_session_t *session, const char *desc)
{
    if (!session || !session->initialized || !desc) return ENI_ERR_INVALID;
    eni_mutex_lock(&session->lock);
    strncpy(session->description, desc, sizeof(session->description) - 1);
    eni_mutex_unlock(&session->lock);
    return ENI_OK;
}

eni_status_t eni_session_set_meta(eni_session_t *session, const char *key, const char *value)
{
    if (!session || !session->initialized || !key || !value) return ENI_ERR_INVALID;

    eni_mutex_lock(&session->lock);

    /* Update existing */
    for (int i = 0; i < session->meta_count; i++) {
        if (strcmp(session->metadata[i].key, key) == 0) {
            strncpy(session->metadata[i].value, value, sizeof(session->metadata[i].value) - 1);
            eni_mutex_unlock(&session->lock);
            return ENI_OK;
        }
    }

    if (session->meta_count >= ENI_SESSION_MAX_META) {
        eni_mutex_unlock(&session->lock);
        return ENI_ERR_OVERFLOW;
    }

    strncpy(session->metadata[session->meta_count].key, key, sizeof(session->metadata[0].key) - 1);
    strncpy(session->metadata[session->meta_count].value, value, sizeof(session->metadata[0].value) - 1);
    session->meta_count++;

    eni_mutex_unlock(&session->lock);
    return ENI_OK;
}

const char *eni_session_get_meta(const eni_session_t *session, const char *key)
{
    if (!session || !key) return NULL;
    for (int i = 0; i < session->meta_count; i++) {
        if (strcmp(session->metadata[i].key, key) == 0)
            return session->metadata[i].value;
    }
    return NULL;
}

eni_status_t eni_session_set_callback(eni_session_t *session, eni_session_state_cb_t cb, void *data)
{
    if (!session || !session->initialized) return ENI_ERR_INVALID;
    session->state_callback = cb;
    session->callback_data = data;
    return ENI_OK;
}

eni_status_t eni_session_start_calibration(eni_session_t *session)
{
    if (!session || !session->initialized) return ENI_ERR_INVALID;
    eni_mutex_lock(&session->lock);
    if (session->state != ENI_SESSION_IDLE) {
        eni_mutex_unlock(&session->lock);
        return ENI_ERR_RUNTIME;
    }
    session->start_time_ms = eni_platform_monotonic_ms();
    transition(session, ENI_SESSION_CALIBRATING);
    eni_mutex_unlock(&session->lock);
    return ENI_OK;
}

eni_status_t eni_session_start(eni_session_t *session)
{
    if (!session || !session->initialized) return ENI_ERR_INVALID;
    eni_mutex_lock(&session->lock);
    if (session->state != ENI_SESSION_IDLE && session->state != ENI_SESSION_CALIBRATING) {
        eni_mutex_unlock(&session->lock);
        return ENI_ERR_RUNTIME;
    }
    if (session->state == ENI_SESSION_IDLE)
        session->start_time_ms = eni_platform_monotonic_ms();
    transition(session, ENI_SESSION_RUNNING);
    eni_mutex_unlock(&session->lock);
    return ENI_OK;
}

eni_status_t eni_session_pause(eni_session_t *session)
{
    if (!session || !session->initialized) return ENI_ERR_INVALID;
    eni_mutex_lock(&session->lock);
    if (session->state != ENI_SESSION_RUNNING) {
        eni_mutex_unlock(&session->lock);
        return ENI_ERR_RUNTIME;
    }
    session->elapsed_ms += eni_platform_monotonic_ms() - session->start_time_ms;
    transition(session, ENI_SESSION_PAUSED);
    eni_mutex_unlock(&session->lock);
    return ENI_OK;
}

eni_status_t eni_session_resume(eni_session_t *session)
{
    if (!session || !session->initialized) return ENI_ERR_INVALID;
    eni_mutex_lock(&session->lock);
    if (session->state != ENI_SESSION_PAUSED) {
        eni_mutex_unlock(&session->lock);
        return ENI_ERR_RUNTIME;
    }
    session->start_time_ms = eni_platform_monotonic_ms();
    transition(session, ENI_SESSION_RUNNING);
    eni_mutex_unlock(&session->lock);
    return ENI_OK;
}

eni_status_t eni_session_stop(eni_session_t *session)
{
    if (!session || !session->initialized) return ENI_ERR_INVALID;
    eni_mutex_lock(&session->lock);
    if (session->state == ENI_SESSION_RUNNING)
        session->elapsed_ms += eni_platform_monotonic_ms() - session->start_time_ms;
    transition(session, ENI_SESSION_STOPPED);
    eni_mutex_unlock(&session->lock);
    return ENI_OK;
}

eni_session_state_t eni_session_get_state(const eni_session_t *session)
{
    return session ? session->state : ENI_SESSION_IDLE;
}

uint64_t eni_session_elapsed_ms(const eni_session_t *session)
{
    if (!session) return 0;
    uint64_t total = session->elapsed_ms;
    if (session->state == ENI_SESSION_RUNNING)
        total += eni_platform_monotonic_ms() - session->start_time_ms;
    return total;
}

void eni_session_destroy(eni_session_t *session)
{
    if (!session || !session->initialized) return;
    eni_mutex_destroy(&session->lock);
    session->initialized = false;
}
