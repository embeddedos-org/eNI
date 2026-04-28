// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// JNI bridge for com.eos.eni.EniSession -> libeni session API

#include <jni.h>
#include <stdlib.h>
#include <string.h>

#include "eni/session.h"

// ---------------------------------------------------------------------------
// Helper: get the eni_session_t pointer from the Java long handle
// ---------------------------------------------------------------------------

static eni_session_t *get_session(jlong handle) {
    return (eni_session_t *)(uintptr_t)handle;
}

// ---------------------------------------------------------------------------
// JNI method implementations
// ---------------------------------------------------------------------------

JNIEXPORT jlong JNICALL
Java_com_eos_eni_EniSession_nativeInit(JNIEnv *env, jclass cls) {
    (void)env;
    (void)cls;

    eni_session_t *session = (eni_session_t *)calloc(1, sizeof(eni_session_t));
    if (!session) {
        return 0;
    }

    eni_status_t rc = eni_session_init(session);
    if (rc != ENI_OK) {
        free(session);
        return 0;
    }

    return (jlong)(uintptr_t)session;
}

JNIEXPORT jint JNICALL
Java_com_eos_eni_EniSession_nativeStart(JNIEnv *env, jclass cls, jlong handle) {
    (void)env;
    (void)cls;

    eni_session_t *session = get_session(handle);
    if (!session) {
        return (jint)ENI_ERR_INVALID;
    }
    return (jint)eni_session_start(session);
}

JNIEXPORT jint JNICALL
Java_com_eos_eni_EniSession_nativeStop(JNIEnv *env, jclass cls, jlong handle) {
    (void)env;
    (void)cls;

    eni_session_t *session = get_session(handle);
    if (!session) {
        return (jint)ENI_ERR_INVALID;
    }
    return (jint)eni_session_stop(session);
}

JNIEXPORT jint JNICALL
Java_com_eos_eni_EniSession_nativePause(JNIEnv *env, jclass cls, jlong handle) {
    (void)env;
    (void)cls;

    eni_session_t *session = get_session(handle);
    if (!session) {
        return (jint)ENI_ERR_INVALID;
    }
    return (jint)eni_session_pause(session);
}

JNIEXPORT jint JNICALL
Java_com_eos_eni_EniSession_nativeResume(JNIEnv *env, jclass cls, jlong handle) {
    (void)env;
    (void)cls;

    eni_session_t *session = get_session(handle);
    if (!session) {
        return (jint)ENI_ERR_INVALID;
    }
    return (jint)eni_session_resume(session);
}

JNIEXPORT jint JNICALL
Java_com_eos_eni_EniSession_nativeGetState(JNIEnv *env, jclass cls, jlong handle) {
    (void)env;
    (void)cls;

    eni_session_t *session = get_session(handle);
    if (!session) {
        return (jint)ENI_SESSION_ERROR;
    }
    return (jint)eni_session_get_state(session);
}

JNIEXPORT jlong JNICALL
Java_com_eos_eni_EniSession_nativeGetElapsedMs(JNIEnv *env, jclass cls, jlong handle) {
    (void)env;
    (void)cls;

    eni_session_t *session = get_session(handle);
    if (!session) {
        return 0;
    }
    return (jlong)eni_session_elapsed_ms(session);
}

JNIEXPORT jint JNICALL
Java_com_eos_eni_EniSession_nativeSetSubject(JNIEnv *env, jclass cls,
                                              jlong handle, jstring subjectId) {
    (void)cls;

    eni_session_t *session = get_session(handle);
    if (!session) {
        return (jint)ENI_ERR_INVALID;
    }

    const char *c_subject = (*env)->GetStringUTFChars(env, subjectId, NULL);
    if (!c_subject) {
        return (jint)ENI_ERR_NOMEM;
    }

    eni_status_t rc = eni_session_set_subject(session, c_subject);
    (*env)->ReleaseStringUTFChars(env, subjectId, c_subject);

    return (jint)rc;
}

JNIEXPORT void JNICALL
Java_com_eos_eni_EniSession_nativeDestroy(JNIEnv *env, jclass cls, jlong handle) {
    (void)env;
    (void)cls;

    eni_session_t *session = get_session(handle);
    if (session) {
        eni_session_destroy(session);
        free(session);
    }
}
