// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023
//
// Coverage tests for common/src/event.c
// Exercises: eni_event_print for all 4 event types (intent/features/raw/control),
//            null event guard, overflow scenarios, long source names

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "eni/common.h"

static int tests_run    = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    do { \
        tests_run++; \
        printf("  TEST %-40s ", #name); \
    } while (0)

#define PASS() \
    do { \
        tests_passed++; \
        printf("[PASS]\n"); \
    } while (0)

#define FAIL(msg) \
    do { \
        tests_failed++; \
        printf("[FAIL] %s\n", msg); \
    } while (0)

/* ---------- eni_event_print: null guard ---------- */

static void test_event_print_null(void)
{
    TEST(event_print_null);
    /* Should not crash; just returns immediately */
    eni_event_print(NULL);
    PASS();
}

/* ---------- eni_event_print: intent type ---------- */

static void test_event_print_intent(void)
{
    TEST(event_print_intent);
    eni_event_t ev;
    eni_event_init(&ev, ENI_EVENT_INTENT, "src-intent");
    eni_event_set_intent(&ev, "move_left", 0.95f);

    /* Exercises lines 80-92: type switch (INTENT branch) + intent detail print */
    eni_event_print(&ev);
    PASS();
}

/* ---------- eni_event_print: features type ---------- */

static void test_event_print_features(void)
{
    TEST(event_print_features);
    eni_event_t ev;
    eni_event_init(&ev, ENI_EVENT_FEATURES, "src-feat");
    eni_event_add_feature(&ev, "alpha", 0.5f);
    eni_event_add_feature(&ev, "beta", 0.8f);

    /* Exercises lines 82, 93-99: FEATURES branch + feature loop */
    eni_event_print(&ev);
    PASS();
}

/* ---------- eni_event_print: raw type ---------- */

static void test_event_print_raw(void)
{
    TEST(event_print_raw);
    eni_event_t ev;
    eni_event_init(&ev, ENI_EVENT_RAW, "src-raw");
    uint8_t data[] = {0xAA, 0xBB, 0xCC};
    eni_event_set_raw(&ev, data, sizeof(data));

    /* Exercises lines 83, 100-101: RAW branch */
    eni_event_print(&ev);
    PASS();
}

/* ---------- eni_event_print: control type ---------- */

static void test_event_print_control(void)
{
    TEST(event_print_control);
    eni_event_t ev;
    eni_event_init(&ev, ENI_EVENT_CONTROL, "src-ctrl");

    /* Exercises line 84: CONTROL branch (no extra detail printed) */
    eni_event_print(&ev);
    PASS();
}

/* ---------- eni_event_print: features with zero count ---------- */

static void test_event_print_features_empty(void)
{
    TEST(event_print_features_empty);
    eni_event_t ev;
    eni_event_init(&ev, ENI_EVENT_FEATURES, "src-feat-empty");

    /* Exercises the features branch with count=0 (loop body skipped) */
    eni_event_print(&ev);
    PASS();
}

/* ---------- overflow: features at max ---------- */

static void test_event_feature_overflow(void)
{
    TEST(event_feature_overflow);
    eni_event_t ev;
    eni_event_init(&ev, ENI_EVENT_FEATURES, "src");

    /* Fill to capacity */
    char name[32];
    for (int i = 0; i < ENI_EVENT_FEATURES_MAX; i++) {
        snprintf(name, sizeof(name), "f%d", i);
        eni_status_t st = eni_event_add_feature(&ev, name, (float)i);
        if (st != ENI_OK) { FAIL("add_feature should succeed"); return; }
    }

    /* One more should overflow */
    eni_status_t st = eni_event_add_feature(&ev, "overflow", 999.0f);
    if (st != ENI_ERR_OVERFLOW) { FAIL("expected overflow"); return; }
    PASS();
}

/* ---------- overflow: raw payload too large ---------- */

static void test_event_raw_overflow(void)
{
    TEST(event_raw_overflow);
    eni_event_t ev;
    eni_event_init(&ev, ENI_EVENT_RAW, "src");

    uint8_t big[ENI_EVENT_PAYLOAD_MAX + 1];
    memset(big, 0xAA, sizeof(big));
    eni_status_t st = eni_event_set_raw(&ev, big, sizeof(big));
    if (st != ENI_ERR_OVERFLOW) { FAIL("expected overflow for raw"); return; }
    PASS();
}

/* ---------- null inputs ---------- */

static void test_event_init_null_ev(void)
{
    TEST(event_init_null_ev);
    eni_status_t st = eni_event_init(NULL, ENI_EVENT_INTENT, "src");
    if (st != ENI_ERR_INVALID) { FAIL("expected INVALID"); return; }
    PASS();
}

static void test_event_init_null_source(void)
{
    TEST(event_init_null_source);
    eni_event_t ev;
    eni_status_t st = eni_event_init(&ev, ENI_EVENT_INTENT, NULL);
    if (st != ENI_ERR_INVALID) { FAIL("expected INVALID"); return; }
    PASS();
}

static void test_event_set_intent_null_ev(void)
{
    TEST(event_set_intent_null_ev);
    eni_status_t st = eni_event_set_intent(NULL, "test", 0.5f);
    if (st != ENI_ERR_INVALID) { FAIL("expected INVALID"); return; }
    PASS();
}

static void test_event_set_intent_null_intent(void)
{
    TEST(event_set_intent_null_intent);
    eni_event_t ev;
    eni_event_init(&ev, ENI_EVENT_INTENT, "src");
    eni_status_t st = eni_event_set_intent(&ev, NULL, 0.5f);
    if (st != ENI_ERR_INVALID) { FAIL("expected INVALID"); return; }
    PASS();
}

static void test_event_set_raw_null_data(void)
{
    TEST(event_set_raw_null_data);
    eni_event_t ev;
    eni_event_init(&ev, ENI_EVENT_RAW, "src");
    eni_status_t st = eni_event_set_raw(&ev, NULL, 10);
    if (st != ENI_ERR_INVALID) { FAIL("expected INVALID"); return; }
    PASS();
}

static void test_event_add_feature_null_name(void)
{
    TEST(event_add_feature_null_name);
    eni_event_t ev;
    eni_event_init(&ev, ENI_EVENT_FEATURES, "src");
    eni_status_t st = eni_event_add_feature(&ev, NULL, 1.0f);
    if (st != ENI_ERR_INVALID) { FAIL("expected INVALID"); return; }
    PASS();
}

/* ---------- type mismatch ---------- */

static void test_event_set_intent_wrong_type(void)
{
    TEST(event_set_intent_wrong_type);
    eni_event_t ev;
    eni_event_init(&ev, ENI_EVENT_RAW, "src");
    eni_status_t st = eni_event_set_intent(&ev, "intent", 0.5f);
    if (st != ENI_ERR_INVALID) { FAIL("expected INVALID"); return; }
    PASS();
}

static void test_event_add_feature_wrong_type(void)
{
    TEST(event_add_feature_wrong_type);
    eni_event_t ev;
    eni_event_init(&ev, ENI_EVENT_INTENT, "src");
    eni_status_t st = eni_event_add_feature(&ev, "feat", 1.0f);
    if (st != ENI_ERR_INVALID) { FAIL("expected INVALID"); return; }
    PASS();
}

static void test_event_set_raw_wrong_type(void)
{
    TEST(event_set_raw_wrong_type);
    eni_event_t ev;
    eni_event_init(&ev, ENI_EVENT_INTENT, "src");
    uint8_t d = 0;
    eni_status_t st = eni_event_set_raw(&ev, &d, 1);
    if (st != ENI_ERR_INVALID) { FAIL("expected INVALID"); return; }
    PASS();
}

/* ---------- long source name truncation ---------- */

static void test_event_long_source(void)
{
    TEST(event_long_source);
    char long_src[ENI_EVENT_SOURCE_MAX + 32];
    memset(long_src, 'A', sizeof(long_src) - 1);
    long_src[sizeof(long_src) - 1] = '\0';

    eni_event_t ev;
    eni_status_t st = eni_event_init(&ev, ENI_EVENT_INTENT, long_src);
    if (st != ENI_OK) { FAIL("init failed"); return; }
    if (strlen(ev.source) >= ENI_EVENT_SOURCE_MAX) { FAIL("source not truncated"); return; }
    PASS();
}

/* ---------- long intent name truncation ---------- */

static void test_event_long_intent(void)
{
    TEST(event_long_intent);
    char long_intent[ENI_EVENT_INTENT_MAX + 32];
    memset(long_intent, 'B', sizeof(long_intent) - 1);
    long_intent[sizeof(long_intent) - 1] = '\0';

    eni_event_t ev;
    eni_event_init(&ev, ENI_EVENT_INTENT, "src");
    eni_status_t st = eni_event_set_intent(&ev, long_intent, 0.5f);
    if (st != ENI_OK) { FAIL("set_intent failed"); return; }
    if (strlen(ev.payload.intent.name) >= ENI_EVENT_INTENT_MAX) { FAIL("intent not truncated"); return; }
    PASS();
}

/* ---------- main ---------- */

int main(void)
{
    printf("=== event.c Coverage Tests ===\n\n");

    /* eni_event_print branches — exercises all 4 type switch cases */
    test_event_print_null();
    test_event_print_intent();
    test_event_print_features();
    test_event_print_raw();
    test_event_print_control();
    test_event_print_features_empty();

    /* Overflow */
    test_event_feature_overflow();
    test_event_raw_overflow();

    /* Null inputs */
    test_event_init_null_ev();
    test_event_init_null_source();
    test_event_set_intent_null_ev();
    test_event_set_intent_null_intent();
    test_event_set_raw_null_data();
    test_event_add_feature_null_name();

    /* Type mismatch */
    test_event_set_intent_wrong_type();
    test_event_add_feature_wrong_type();
    test_event_set_raw_wrong_type();

    /* Truncation */
    test_event_long_source();
    test_event_long_intent();

    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           tests_passed, tests_run, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
