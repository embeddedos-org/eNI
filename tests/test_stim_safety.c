// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include "eni/stim_safety.h"

static int passed = 0;
#define PASS(name) do { printf("[PASS] %s\n", name); passed++; } while(0)

static void test_safety_init(void) {
    eni_stim_safety_t safety;
    eni_stim_safety_init(&safety, 1.0f, 60000, 5000, 50);
    assert(safety.max_amplitude <= 1.0f);
    assert(safety.max_duration_ms == 60000);
    assert(safety.min_interval_ms == 5000);
    assert(safety.max_daily_count == 50);
    PASS("safety_init");
}

static void test_safety_clamp_absolute(void) {
    eni_stim_safety_t safety;
    eni_stim_safety_init(&safety, 10.0f, 99999999, 0, 0); /* exceeds absolute max */
    assert(safety.max_amplitude <= ENI_STIM_ABSOLUTE_MAX_AMPLITUDE);
    assert(safety.max_duration_ms <= ENI_STIM_ABSOLUTE_MAX_DURATION_MS);
    PASS("safety_clamp_absolute");
}

static void test_safety_check_within_limits(void) {
    eni_stim_safety_t safety;
    eni_stim_safety_init(&safety, 1.5f, 300000, 1000, 100);
    eni_stim_params_t params = {0};
    params.amplitude = 1.0f;
    params.duration_ms = 1000;
    assert(eni_stim_safety_check(&safety, &params, 0) == ENI_OK);
    PASS("safety_check_within_limits");
}

static void test_safety_check_amplitude_exceed(void) {
    eni_stim_safety_t safety;
    eni_stim_safety_init(&safety, 1.0f, 300000, 0, 0);
    eni_stim_params_t params = {0};
    params.amplitude = 1.5f;
    params.duration_ms = 100;
    assert(eni_stim_safety_check(&safety, &params, 0) == ENI_ERR_PERMISSION);
    PASS("safety_check_amplitude_exceed");
}

static void test_safety_check_absolute_exceed(void) {
    eni_stim_safety_t safety;
    eni_stim_safety_init(&safety, 1.5f, 300000, 0, 0);
    eni_stim_params_t params = {0};
    params.amplitude = 3.0f; /* exceeds absolute max of 2.0 */
    params.duration_ms = 100;
    assert(eni_stim_safety_check(&safety, &params, 0) == ENI_ERR_PERMISSION);
    PASS("safety_check_absolute_exceed");
}

static void test_safety_rate_limit(void) {
    eni_stim_safety_t safety;
    eni_stim_safety_init(&safety, 1.5f, 300000, 5000, 100);
    eni_stim_params_t params = {0};
    params.amplitude = 0.5f;
    params.duration_ms = 100;
    /* First stim ok */
    assert(eni_stim_safety_check(&safety, &params, 1000) == ENI_OK);
    eni_stim_safety_record(&safety, 1000);
    /* Too soon */
    assert(eni_stim_safety_check(&safety, &params, 2000) == ENI_ERR_PERMISSION);
    /* After interval */
    assert(eni_stim_safety_check(&safety, &params, 7000) == ENI_OK);
    PASS("safety_rate_limit");
}

static void test_safety_daily_count(void) {
    eni_stim_safety_t safety;
    eni_stim_safety_init(&safety, 1.5f, 300000, 0, 3);
    eni_stim_params_t params = {0};
    params.amplitude = 0.5f;
    params.duration_ms = 100;
    for (int i = 0; i < 3; i++) {
        assert(eni_stim_safety_check(&safety, &params, 0) == ENI_OK);
        eni_stim_safety_record(&safety, 0);
    }
    assert(eni_stim_safety_check(&safety, &params, 0) == ENI_ERR_PERMISSION);
    eni_stim_safety_reset_daily(&safety);
    assert(eni_stim_safety_check(&safety, &params, 0) == ENI_OK);
    PASS("safety_daily_count");
}

static void test_safety_check_nan_amplitude(void) {
    eni_stim_safety_t safety;
    eni_stim_safety_init(&safety, 1.5f, 300000, 0, 0);
    eni_stim_params_t params = {0};
    params.amplitude = NAN; /* every `>` comparison is false for NaN */
    params.duration_ms = 100;
    assert(eni_stim_safety_check(&safety, &params, 0) == ENI_ERR_PERMISSION);
    PASS("safety_check_nan_amplitude");
}

