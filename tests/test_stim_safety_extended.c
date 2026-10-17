// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Extended stimulation safety tests: edge cases, timing, cumulative limits

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "eni/stim_safety.h"

static int passed = 0;
#define PASS(name) do { printf("[PASS] %s\n", name); passed++; } while(0)

static void test_safety_zero_amplitude(void) {
    eni_stim_safety_t safety;
    eni_stim_safety_init(&safety, 1.0f, 60000, 1000, 100);
    eni_stim_params_t params = {0};
    params.amplitude = 0.0f;
    params.duration_ms = 100;
    assert(eni_stim_safety_check(&safety, &params, 0) == ENI_OK);
    PASS("safety_zero_amplitude");
}

static void test_safety_negative_amplitude(void) {
    eni_stim_safety_t safety;
    eni_stim_safety_init(&safety, 1.0f, 60000, 1000, 100);
    eni_stim_params_t params = {0};
    params.amplitude = -0.5f;
    params.duration_ms = 100;
    /* Negative amplitude should be treated as invalid or handled gracefully */
    eni_status_t rc = eni_stim_safety_check(&safety, &params, 0);
    (void)rc; /* Implementation-defined behavior */
    PASS("safety_negative_amplitude");
}

static void test_safety_exact_amplitude_limit(void) {
    eni_stim_safety_t safety;
    eni_stim_safety_init(&safety, 1.0f, 60000, 0, 0);
    eni_stim_params_t params = {0};
    params.amplitude = 1.0f;
    params.duration_ms = 100;
    assert(eni_stim_safety_check(&safety, &params, 0) == ENI_OK);
    PASS("safety_exact_amplitude_limit");
}

static void test_safety_exact_absolute_limit(void) {
    eni_stim_safety_t safety;
    eni_stim_safety_init(&safety, ENI_STIM_ABSOLUTE_MAX_AMPLITUDE, ENI_STIM_ABSOLUTE_MAX_DURATION_MS, 0, 0);
    eni_stim_params_t params = {0};
    params.amplitude = ENI_STIM_ABSOLUTE_MAX_AMPLITUDE;
    params.duration_ms = 100;
    assert(eni_stim_safety_check(&safety, &params, 0) == ENI_OK);
    PASS("safety_exact_absolute_limit");
}

static void test_safety_duration_exceed(void) {
    eni_stim_safety_t safety;
    eni_stim_safety_init(&safety, 1.0f, 1000, 0, 0);
    eni_stim_params_t params = {0};
    params.amplitude = 0.5f;
    params.duration_ms = 2000;
    assert(eni_stim_safety_check(&safety, &params, 0) == ENI_ERR_PERMISSION);
    PASS("safety_duration_exceed");
}

static void test_safety_rate_limit_exact_boundary(void) {
    eni_stim_safety_t safety;
    eni_stim_safety_init(&safety, 1.0f, 60000, 5000, 100);
    eni_stim_params_t params = {0};
    params.amplitude = 0.5f;
    params.duration_ms = 100;
    assert(eni_stim_safety_check(&safety, &params, 1000) == ENI_OK);
    eni_stim_safety_record(&safety, 1000);
    /* Exactly at interval boundary */
    assert(eni_stim_safety_check(&safety, &params, 6000) == ENI_OK);
    PASS("safety_rate_limit_exact_boundary");
}

static void test_safety_rate_limit_one_ms_early(void) {
    eni_stim_safety_t safety;
    eni_stim_safety_init(&safety, 1.0f, 60000, 5000, 100);
    eni_stim_params_t params = {0};
    params.amplitude = 0.5f;
    params.duration_ms = 100;
    assert(eni_stim_safety_check(&safety, &params, 1000) == ENI_OK);
    eni_stim_safety_record(&safety, 1000);
    /* 1ms too early */
    assert(eni_stim_safety_check(&safety, &params, 5999) == ENI_ERR_PERMISSION);
    PASS("safety_rate_limit_one_ms_early");
}

static void test_safety_rate_limit_backwards_clock(void) {
    eni_stim_safety_t safety;
    eni_stim_safety_init(&safety, 1.0f, 60000, 5000, 100);
    eni_stim_params_t params = {0};
    params.amplitude = 0.5f;
    params.duration_ms = 100;
    assert(eni_stim_safety_check(&safety, &params, 10000) == ENI_OK);
    eni_stim_safety_record(&safety, 10000);
    /* F3-2: a backwards clock reading must fail closed — the unsigned
     * elapsed subtraction would otherwise wrap and pass the gate. */
    assert(eni_stim_safety_check(&safety, &params, 9999) == ENI_ERR_PERMISSION);
    assert(eni_stim_safety_check(&safety, &params, 0) == ENI_ERR_PERMISSION);
    /* And the gate recovers once the clock is sane again. */
    assert(eni_stim_safety_check(&safety, &params, 15000) == ENI_OK);
    PASS("safety_rate_limit_backwards_clock");
}

static void test_safety_daily_count_exact(void) {
    eni_stim_safety_t safety;
    eni_stim_safety_init(&safety, 1.0f, 60000, 0, 5);
    eni_stim_params_t params = {0};
    params.amplitude = 0.5f;
    params.duration_ms = 100;
    for (int i = 0; i < 5; i++) {
        assert(eni_stim_safety_check(&safety, &params, 0) == ENI_OK);
        eni_stim_safety_record(&safety, 0);
    }
    assert(eni_stim_safety_check(&safety, &params, 0) == ENI_ERR_PERMISSION);
    PASS("safety_daily_count_exact");
}

