// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef ENI_PLATFORM_H
#define ENI_PLATFORM_H

#include "eni/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Platform Info ─────────────────────────────────────────────────── */

typedef struct {
    const char *os_name;
    const char *arch;
    bool        realtime_capable;
    bool        hardware_access;
} eni_platform_info_t;

eni_status_t        eni_platform_init(void);
eni_platform_info_t eni_platform_info(void);
void                eni_platform_sleep_ms(uint32_t ms);
uint64_t            eni_platform_monotonic_ms(void);

/* ── Threading Primitives ──────────────────────────────────────────── */

typedef struct eni_mutex_s      eni_mutex_t;
typedef struct eni_condvar_s    eni_condvar_t;
typedef struct eni_thread_s     eni_thread_t;

typedef void *(*eni_thread_func_t)(void *arg);

/* Mutex */
eni_status_t eni_mutex_init(eni_mutex_t *mtx);
eni_status_t eni_mutex_lock(eni_mutex_t *mtx);
eni_status_t eni_mutex_trylock(eni_mutex_t *mtx);
eni_status_t eni_mutex_unlock(eni_mutex_t *mtx);
void         eni_mutex_destroy(eni_mutex_t *mtx);

/* Condition Variable */
eni_status_t eni_condvar_init(eni_condvar_t *cv);
eni_status_t eni_condvar_wait(eni_condvar_t *cv, eni_mutex_t *mtx);
eni_status_t eni_condvar_timedwait(eni_condvar_t *cv, eni_mutex_t *mtx, uint32_t timeout_ms);
eni_status_t eni_condvar_signal(eni_condvar_t *cv);
eni_status_t eni_condvar_broadcast(eni_condvar_t *cv);
void         eni_condvar_destroy(eni_condvar_t *cv);

/* Thread */
eni_status_t eni_thread_create(eni_thread_t *thr, eni_thread_func_t func, void *arg);
eni_status_t eni_thread_join(eni_thread_t *thr, void **retval);
eni_status_t eni_thread_detach(eni_thread_t *thr);

/* Atomics */
typedef struct {
    volatile int value;
} eni_atomic_int_t;

void eni_atomic_init(eni_atomic_int_t *a, int val);
int  eni_atomic_load(const eni_atomic_int_t *a);
void eni_atomic_store(eni_atomic_int_t *a, int val);
int  eni_atomic_fetch_add(eni_atomic_int_t *a, int val);
int  eni_atomic_fetch_sub(eni_atomic_int_t *a, int val);
int  eni_atomic_compare_exchange(eni_atomic_int_t *a, int expected, int desired);

/* ── Opaque Platform Structs ───────────────────────────────────────── */

#if defined(_WIN32)
  #include <windows.h>
  struct eni_mutex_s   { CRITICAL_SECTION cs; bool initialized; };
  struct eni_condvar_s { CONDITION_VARIABLE cv; bool initialized; };
  struct eni_thread_s  { HANDLE handle; eni_thread_func_t func; void *arg; };
#elif defined(__APPLE__) || defined(__linux__) || defined(__unix__) || defined(_POSIX_VERSION)
  #include <pthread.h>
  struct eni_mutex_s   { pthread_mutex_t mtx; bool initialized; };
  struct eni_condvar_s { pthread_cond_t  cv;  bool initialized; };
  struct eni_thread_s  { pthread_t       thr; bool joined; };
#else
  /* Stub for bare-metal / EoS — single-threaded fallback */
  struct eni_mutex_s   { bool locked; bool initialized; };
  struct eni_condvar_s { bool initialized; };
  struct eni_thread_s  { eni_thread_func_t func; void *arg; void *retval; };
#endif

#ifdef __cplusplus
}
#endif

#endif /* ENI_PLATFORM_H */
