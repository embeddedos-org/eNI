// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eni_platform/platform.h"

#ifdef _WIN32

#include <windows.h>

eni_status_t eni_platform_init(void)
{
    return ENI_OK;
}

eni_platform_info_t eni_platform_info(void)
{
    eni_platform_info_t info = {
        .os_name          = "windows",
#if defined(_M_X64)
        .arch             = "x86_64",
#elif defined(_M_ARM64)
        .arch             = "arm64",
#else
        .arch             = "unknown",
#endif
        .realtime_capable = false,
        .hardware_access  = false,
    };
    return info;
}

void eni_platform_sleep_ms(uint32_t ms)
{
    Sleep(ms);
}

uint64_t eni_platform_monotonic_ms(void)
{
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (uint64_t)(count.QuadPart * 1000 / freq.QuadPart);
}

/* ── Mutex ─────────────────────────────────────────────────────────── */

eni_status_t eni_mutex_init(eni_mutex_t *mtx)
{
    if (!mtx) return ENI_ERR_INVALID;
    InitializeCriticalSection(&mtx->cs);
    mtx->initialized = true;
    return ENI_OK;
}

eni_status_t eni_mutex_lock(eni_mutex_t *mtx)
{
    if (!mtx || !mtx->initialized) return ENI_ERR_INVALID;
    EnterCriticalSection(&mtx->cs);
    return ENI_OK;
}

eni_status_t eni_mutex_trylock(eni_mutex_t *mtx)
{
    if (!mtx || !mtx->initialized) return ENI_ERR_INVALID;
    return TryEnterCriticalSection(&mtx->cs) ? ENI_OK : ENI_ERR_TIMEOUT;
}

eni_status_t eni_mutex_unlock(eni_mutex_t *mtx)
{
    if (!mtx || !mtx->initialized) return ENI_ERR_INVALID;
    LeaveCriticalSection(&mtx->cs);
    return ENI_OK;
}

void eni_mutex_destroy(eni_mutex_t *mtx)
{
    if (!mtx || !mtx->initialized) return;
    DeleteCriticalSection(&mtx->cs);
    mtx->initialized = false;
}

/* ── Condition Variable ────────────────────────────────────────────── */

eni_status_t eni_condvar_init(eni_condvar_t *cv)
{
    if (!cv) return ENI_ERR_INVALID;
    InitializeConditionVariable(&cv->cv);
    cv->initialized = true;
    return ENI_OK;
}

eni_status_t eni_condvar_wait(eni_condvar_t *cv, eni_mutex_t *mtx)
{
    if (!cv || !cv->initialized || !mtx || !mtx->initialized) return ENI_ERR_INVALID;
    return SleepConditionVariableCS(&cv->cv, &mtx->cs, INFINITE) ? ENI_OK : ENI_ERR_RUNTIME;
}

eni_status_t eni_condvar_timedwait(eni_condvar_t *cv, eni_mutex_t *mtx, uint32_t timeout_ms)
{
    if (!cv || !cv->initialized || !mtx || !mtx->initialized) return ENI_ERR_INVALID;
    if (!SleepConditionVariableCS(&cv->cv, &mtx->cs, timeout_ms)) {
        return (GetLastError() == ERROR_TIMEOUT) ? ENI_ERR_TIMEOUT : ENI_ERR_RUNTIME;
    }
    return ENI_OK;
}

eni_status_t eni_condvar_signal(eni_condvar_t *cv)
{
    if (!cv || !cv->initialized) return ENI_ERR_INVALID;
    WakeConditionVariable(&cv->cv);
    return ENI_OK;
}

eni_status_t eni_condvar_broadcast(eni_condvar_t *cv)
{
    if (!cv || !cv->initialized) return ENI_ERR_INVALID;
    WakeAllConditionVariable(&cv->cv);
    return ENI_OK;
}

void eni_condvar_destroy(eni_condvar_t *cv)
{
    if (!cv) return;
    cv->initialized = false;
    /* Windows CONDITION_VARIABLE doesn't need explicit destruction */
}

/* ── Thread ────────────────────────────────────────────────────────── */

static DWORD WINAPI win_thread_wrapper(LPVOID param)
{
    eni_thread_t *thr = (eni_thread_t *)param;
    thr->func(thr->arg);
    return 0;
}

eni_status_t eni_thread_create(eni_thread_t *thr, eni_thread_func_t func, void *arg)
{
    if (!thr || !func) return ENI_ERR_INVALID;
    thr->func = func;
    thr->arg  = arg;
    thr->handle = CreateThread(NULL, 0, win_thread_wrapper, thr, 0, NULL);
    return thr->handle ? ENI_OK : ENI_ERR_RUNTIME;
}

eni_status_t eni_thread_join(eni_thread_t *thr, void **retval)
{
    if (!thr || !thr->handle) return ENI_ERR_INVALID;
    DWORD rc = WaitForSingleObject(thr->handle, INFINITE);
    if (rc != WAIT_OBJECT_0) return ENI_ERR_RUNTIME;
    CloseHandle(thr->handle);
    thr->handle = NULL;
    if (retval) *retval = NULL;
    return ENI_OK;
}

eni_status_t eni_thread_detach(eni_thread_t *thr)
{
    if (!thr || !thr->handle) return ENI_ERR_INVALID;
    CloseHandle(thr->handle);
    thr->handle = NULL;
    return ENI_OK;
}

/* ── Atomics ───────────────────────────────────────────────────────── */

void eni_atomic_init(eni_atomic_int_t *a, int val)
{
    if (a) InterlockedExchange((volatile LONG *)&a->value, val);
}

int eni_atomic_load(const eni_atomic_int_t *a)
{
    return a ? InterlockedCompareExchange((volatile LONG *)&a->value, 0, 0) : 0;
}

void eni_atomic_store(eni_atomic_int_t *a, int val)
{
    if (a) InterlockedExchange((volatile LONG *)&a->value, val);
}

int eni_atomic_fetch_add(eni_atomic_int_t *a, int val)
{
    return a ? InterlockedExchangeAdd((volatile LONG *)&a->value, val) : 0;
}

int eni_atomic_fetch_sub(eni_atomic_int_t *a, int val)
{
    return a ? InterlockedExchangeAdd((volatile LONG *)&a->value, -val) : 0;
}

int eni_atomic_compare_exchange(eni_atomic_int_t *a, int expected, int desired)
{
    if (!a) return 0;
    LONG old = InterlockedCompareExchange((volatile LONG *)&a->value, desired, expected);
    return (old == expected) ? 1 : 0;
}

#endif /* _WIN32 */
