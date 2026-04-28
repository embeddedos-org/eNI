// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023
// Generic POSIX fallback — used on BSDs, Solaris, etc.

#include "eni_platform/platform.h"

#if !defined(_WIN32) && !defined(__APPLE__) && !defined(__linux__) && \
    (defined(__unix__) || defined(_POSIX_VERSION))

#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>
#include <errno.h>

eni_status_t eni_platform_init(void) { return ENI_OK; }

eni_platform_info_t eni_platform_info(void)
{
    eni_platform_info_t info = {
        .os_name = "posix",
        .arch    = "unknown",
        .realtime_capable = false,
        .hardware_access  = false,
    };
    return info;
}

void eni_platform_sleep_ms(uint32_t ms)
{
    struct timespec ts = { .tv_sec = ms / 1000, .tv_nsec = (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

uint64_t eni_platform_monotonic_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/* Threading — same as Linux POSIX implementation */
eni_status_t eni_mutex_init(eni_mutex_t *mtx) {
    if (!mtx) return ENI_ERR_INVALID;
    if (pthread_mutex_init(&mtx->mtx, NULL) != 0) return ENI_ERR_RUNTIME;
    mtx->initialized = true; return ENI_OK;
}
eni_status_t eni_mutex_lock(eni_mutex_t *mtx) {
    if (!mtx || !mtx->initialized) return ENI_ERR_INVALID;
    return (pthread_mutex_lock(&mtx->mtx) == 0) ? ENI_OK : ENI_ERR_RUNTIME;
}
eni_status_t eni_mutex_trylock(eni_mutex_t *mtx) {
    if (!mtx || !mtx->initialized) return ENI_ERR_INVALID;
    int rc = pthread_mutex_trylock(&mtx->mtx);
    if (rc == EBUSY) return ENI_ERR_TIMEOUT;
    return (rc == 0) ? ENI_OK : ENI_ERR_RUNTIME;
}
eni_status_t eni_mutex_unlock(eni_mutex_t *mtx) {
    if (!mtx || !mtx->initialized) return ENI_ERR_INVALID;
    return (pthread_mutex_unlock(&mtx->mtx) == 0) ? ENI_OK : ENI_ERR_RUNTIME;
}
void eni_mutex_destroy(eni_mutex_t *mtx) {
    if (!mtx || !mtx->initialized) return;
    pthread_mutex_destroy(&mtx->mtx); mtx->initialized = false;
}

eni_status_t eni_condvar_init(eni_condvar_t *cv) {
    if (!cv) return ENI_ERR_INVALID;
    if (pthread_cond_init(&cv->cv, NULL) != 0) return ENI_ERR_RUNTIME;
    cv->initialized = true; return ENI_OK;
}
eni_status_t eni_condvar_wait(eni_condvar_t *cv, eni_mutex_t *mtx) {
    if (!cv || !cv->initialized || !mtx || !mtx->initialized) return ENI_ERR_INVALID;
    return (pthread_cond_wait(&cv->cv, &mtx->mtx) == 0) ? ENI_OK : ENI_ERR_RUNTIME;
}
eni_status_t eni_condvar_timedwait(eni_condvar_t *cv, eni_mutex_t *mtx, uint32_t timeout_ms) {
    if (!cv || !cv->initialized || !mtx || !mtx->initialized) return ENI_ERR_INVALID;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000L;
    if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }
    int rc = pthread_cond_timedwait(&cv->cv, &mtx->mtx, &ts);
    if (rc == ETIMEDOUT) return ENI_ERR_TIMEOUT;
    return (rc == 0) ? ENI_OK : ENI_ERR_RUNTIME;
}
eni_status_t eni_condvar_signal(eni_condvar_t *cv) {
    if (!cv || !cv->initialized) return ENI_ERR_INVALID;
    return (pthread_cond_signal(&cv->cv) == 0) ? ENI_OK : ENI_ERR_RUNTIME;
}
eni_status_t eni_condvar_broadcast(eni_condvar_t *cv) {
    if (!cv || !cv->initialized) return ENI_ERR_INVALID;
    return (pthread_cond_broadcast(&cv->cv) == 0) ? ENI_OK : ENI_ERR_RUNTIME;
}
void eni_condvar_destroy(eni_condvar_t *cv) {
    if (!cv || !cv->initialized) return;
    pthread_cond_destroy(&cv->cv); cv->initialized = false;
}

eni_status_t eni_thread_create(eni_thread_t *thr, eni_thread_func_t func, void *arg) {
    if (!thr || !func) return ENI_ERR_INVALID;
    thr->joined = false;
    return (pthread_create(&thr->thr, NULL, func, arg) == 0) ? ENI_OK : ENI_ERR_RUNTIME;
}
eni_status_t eni_thread_join(eni_thread_t *thr, void **retval) {
    if (!thr || thr->joined) return ENI_ERR_INVALID;
    int rc = pthread_join(thr->thr, retval);
    if (rc == 0) thr->joined = true;
    return (rc == 0) ? ENI_OK : ENI_ERR_RUNTIME;
}
eni_status_t eni_thread_detach(eni_thread_t *thr) {
    if (!thr) return ENI_ERR_INVALID;
    return (pthread_detach(thr->thr) == 0) ? ENI_OK : ENI_ERR_RUNTIME;
}

void eni_atomic_init(eni_atomic_int_t *a, int val) { if (a) atomic_init((_Atomic int *)&a->value, val); }
int  eni_atomic_load(const eni_atomic_int_t *a) { return a ? atomic_load((_Atomic int *)&a->value) : 0; }
void eni_atomic_store(eni_atomic_int_t *a, int val) { if (a) atomic_store((_Atomic int *)&a->value, val); }
int  eni_atomic_fetch_add(eni_atomic_int_t *a, int val) { return a ? atomic_fetch_add((_Atomic int *)&a->value, val) : 0; }
int  eni_atomic_fetch_sub(eni_atomic_int_t *a, int val) { return a ? atomic_fetch_sub((_Atomic int *)&a->value, val) : 0; }
int  eni_atomic_compare_exchange(eni_atomic_int_t *a, int expected, int desired) {
    if (!a) return 0;
    return atomic_compare_exchange_strong((_Atomic int *)&a->value, &expected, desired);
}

#endif /* POSIX fallback */