static void test_safety_check_infinite_amplitude(void) {
    eni_stim_safety_t safety;
    eni_stim_safety_init(&safety, 1.5f, 300000, 0, 0);
    eni_stim_params_t params = {0};
    params.duration_ms = 100;
    params.amplitude = INFINITY;
    assert(eni_stim_safety_check(&safety, &params, 0) == ENI_ERR_PERMISSION);
    params.amplitude = -INFINITY;
    assert(eni_stim_safety_check(&safety, &params, 0) == ENI_ERR_PERMISSION);
    PASS("safety_check_infinite_amplitude");
}

static void test_safety_check_negative_amplitude(void) {
    eni_stim_safety_t safety;
    eni_stim_safety_init(&safety, 1.5f, 300000, 0, 0);
    eni_stim_params_t params = {0};
    params.amplitude = -0.5f; /* below every cap: passed the gate before F3-1 */
    params.duration_ms = 100;
    assert(eni_stim_safety_check(&safety, &params, 0) == ENI_ERR_PERMISSION);
    PASS("safety_check_negative_amplitude");
}

static void test_safety_check_zero_amplitude_ok(void) {
    eni_stim_safety_t safety;
    eni_stim_safety_init(&safety, 1.5f, 300000, 0, 0);
    eni_stim_params_t params = {0};
    params.amplitude = 0.0f; /* finite and non-negative: the gate must not over-refuse */
    params.duration_ms = 100;
    assert(eni_stim_safety_check(&safety, &params, 0) == ENI_OK);
    PASS("safety_check_zero_amplitude_ok");
}

static void test_safety_init_nan_max_refuses(void) {
    /* A NaN configured ceiling must not disable the configured limit:
     * `x > NaN` is false for every x, so a NaN max_amplitude used to let
     * everything up to the 2.0 mA absolute ceiling through. */
    eni_stim_safety_t safety;
    eni_stim_safety_init(&safety, NAN, 300000, 0, 0);
    eni_stim_params_t params = {0};
    params.amplitude = 1.9f;
    params.duration_ms = 100;
    assert(eni_stim_safety_check(&safety, &params, 0) == ENI_ERR_PERMISSION);
    PASS("safety_init_nan_max_refuses");
}

static void test_safety_init_nan_max_stores_zero(void) {
    /* Pins the init clamp itself: the check-side `!(a <= max)` would
     * refuse a NaN ceiling anyway, so only this assertion notices if
     * init stops replacing NaN with 0 mA. */
    eni_stim_safety_t safety;
    eni_stim_safety_init(&safety, NAN, 300000, 0, 0);
    assert(!isnan(safety.max_amplitude));
    assert(safety.max_amplitude == 0.0f);
    PASS("safety_init_nan_max_stores_zero");
}

static void test_safety_init_inf_max_clamps_absolute(void) {
    /* +Inf is clamped to the absolute ceiling, not treated like NaN. */
    eni_stim_safety_t safety;
    eni_stim_safety_init(&safety, INFINITY, 300000, 0, 0);
    assert(safety.max_amplitude == ENI_STIM_ABSOLUTE_MAX_AMPLITUDE);
    PASS("safety_init_inf_max_clamps_absolute");
}

static void test_safety_check_nan_configured_max_refuses(void) {
    /* The struct is public; a NaN written to max_amplitude after init
     * must also fail closed at the gate. */
    eni_stim_safety_t safety;
    eni_stim_safety_init(&safety, 1.0f, 300000, 0, 0);
    safety.max_amplitude = NAN;
    eni_stim_params_t params = {0};
    params.amplitude = 1.9f;
    params.duration_ms = 100;
    assert(eni_stim_safety_check(&safety, &params, 0) == ENI_ERR_PERMISSION);
    PASS("safety_check_nan_configured_max_refuses");
}

int main(void) {
    printf("=== ENI Stim Safety Tests ===\n");
    test_safety_init();
    test_safety_clamp_absolute();
    test_safety_check_within_limits();
    test_safety_check_amplitude_exceed();
    test_safety_check_absolute_exceed();
    test_safety_rate_limit();
    test_safety_daily_count();
    test_safety_check_nan_amplitude();
    test_safety_check_infinite_amplitude();
    test_safety_check_negative_amplitude();
    test_safety_check_zero_amplitude_ok();
    test_safety_init_nan_max_refuses();
    test_safety_init_nan_max_stores_zero();
    test_safety_init_inf_max_clamps_absolute();
    test_safety_check_nan_configured_max_refuses();
    printf("\n=== ALL %d TESTS PASSED ===\n", passed);
    return 0;
}
