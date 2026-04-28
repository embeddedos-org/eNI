// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Thread safety tests for platform primitives and stream bus

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "eni_platform/platform.h"
#include "eni_fw/stream_bus.h"

static int tests_run = 0, tests_passed = 0, tests_failed = 0;
#define TEST(name) do { tests_run++; printf("  TEST %-40s ", #name); } while(0)
#define PASS() do { tests_passed++; printf("[PASS]\n"); } while(0)
#define FAIL(msg) do { tests_failed++; printf("[FAIL] %s\n", msg); } while(0)

static void test_mutex_basic(void)
{
    TEST(mutex_basic);
    eni_mutex_t mtx;
    assert(eni_mutex_init(&mtx) == ENI_OK);
    assert(eni_mutex_lock(&mtx) == ENI_OK);
    assert(eni_mutex_unlock(&mtx) == ENI_OK);
    eni_mutex_destroy(&mtx);
    PASS();
}

static void test_mutex_trylock(void)
{
    TEST(mutex_trylock);
    eni_mutex_t mtx;
    eni_mutex_init(&mtx);
    assert(eni_mutex_trylock(&mtx) == ENI_OK);
    eni_mutex_unlock(&mtx);
    eni_mutex_destroy(&mtx);
    PASS();
}

static void test_condvar_basic(void)
{
    TEST(condvar_basic);
    eni_condvar_t cv;
    assert(eni_condvar_init(&cv) == ENI_OK);
    eni_condvar_destroy(&cv);
    PASS();
}

static void test_condvar_timedwait(void)
{
    TEST(condvar_timedwait);
    eni_condvar_t cv;
    eni_mutex_t mtx;
    eni_condvar_init(&cv);
    eni_mutex_init(&mtx);

    eni_mutex_lock(&mtx);
    eni_status_t rc = eni_condvar_timedwait(&cv, &mtx, 10); /* 10ms timeout */
    assert(rc == ENI_ERR_TIMEOUT);
    eni_mutex_unlock(&mtx);

    eni_condvar_destroy(&cv);
    eni_mutex_destroy(&mtx);
    PASS();
}

static void test_atomics(void)
{
    TEST(atomics);
    eni_atomic_int_t a;
    eni_atomic_init(&a, 0);
    assert(eni_atomic_load(&a) == 0);

    eni_atomic_store(&a, 42);
    assert(eni_atomic_load(&a) == 42);

    int old = eni_atomic_fetch_add(&a, 10);
    assert(old == 42);
    assert(eni_atomic_load(&a) == 52);

    old = eni_atomic_fetch_sub(&a, 2);
    assert(old == 52);
    assert(eni_atomic_load(&a) == 50);

    int ok = eni_atomic_compare_exchange(&a, 50, 100);
    assert(ok);
    assert(eni_atomic_load(&a) == 100);

    ok = eni_atomic_compare_exchange(&a, 999, 200);
    assert(!ok);
    assert(eni_atomic_load(&a) == 100);
    PASS();
}

static eni_atomic_int_t counter;

static void *increment_thread(void *arg)
{
    int n = *(int *)arg;
    for (int i = 0; i < n; i++) {
        eni_atomic_fetch_add(&counter, 1);
    }
    return NULL;
}

static void test_thread_create_join(void)
{
    TEST(thread_create_join);
    eni_atomic_init(&counter, 0);
    int count = 1000;

    eni_thread_t t1, t2;
    assert(eni_thread_create(&t1, increment_thread, &count) == ENI_OK);
    assert(eni_thread_create(&t2, increment_thread, &count) == ENI_OK);

    assert(eni_thread_join(&t1, NULL) == ENI_OK);
    assert(eni_thread_join(&t2, NULL) == ENI_OK);

    assert(eni_atomic_load(&counter) == 2000);
    PASS();
}

static void test_stream_bus_threadsafe(void)
{
    TEST(stream_bus_threadsafe);
    static eni_fw_stream_bus_t bus; /* static — too large for stack (~1.1MB) */
    memset(&bus, 0, sizeof(bus));
    assert(eni_fw_stream_bus_init(&bus) == ENI_OK);

    /* Push/pop test */
    eni_event_t ev;
    eni_event_init(&ev, ENI_EVENT_INTENT, "test");
    eni_event_set_intent(&ev, "focus", 0.9f);

    for (int i = 0; i < 10; i++) {
        assert(eni_fw_stream_bus_push(&bus, &ev) == ENI_OK);
    }
    assert(eni_fw_stream_bus_pending(&bus) == 10);
    assert(!eni_fw_stream_bus_empty(&bus));

    eni_event_t popped;
    for (int i = 0; i < 10; i++) {
        assert(eni_fw_stream_bus_pop(&bus, &popped) == ENI_OK);
    }
    assert(eni_fw_stream_bus_empty(&bus));
    assert(eni_fw_stream_bus_pop(&bus, &popped) == ENI_ERR_TIMEOUT);

    eni_fw_stream_bus_destroy(&bus);
    PASS();
}

int main(void)
{
    printf("=== Thread Safety Tests ===\n");
    test_mutex_basic();
    test_mutex_trylock();
    test_condvar_basic();
    test_condvar_timedwait();
    test_atomics();
    test_thread_create_join();
    test_stream_bus_threadsafe();
    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return tests_failed > 0 ? 1 : 0;
}
