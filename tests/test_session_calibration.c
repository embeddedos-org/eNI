// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Tests for session, calibration, and profile modules

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "eni/session.h"
#include "eni/profile.h"
#include "eni/calibration.h"
#include "eni/transfer.h"

static int tests_run = 0, tests_passed = 0, tests_failed = 0;
#define TEST(name) do { tests_run++; printf("  TEST %-40s ", #name); } while(0)
#define PASS() do { tests_passed++; printf("[PASS]\n"); } while(0)
#define FAIL(msg) do { tests_failed++; printf("[FAIL] %s\n", msg); } while(0)

static int state_change_count = 0;
static void on_state_change(eni_session_state_t old, eni_session_state_t new, void *data)
{
    (void)old; (void)new; (void)data;
    state_change_count++;
}

static void test_session_lifecycle(void)
{
    TEST(session_lifecycle);
    eni_session_t session;
    assert(eni_session_init(&session) == ENI_OK);
    assert(eni_session_get_state(&session) == ENI_SESSION_IDLE);

    eni_session_set_callback(&session, on_state_change, NULL);
    state_change_count = 0;

    assert(eni_session_set_subject(&session, "subject-001") == ENI_OK);
    assert(eni_session_set_description(&session, "Test session") == ENI_OK);
    assert(eni_session_set_meta(&session, "paradigm", "P300") == ENI_OK);

    assert(strcmp(eni_session_get_meta(&session, "paradigm"), "P300") == 0);

    assert(eni_session_start_calibration(&session) == ENI_OK);
    assert(eni_session_get_state(&session) == ENI_SESSION_CALIBRATING);

    assert(eni_session_start(&session) == ENI_OK);
    assert(eni_session_get_state(&session) == ENI_SESSION_RUNNING);

    assert(eni_session_pause(&session) == ENI_OK);
    assert(eni_session_get_state(&session) == ENI_SESSION_PAUSED);

    assert(eni_session_resume(&session) == ENI_OK);
    assert(eni_session_get_state(&session) == ENI_SESSION_RUNNING);

    assert(eni_session_stop(&session) == ENI_OK);
    assert(eni_session_get_state(&session) == ENI_SESSION_STOPPED);

    assert(state_change_count == 5);
    eni_session_destroy(&session);
    PASS();
}

static void test_profile_save_load(void)
{
    TEST(profile_save_load);
    const char *path = "/tmp/eni_test_profile.json";

    eni_profile_t profile;
    assert(eni_profile_init(&profile, "user-42") == ENI_OK);
    strncpy(profile.name, "Test User", sizeof(profile.name) - 1);
    profile.num_classes = 4;
    profile.confidence_threshold = 0.85f;

    eni_profile_set_channel_cal(&profile, 0, 1.5f, -0.1f, 15.0f);
    eni_profile_set_channel_cal(&profile, 1, 1.3f, 0.05f, 20.0f);
    eni_profile_update_session(&profile, 60000);

    assert(eni_profile_save(&profile, path) == ENI_OK);

    eni_profile_t loaded;
    assert(eni_profile_load(&loaded, path) == ENI_OK);
    assert(strcmp(loaded.user_id, "user-42") == 0);
    assert(loaded.num_classes == 4);
    assert(loaded.total_sessions == 1);
    assert(loaded.valid);
    PASS();
}

static void test_calibration_pipeline(void)
{
    TEST(calibration_pipeline);
    eni_calibration_t cal;
    assert(eni_calibration_init(&cal, 4, 256.0f) == ENI_OK);

    /* Stage 1: Impedance */
    assert(eni_calibration_start_impedance(&cal) == ENI_OK);
    float impedances[] = {10.0f, 15.0f, 20.0f, 12.0f};
    assert(eni_calibration_submit_impedance(&cal, impedances, 4) == ENI_OK);

    /* Stage 2: Baseline */
    assert(eni_calibration_start_baseline(&cal) == ENI_OK);
    for (int s = 0; s < 100; s++) {
        float samples[] = {0.1f, 0.2f, 0.15f, 0.12f};
        assert(eni_calibration_push_baseline_sample(&cal, samples, 4) == ENI_OK);
    }
    assert(eni_calibration_finalize_baseline(&cal) == ENI_OK);

    /* Stage 3: Thresholds */
    assert(eni_calibration_compute_thresholds(&cal, 0.95f) == ENI_OK);
    assert(eni_calibration_get_stage(&cal) == ENI_CAL_THRESHOLD);

    /* Stage 4: Validation */
    assert(eni_calibration_start_validation(&cal) == ENI_OK);
    for (int t = 0; t < 10; t++) {
        eni_calibration_submit_trial(&cal, t < 8); /* 80% accuracy */
    }
    assert(eni_calibration_get_accuracy(&cal) >= 0.7f);

    eni_profile_t profile;
    eni_profile_init(&profile, "cal-user");
    assert(eni_calibration_finalize(&cal, &profile) == ENI_OK);
    assert(eni_calibration_get_stage(&cal) == ENI_CAL_COMPLETE);
    assert(profile.num_channels == 4);

    eni_calibration_destroy(&cal);
    PASS();
}

static void test_transfer_learning(void)
{
    TEST(transfer_learning);
    eni_transfer_t xfer;
    assert(eni_transfer_init(&xfer, 5) == ENI_OK);

    /* Source data */
    float src_data[50]; /* 10 samples x 5 features */
    for (int i = 0; i < 50; i++) src_data[i] = (float)i * 0.1f;
    assert(eni_transfer_fit_source(&xfer, src_data, 10) == ENI_OK);

    /* Target data — different distribution */
    float tgt_data[50];
    for (int i = 0; i < 50; i++) tgt_data[i] = (float)i * 0.2f + 1.0f;
    assert(eni_transfer_fit_target(&xfer, tgt_data, 10) == ENI_OK);

    assert(eni_transfer_compute_alignment(&xfer) == ENI_OK);
    assert(xfer.fitted);

    float input[5] = {0.5f, 1.0f, 1.5f, 2.0f, 2.5f};
    float output[5];
    assert(eni_transfer_apply(&xfer, input, output, 5) == ENI_OK);
    /* Output should be in target distribution space */
    PASS();
}

int main(void)
{
    printf("=== Session & Calibration Tests ===\n");
    test_session_lifecycle();
    test_profile_save_load();
    test_calibration_pipeline();
    test_transfer_learning();
    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return tests_failed > 0 ? 1 : 0;
}
