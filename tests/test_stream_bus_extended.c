// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Test: framework/src/stream_bus.c — overflow, pop_wait, stats, destroy

#include <stdio.h>
#include <string.h>
#include "eni/common.h"
#include "eni_fw/stream_bus.h"
#include "eni_platform/platform.h"

static int tests_run = 0, tests_passed = 0, tests_failed = 0;
#define TEST(name) do { tests_run++; printf("  TEST %-40s ", #name); } while(0)
#define PASS() do { tests_passed++; printf("[PASS]\n"); } while(0)
#define FAIL(msg) do { tests_failed++; printf("[FAIL] %s\n", msg); } while(0)

/* ── Test: bus overflow ── */
static void test_bus_overflow(void)
{
    TEST(stream_bus_overflow);
    eni_fw_stream_bus_t bus;
    eni_fw_stream_bus_init(&bus);

    eni_event_t ev;
    eni_event_init(&ev, ENI_EVENT_CONTROL, "test");

    /* Fill the bus to capacity */
    for (int i = 0; i < ENI_FW_STREAM_BUS_CAPACITY; i++) {
        eni_status_t rc = eni_fw_stream_bus_push(&bus, &ev);
        if (rc != ENI_OK) { FAIL("push should succeed within capacity"); eni_fw_stream_bus_destroy(&bus); return; }
    }

    /* Next push should overflow */
    eni_status_t rc = eni_fw_stream_bus_push(&bus, &ev);
    if (rc != ENI_ERR_OVERFLOW) { FAIL("should overflow"); eni_fw_stream_bus_destroy(&bus); return; }

    eni_fw_stream_bus_destroy(&bus);
    PASS();
}

/* ── Test: pop from empty bus ── */
static void test_bus_pop_empty(void)
{
    TEST(stream_bus_pop_empty);
    eni_fw_stream_bus_t bus;
    eni_fw_stream_bus_init(&bus);

    eni_event_t ev;
    eni_status_t rc = eni_fw_stream_bus_pop(&bus, &ev);
    if (rc != ENI_ERR_TIMEOUT) { FAIL("should return TIMEOUT on empty"); eni_fw_stream_bus_destroy(&bus); return; }

    eni_fw_stream_bus_destroy(&bus);
    PASS();
}

/* ── Test: pop_wait timeout on empty bus ── */
static void test_bus_pop_wait_timeout(void)
{
    TEST(stream_bus_pop_wait_timeout);
    eni_fw_stream_bus_t bus;
    eni_fw_stream_bus_init(&bus);

    eni_event_t ev;
    eni_status_t rc = eni_fw_stream_bus_pop_wait(&bus, &ev, 50); /* 50ms timeout */
    if (rc != ENI_ERR_TIMEOUT) { FAIL("should timeout"); eni_fw_stream_bus_destroy(&bus); return; }

    eni_fw_stream_bus_destroy(&bus);
    PASS();
}

/* ── Test: pop_wait with data ── */
typedef struct {
    eni_fw_stream_bus_t *bus;
} producer_arg_t;

static void *producer_thread(void *arg)
{
    producer_arg_t *pa = (producer_arg_t *)arg;
    eni_platform_sleep_ms(30); /* Wait a bit before pushing */
    eni_event_t ev;
    eni_event_init(&ev, ENI_EVENT_INTENT, "producer");
    eni_event_set_intent(&ev, "test_intent", 0.99f);
    eni_fw_stream_bus_push(pa->bus, &ev);
    return NULL;
}

static void test_bus_pop_wait_signal(void)
{
    TEST(stream_bus_pop_wait_signal);
    eni_fw_stream_bus_t bus;
    eni_fw_stream_bus_init(&bus);

    producer_arg_t pa = { .bus = &bus };
    eni_thread_t thr;
    eni_thread_create(&thr, producer_thread, &pa);

    eni_event_t ev;
    eni_status_t rc = eni_fw_stream_bus_pop_wait(&bus, &ev, 2000); /* 2s timeout - should get data */
    eni_thread_join(&thr, NULL);

    if (rc != ENI_OK) { FAIL("pop_wait should succeed"); eni_fw_stream_bus_destroy(&bus); return; }
    if (ev.type != ENI_EVENT_INTENT) { FAIL("wrong event type"); eni_fw_stream_bus_destroy(&bus); return; }
    if (strcmp(ev.payload.intent.name, "test_intent") != 0) { FAIL("wrong intent"); eni_fw_stream_bus_destroy(&bus); return; }

    eni_fw_stream_bus_destroy(&bus);
    PASS();
}

