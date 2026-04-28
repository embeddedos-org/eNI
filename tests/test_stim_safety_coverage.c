// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Test: common/src/stim_safety.c — safety checks and recording

#include <stdio.h>
#include <string.h>
#include "eni/common.h"
#include "eni/stim_safety.h"

static int tests_run = 0, tests_passed = 0, tests_failed = 0;
#define TEST(name) do { tests_run++; printf("  TEST %-40s ", #name); } while(0)
#define PASS() do { tests_passed++; printf("[PASS]\n"); } while(0)
#define FAIL(msg) do { tests_failed++; printf("[FAIL] %s\n", msg); } while(0)

/* ── Test: safety init ── */
static void test_safety_init(void)
{
    TEST(stim_safety_init);
    eni_stim_safety_t safety;
    eni_stim_safety_init(&safety, 1.0f, 500, 100, 50);
    if (safety.max_amplitude != 1.0f) { FAIL("wrong max_amp"); return; }
    if (safety.max_duration_ms != 500) { FAIL("wrong max_dur"); return; }
    if (safety.min_interval_ms != 100) { FAIL("wrong interval"); return; }
    if (safety.max_daily_count != 50) { FAIL("wrong daily"); return; }
    PASS();
}

/* ── Test: safety check passes for valid params ── */
static void test_safety_check_valid(void)
{
    TEST(stim_safety_check_valid);
    eni_stim_safety_t safety;
    eni_stim_safety_init(&safety, 1.0f, 500, 100, 50);

    eni_stim_params_t params = {
        .type = ENI_STIM_HAPTIC,
        .amplitude = 0.5f,
        .duration_ms = 200,
    };

    eni_status_t rc = eni_stim_safety_check(&safety, &params, 1000);
    if (rc != ENI_OK) { FAIL("should pass for valid params"); return; }
    PASS();
}

/* ── Test: safety check rejects excessive amplitude ── */
static void test_safety_check_amplitude(void)
{
    TEST(stim_safety_check_amplitude);
    eni_stim_safety_t safety;
    eni_stim_safety_init(&safety, 1.0f, 500, 100, 50);

    eni_stim_params_t params = {
        .type = ENI_STIM_HAPTIC,
        .amplitude = 1.5f, /* Over max */
        .duration_ms = 200,
    };

    eni_status_t rc = eni_stim_safety_check(&safety, &params, 1000);
    if (rc == ENI_OK) { FAIL("should reject high amplitude"); return; }
    PASS();
}

/* ── Test: safety check rejects excessive duration ── */
static void test_safety_check_duration(void)
{
    TEST(stim_safety_check_duration);
    eni_stim_safety_t safety;
    eni_stim_safety_init(&safety, 1.0f, 500, 100, 50);

    eni_stim_params_t params = {
        .type = ENI_STIM_HAPTIC,
        .amplitude = 0.5f,
        .duration_ms = 1000, /* Over max */
    };

    eni_status_t rc = eni_stim_safety_check(&safety, &params, 1000);
    if (rc == ENI_OK) { FAIL("should reject long duration"); return; }
    PASS();
}

/* ── Test: safety record and interval check ── */
static void test_safety_interval(void)
{
    TEST(stim_safety_interval);
    eni_stim_safety_t safety;
    eni_stim_safety_init(&safety, 1.0f, 500, 1000, 50); /* 1000ms min interval */

    eni_stim_params_t params = {
        .type = ENI_STIM_HAPTIC,
        .amplitude = 0.5f,
        .duration_ms = 200,
    };

    /* First stim at t=1000 */
    eni_stim_safety_record(&safety, 1000);

    /* Try at t=1500 - only 500ms later, should fail */
    eni_status_t rc = eni_stim_safety_check(&safety, &params, 1500);
    if (rc == ENI_OK) { FAIL("should reject too-soon stim"); return; }

    /* Try at t=2500 - 1500ms later, should pass */
    rc = eni_stim_safety_check(&safety, &params, 2500);
    if (rc != ENI_OK) { FAIL("should pass after interval"); return; }
    PASS();
}

/* ── Test: safety reset daily ── */
static void test_safety_reset_daily(void)
{
    TEST(stim_safety_reset_daily);
    eni_stim_safety_t safety;
    eni_stim_safety_init(&safety, 1.0f, 500, 100, 50);
    safety.daily_count = 45;
    eni_stim_safety_reset_daily(&safety);
    if (safety.daily_count != 0) { FAIL("daily count not reset"); return; }
    PASS();
}

/* ── Test: absolute max amplitude ── */
static void test_safety_absolute_max(void)
{
    TEST(stim_safety_absolute_max);
    eni_stim_safety_t safety;
    eni_stim_safety_init(&safety, 5.0f, 500, 100, 50); /* User sets 5mA but absolute max is 2mA */

    eni_stim_params_t params = {
        .type = ENI_STIM_NEURAL,
        .amplitude = 3.0f, /* Over absolute max of 2.0 */
        .duration_ms = 200,
    };

    eni_status_t rc = eni_stim_safety_check(&safety, &params, 1000);
    if (rc == ENI_OK) { FAIL("should reject above absolute max"); return; }
    PASS();
}

int main(void)
{
    printf("=== ENI Stim Safety Coverage Tests ===\n\n");
    test_safety_init();
    test_safety_check_valid();
    test_safety_check_amplitude();
    test_safety_check_duration();
    test_safety_interval();
    test_safety_reset_daily();
    test_safety_absolute_max();
    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           tests_passed, tests_run, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
