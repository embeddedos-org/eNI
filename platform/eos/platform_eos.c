// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eni_platform/platform.h"

#ifdef ENI_PLATFORM_EOS_ENABLED

eni_status_t eni_platform_init(void)
{
    /* EoS: initialize system services, GPIO, real-time scheduler */
    return ENI_OK;
}

eni_platform_info_t eni_platform_info(void)
{
    eni_platform_info_t info = {
        .os_name          = "eos",
#if defined(__aarch64__)
        .arch             = "arm64",
#elif defined(__x86_64__)
        .arch             = "x86_64",
#elif defined(__riscv)
        .arch             = "riscv64",
#else
        .arch             = "unknown",
#endif
        .realtime_capable = true,
        .hardware_access  = true,
    };
    return info;
}

void eni_platform_sleep_ms(uint32_t ms)
{
    /* EoS: real-time sleep via system scheduler */
    (void)ms;
}

uint64_t eni_platform_monotonic_ms(void)
{
    /* EoS: high-precision monotonic clock */
    return 0;
}

/* ── Mutex (single-threaded no-op) ─────────────────────────────────── */

eni_status_t eni_mutex_init(eni_mutex_t *mtx)
{
    if (!mtx) return ENI_ERR_INVALID;
    mtx->initialized = true;
    return ENI_OK;
}

eni_status_t eni_mutex_lock(eni_mutex_t *mtx)    { (void)mtx; return ENI_OK; }
eni_status_t eni_mutex_trylock(eni_mutex_t *mtx)  { (void)mtx; return ENI_OK; }
eni_status_t eni_mutex_unlock(eni_mutex_t *mtx)   { (void)mtx; return ENI_OK; }

void eni_mutex_destroy(eni_mutex_t *mtx)
{
    if (mtx) mtx->initialized = false;
}

/* ── Condition Variable (single-threaded no-op) ────────────────────── */

eni_status_t eni_condvar_init(eni_condvar_t *cv)
{
    if (!cv) return ENI_ERR_INVALID;
    cv->initialized = true;
    return ENI_OK;
}

eni_status_t eni_condvar_wait(eni_condvar_t *cv, eni_mutex_t *mtx)
{
    (void)cv; (void)mtx;
    return ENI_OK;
}

eni_status_t eni_condvar_timedwait(eni_condvar_t *cv, eni_mutex_t *mtx, uint32_t timeout_ms)
{
    (void)cv; (void)mtx; (void)timeout_ms;
    return ENI_ERR_TIMEOUT;
}

eni_status_t eni_condvar_signal(eni_condvar_t *cv)    { (void)cv; return ENI_OK; }
eni_status_t eni_condvar_broadcast(eni_condvar_t *cv)  { (void)cv; return ENI_OK; }

void eni_condvar_destroy(eni_condvar_t *cv)
{
    if (cv) cv->initialized = false;
}

/* ── Thread (synchronous fallback) ─────────────────────────────────── */

eni_status_t eni_thread_create(eni_thread_t *thr, eni_thread_func_t func, void *arg)
{
    if (!thr || !func) return ENI_ERR_INVALID;
    thr->retval = func(arg);
    return ENI_OK;
}

eni_status_t eni_thread_join(eni_thread_t *thr, void **retval)
{
    if (!thr) return ENI_ERR_INVALID;
    if (retval) *retval = thr->retval;
    return ENI_OK;
}

eni_status_t eni_thread_detach(eni_thread_t *thr) { (void)thr; return ENI_OK; }

/* ── Atomics (volatile reads/writes) ───────────────────────────────── */

void eni_atomic_init(eni_atomic_int_t *a, int val)
{
    if (a) a->value = val;
}

int eni_atomic_load(const eni_atomic_int_t *a)
{
    return a ? a->value : 0;
}

void eni_atomic_store(eni_atomic_int_t *a, int val)
{
    if (a) a->value = val;
}

int eni_atomic_fetch_add(eni_atomic_int_t *a, int val)
{
    if (!a) return 0;
    int old = a->value;
    a->value += val;
    return old;
}

int eni_atomic_fetch_sub(eni_atomic_int_t *a, int val)
{
    if (!a) return 0;
    int old = a->value;
    a->value -= val;
    return old;
}

int eni_atomic_compare_exchange(eni_atomic_int_t *a, int expected, int desired)
{
    if (!a) return 0;
    if (a->value == expected) {
        a->value = desired;
        return 1;
    }
    return 0;
}

#endif /* ENI_PLATFORM_EOS_ENABLED */