/* ── Test: pending count ── */
static void test_bus_pending(void)
{
    TEST(stream_bus_pending);
    eni_fw_stream_bus_t bus;
    eni_fw_stream_bus_init(&bus);

    if (eni_fw_stream_bus_pending(&bus) != 0) { FAIL("should be 0"); eni_fw_stream_bus_destroy(&bus); return; }

    eni_event_t ev;
    eni_event_init(&ev, ENI_EVENT_CONTROL, "test");
    eni_fw_stream_bus_push(&bus, &ev);
    eni_fw_stream_bus_push(&bus, &ev);

    if (eni_fw_stream_bus_pending(&bus) != 2) { FAIL("should be 2"); eni_fw_stream_bus_destroy(&bus); return; }

    eni_fw_stream_bus_pop(&bus, &ev);
    if (eni_fw_stream_bus_pending(&bus) != 1) { FAIL("should be 1"); eni_fw_stream_bus_destroy(&bus); return; }

    eni_fw_stream_bus_destroy(&bus);
    PASS();
}

/* ── Test: stats output (no crash) ── */
static void test_bus_stats(void)
{
    TEST(stream_bus_stats);
    eni_fw_stream_bus_t bus;
    eni_fw_stream_bus_init(&bus);

    eni_event_t ev;
    eni_event_init(&ev, ENI_EVENT_CONTROL, "test");
    eni_fw_stream_bus_push(&bus, &ev);

    eni_fw_stream_bus_stats(&bus);
    eni_fw_stream_bus_stats(NULL); /* Should not crash */

    eni_fw_stream_bus_destroy(&bus);
    PASS();
}

/* ── Test: destroy and empty on uninitialized ── */
static void test_bus_uninit_ops(void)
{
    TEST(stream_bus_uninit_ops);
    eni_fw_stream_bus_t bus;
    memset(&bus, 0, sizeof(bus));

    /* Operations on uninitialized bus */
    if (!eni_fw_stream_bus_empty(&bus)) { FAIL("should be empty"); return; }
    if (eni_fw_stream_bus_pending(&bus) != 0) { FAIL("should be 0"); return; }
    eni_fw_stream_bus_stats(&bus);
    eni_fw_stream_bus_destroy(&bus); /* Should not crash */
    PASS();
}

/* ── Test: NULL arg checks ── */
static void test_bus_null_args(void)
{
    TEST(stream_bus_null_args);
    eni_event_t ev;
    if (eni_fw_stream_bus_init(NULL) != ENI_ERR_INVALID) { FAIL("null init"); return; }
    if (eni_fw_stream_bus_push(NULL, &ev) != ENI_ERR_INVALID) { FAIL("null push bus"); return; }

    eni_fw_stream_bus_t bus;
    eni_fw_stream_bus_init(&bus);
    if (eni_fw_stream_bus_push(&bus, NULL) != ENI_ERR_INVALID) { FAIL("null push ev"); eni_fw_stream_bus_destroy(&bus); return; }
    if (eni_fw_stream_bus_pop(&bus, NULL) != ENI_ERR_INVALID) { FAIL("null pop ev"); eni_fw_stream_bus_destroy(&bus); return; }
    if (eni_fw_stream_bus_pop_wait(&bus, NULL, 10) != ENI_ERR_INVALID) { FAIL("null popwait ev"); eni_fw_stream_bus_destroy(&bus); return; }
    eni_fw_stream_bus_destroy(&bus);
    PASS();
}

int main(void)
{
    printf("=== ENI Stream Bus Extended Tests ===\n\n");
    eni_platform_init();
    test_bus_overflow();
    test_bus_pop_empty();
    test_bus_pop_wait_timeout();
    test_bus_pop_wait_signal();
    test_bus_pending();
    test_bus_stats();
    test_bus_uninit_ops();
    test_bus_null_args();
    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           tests_passed, tests_run, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
