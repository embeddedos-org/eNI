// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
#include <stdio.h>
#include <string.h>
#include <assert.h>
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

int main(void) {
    printf("=== ENI Stim Safety Tests ===\n");
    test_safety_init();
    test_safety_clamp_absolute();
    test_safety_check_within_limits();
    test_safety_check_amplitude_exceed();
    test_safety_check_absolute_exceed();
    test_safety_rate_limit();
    test_safety_daily_count();
    printf("\n=== ALL %d TESTS PASSED ===\n", passed);
    return 0;
}