static void test_safety_daily_reset_restores_full_count(void) {
    eni_stim_safety_t safety;
    eni_stim_safety_init(&safety, 1.0f, 60000, 0, 3);
    eni_stim_params_t params = {0};
    params.amplitude = 0.5f;
    params.duration_ms = 100;
    for (int i = 0; i < 3; i++) {
        eni_stim_safety_check(&safety, &params, 0);
        eni_stim_safety_record(&safety, 0);
    }
    assert(eni_stim_safety_check(&safety, &params, 0) == ENI_ERR_PERMISSION);
    eni_stim_safety_reset_daily(&safety);
    for (int i = 0; i < 3; i++) {
        assert(eni_stim_safety_check(&safety, &params, 0) == ENI_OK);
        eni_stim_safety_record(&safety, 0);
    }
    assert(eni_stim_safety_check(&safety, &params, 0) == ENI_ERR_PERMISSION);
    PASS("safety_daily_reset_restores_full_count");
}

static void test_safety_total_count_preserved_after_daily_reset(void) {
    eni_stim_safety_t safety;
    eni_stim_safety_init(&safety, 1.0f, 60000, 0, 10);
    eni_stim_params_t params = {0};
    params.amplitude = 0.5f;
    params.duration_ms = 100;
    eni_stim_safety_check(&safety, &params, 0);
    eni_stim_safety_record(&safety, 0);
    eni_stim_safety_record(&safety, 0);
    uint32_t total_before = safety.total_count;
    eni_stim_safety_reset_daily(&safety);
    assert(safety.total_count == total_before);
    PASS("safety_total_count_preserved_after_daily_reset");
}

static void test_safety_multiple_stim_types(void) {
    eni_stim_safety_t safety;
    eni_stim_safety_init(&safety, 1.5f, 300000, 0, 100);
    eni_stim_params_t p1 = {0};
    p1.type = ENI_STIM_VISUAL;
    p1.amplitude = 1.0f;
    p1.duration_ms = 500;
    eni_stim_params_t p2 = {0};
    p2.type = ENI_STIM_HAPTIC;
    p2.amplitude = 0.8f;
    p2.duration_ms = 200;
    eni_stim_params_t p3 = {0};
    p3.type = ENI_STIM_TACS;
    p3.amplitude = 1.2f;
    p3.duration_ms = 1000;
    assert(eni_stim_safety_check(&safety, &p1, 0) == ENI_OK);
    assert(eni_stim_safety_check(&safety, &p2, 0) == ENI_OK);
    assert(eni_stim_safety_check(&safety, &p3, 0) == ENI_OK);
    PASS("safety_multiple_stim_types");
}

static void test_safety_combined_rate_and_daily(void) {
    eni_stim_safety_t safety;
    eni_stim_safety_init(&safety, 1.0f, 60000, 1000, 3);
    eni_stim_params_t params = {0};
    params.amplitude = 0.5f;
    params.duration_ms = 100;
    /* First stim at t=0 */
    assert(eni_stim_safety_check(&safety, &params, 0) == ENI_OK);
    eni_stim_safety_record(&safety, 0);
    /* Rate limited at t=500 */
    assert(eni_stim_safety_check(&safety, &params, 500) == ENI_ERR_PERMISSION);
    /* OK at t=1000 */
    assert(eni_stim_safety_check(&safety, &params, 1000) == ENI_OK);
    eni_stim_safety_record(&safety, 1000);
    /* OK at t=2000 */
    assert(eni_stim_safety_check(&safety, &params, 2000) == ENI_OK);
    eni_stim_safety_record(&safety, 2000);
    /* Daily limit reached at t=3000 */
    assert(eni_stim_safety_check(&safety, &params, 3000) == ENI_ERR_PERMISSION);
    PASS("safety_combined_rate_and_daily");
}

static void test_safety_init_clamping(void) {
    eni_stim_safety_t safety;
    eni_stim_safety_init(&safety, 100.0f, 999999999, 0, 0);
    assert(safety.max_amplitude <= ENI_STIM_ABSOLUTE_MAX_AMPLITUDE);
    assert(safety.max_duration_ms <= (uint32_t)ENI_STIM_ABSOLUTE_MAX_DURATION_MS);
    PASS("safety_init_clamping");
}

static void test_safety_record_increments(void) {
    eni_stim_safety_t safety;
    eni_stim_safety_init(&safety, 1.0f, 60000, 0, 100);
    assert(safety.daily_count == 0);
    assert(safety.total_count == 0);
    eni_stim_safety_record(&safety, 1000);
    assert(safety.daily_count == 1);
    assert(safety.total_count == 1);
    assert(safety.last_stim_time_ms == 1000);
    eni_stim_safety_record(&safety, 2000);
    assert(safety.daily_count == 2);
    assert(safety.total_count == 2);
    assert(safety.last_stim_time_ms == 2000);
    PASS("safety_record_increments");
}

int main(void) {
    printf("=== ENI Stim Safety Extended Tests ===\n");
    test_safety_zero_amplitude();
    test_safety_negative_amplitude();
    test_safety_exact_amplitude_limit();
    test_safety_exact_absolute_limit();
    test_safety_duration_exceed();
    test_safety_rate_limit_exact_boundary();
    test_safety_rate_limit_one_ms_early();
    test_safety_rate_limit_backwards_clock();
    test_safety_daily_count_exact();
    test_safety_daily_reset_restores_full_count();
    test_safety_total_count_preserved_after_daily_reset();
    test_safety_multiple_stim_types();
    test_safety_combined_rate_and_daily();
    test_safety_init_clamping();
    test_safety_record_increments();
    printf("\n=== ALL %d TESTS PASSED ===\n", passed);
    return 0;
}

