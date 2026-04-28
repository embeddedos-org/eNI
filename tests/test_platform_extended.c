// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Test: platform/linux/platform_linux.c — condvar, thread_detach, timedwait

#include <stdio.h>
#include <string.h>
#include "eni/common.h"
#include "eni_platform/platform.h"

static int tests_run = 0, tests_passed = 0, tests_failed = 0;
#define TEST(name) do { tests_run++; printf("  TEST %-40s ", #name); } while(0)
#define PASS() do { tests_passed++; printf("[PASS]\n"); } while(0)
#define FAIL(msg) do { tests_failed++; printf("[FAIL] %s\n", msg); } while(0)

/* ── Shared state for condvar tests ── */
static eni_mutex_t g_mtx;
static eni_condvar_t g_cv;
static volatile int g_ready = 0;

static void *waiter_thread(void *arg)
{
    (void)arg;
    eni_mutex_lock(&g_mtx);
    while (!g_ready) {
        eni_condvar_wait(&g_cv, &g_mtx);
    }
    eni_mutex_unlock(&g_mtx);
    return (void *)(long)42;
}

/* ── Test: condvar wait and signal ── */
static void test_condvar_wait_signal(void)
{
    TEST(condvar_wait_signal);
    g_ready = 0;
    eni_mutex_init(&g_mtx);
    eni_condvar_init(&g_cv);

    eni_thread_t thr;
    eni_thread_create(&thr, waiter_thread, NULL);

    eni_platform_sleep_ms(50);
    eni_mutex_lock(&g_mtx);
    g_ready = 1;
    eni_condvar_signal(&g_cv);
    eni_mutex_unlock(&g_mtx);

    void *retval = NULL;
    eni_thread_join(&thr, &retval);
    if ((long)retval != 42) { FAIL("wrong return value"); }
    else { PASS(); }

    eni_condvar_destroy(&g_cv);
    eni_mutex_destroy(&g_mtx);
}

/* ── Test: condvar broadcast ── */
static volatile int g_broadcast_count = 0;
static eni_mutex_t g_bc_mtx;
static eni_condvar_t g_bc_cv;
static volatile int g_bc_go = 0;

static void *broadcast_waiter(void *arg)
{
    (void)arg;
    eni_mutex_lock(&g_bc_mtx);
    while (!g_bc_go) {
        eni_condvar_wait(&g_bc_cv, &g_bc_mtx);
    }
    g_broadcast_count++;
    eni_mutex_unlock(&g_bc_mtx);
    return NULL;
}

static void test_condvar_broadcast(void)
{
    TEST(condvar_broadcast);
    g_broadcast_count = 0;
    g_bc_go = 0;
    eni_mutex_init(&g_bc_mtx);
    eni_condvar_init(&g_bc_cv);

    eni_thread_t threads[3];
    for (int i = 0; i < 3; i++) {
        eni_thread_create(&threads[i], broadcast_waiter, NULL);
    }

    eni_platform_sleep_ms(50);
    eni_mutex_lock(&g_bc_mtx);
    g_bc_go = 1;
    eni_condvar_broadcast(&g_bc_cv);
    eni_mutex_unlock(&g_bc_mtx);

    for (int i = 0; i < 3; i++) {
        eni_thread_join(&threads[i], NULL);
    }

    if (g_broadcast_count != 3) { FAIL("not all threads woke up"); }
    else { PASS(); }

    eni_condvar_destroy(&g_bc_cv);
    eni_mutex_destroy(&g_bc_mtx);
}

/* ── Test: condvar timedwait timeout ── */
static void test_condvar_timedwait_timeout(void)
{
    TEST(condvar_timedwait_timeout);
    eni_mutex_t mtx;
    eni_condvar_t cv;
    eni_mutex_init(&mtx);
    eni_condvar_init(&cv);

    eni_mutex_lock(&mtx);
    eni_status_t rc = eni_condvar_timedwait(&cv, &mtx, 50); /* 50ms */
    eni_mutex_unlock(&mtx);

    if (rc != ENI_ERR_TIMEOUT) { FAIL("should timeout"); }
    else { PASS(); }

    eni_condvar_destroy(&cv);
    eni_mutex_destroy(&mtx);
}

/* ── Test: thread detach ── */
static volatile int g_detach_ran = 0;

static void *detach_thread_func(void *arg)
{
    (void)arg;
    g_detach_ran = 1;
    return NULL;
}

static void test_thread_detach(void)
{
    TEST(thread_detach);
    g_detach_ran = 0;
    eni_thread_t thr;
    eni_thread_create(&thr, detach_thread_func, NULL);
    eni_status_t rc = eni_thread_detach(&thr);
    if (rc != ENI_OK) { FAIL("detach failed"); return; }
    eni_platform_sleep_ms(100);
    if (!g_detach_ran) { FAIL("thread didn't run"); return; }
    PASS();
}

/* ── Test: mutex trylock ── */
static void test_mutex_trylock(void)
{
    TEST(mutex_trylock);
    eni_mutex_t mtx;
    eni_mutex_init(&mtx);

    eni_status_t rc = eni_mutex_trylock(&mtx);
    if (rc != ENI_OK) { FAIL("trylock should succeed"); eni_mutex_destroy(&mtx); return; }
    eni_mutex_unlock(&mtx);
    eni_mutex_destroy(&mtx);
    PASS();
}

/* ── Test: platform info ── */
static void test_platform_info(void)
{
    TEST(platform_info);
    eni_platform_info_t info = eni_platform_info();
    if (!info.os_name) { FAIL("null os_name"); return; }
    if (!info.arch) { FAIL("null arch"); return; }
    PASS();
}

/* ── Test: monotonic clock ── */
static void test_monotonic_clock(void)
{
    TEST(monotonic_clock);
    uint64_t t1 = eni_platform_monotonic_ms();
    eni_platform_sleep_ms(20);
    uint64_t t2 = eni_platform_monotonic_ms();
    if (t2 <= t1) { FAIL("clock should advance"); return; }
    PASS();
}

/* ── Test: atomics ── */
static void test_atomics(void)
{
    TEST(atomics);
    eni_atomic_int_t a;
    eni_atomic_init(&a, 0);
    if (eni_atomic_load(&a) != 0) { FAIL("initial"); return; }
    eni_atomic_store(&a, 10);
    if (eni_atomic_load(&a) != 10) { FAIL("store"); return; }
    int old = eni_atomic_fetch_add(&a, 5);
    if (old != 10) { FAIL("fetch_add return"); return; }
    if (eni_atomic_load(&a) != 15) { FAIL("after add"); return; }
    old = eni_atomic_fetch_sub(&a, 3);
    if (old != 15) { FAIL("fetch_sub return"); return; }
    if (eni_atomic_load(&a) != 12) { FAIL("after sub"); return; }
    PASS();
}

int main(void)
{
    printf("=== ENI Platform Extended Tests ===\n\n");
    eni_platform_init();
    test_condvar_wait_signal();
    test_condvar_broadcast();
    test_condvar_timedwait_timeout();
    test_thread_detach();
    test_mutex_trylock();
    test_platform_info();
    test_monotonic_clock();
    test_atomics();
    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           tests_passed, tests_run, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
