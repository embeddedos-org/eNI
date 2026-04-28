// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
//
// Integration test: End-to-end calibration + session workflow
//
// Simulates a complete BCI session lifecycle:
//   1. Create session and set metadata
//   2. Run 4-stage calibration pipeline (impedance → baseline → threshold → validation)
//   3. Save calibrated profile to disk
//   4. Transition session through CALIBRATING → RUNNING → PAUSED → RUNNING → STOPPED
//   5. Reload profile from disk and verify persistence
//   6. Use transfer learning to align a new session to the saved profile
//   7. Verify all state transitions, timing, and data integrity

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "eni/session.h"
#include "eni/calibration.h"
#include "eni/profile.h"
#include "eni/transfer.h"
#include "eni/config.h"
#include "eni/log.h"
#include "eni_platform/platform.h"

/* ── Test Harness ──────────────────────────────────────────────────── */

static int tests_run = 0, tests_passed = 0, tests_failed = 0;

#define TEST(name) do { tests_run++; printf("  TEST %-50s ", #name); } while(0)
#define PASS()     do { tests_passed++; printf("[PASS]\n"); } while(0)
#define FAIL(msg)  do { tests_failed++; printf("[FAIL] %s\n", msg); } while(0)

#define ASSERT_OK(expr) do { \
    eni_status_t _rc = (expr); \
    if (_rc != ENI_OK) { \
        printf("[FAIL] %s returned %d\n", #expr, _rc); \
        tests_failed++; tests_run++; return; \
    } \
} while(0)

/* ── Callback Tracking ─────────────────────────────────────────────── */

#define MAX_TRANSITIONS 32

static struct {
    eni_session_state_t from;
    eni_session_state_t to;
} g_transitions[MAX_TRANSITIONS];
static int g_transition_count = 0;

static void session_state_callback(eni_session_state_t old_state,
                                    eni_session_state_t new_state,
                                    void *user_data)
{
    (void)user_data;
    if (g_transition_count < MAX_TRANSITIONS) {
        g_transitions[g_transition_count].from = old_state;
        g_transitions[g_transition_count].to   = new_state;
        g_transition_count++;
    }
}

static struct {
    eni_cal_stage_t stage;
    float           progress;
} g_cal_progress[64];
static int g_cal_progress_count = 0;

static void calibration_progress_callback(eni_cal_stage_t stage, float progress,
                                           void *user_data)
{
    (void)user_data;
    if (g_cal_progress_count < 64) {
        g_cal_progress[g_cal_progress_count].stage    = stage;
        g_cal_progress[g_cal_progress_count].progress = progress;
        g_cal_progress_count++;
    }
}

/* ── Simulated Neural Data ─────────────────────────────────────────── */

static float simulate_eeg_sample(int channel, int sample_idx)
{
    /* Simulate a simple sine wave with channel-dependent frequency and noise */
    float freq = 10.0f + (float)channel * 2.0f; /* 10-24 Hz range */
    float t = (float)sample_idx / 256.0f;        /* 256 Hz sample rate */
    float signal = 50.0f * sinf(2.0f * 3.14159f * freq * t);
    /* Add small deterministic "noise" based on sample index */
    float noise = (float)((sample_idx * 7 + channel * 13) % 100) / 100.0f * 5.0f;
    return signal + noise;
}

/* ── Test: Full End-to-End Workflow ─────────────────────────────────── */

static void test_e2e_calibration_session_workflow(void)
{
    TEST(e2e_calibration_session_workflow);

    const int NUM_CHANNELS = 8;
    const float SAMPLE_RATE = 256.0f;
    const char *PROFILE_PATH = "/tmp/eni_e2e_test_profile.json";

    /* ── Step 1: Initialize platform and session ── */

    eni_platform_init();

    eni_session_t session;
    ASSERT_OK(eni_session_init(&session));

    g_transition_count = 0;
    ASSERT_OK(eni_session_set_callback(&session, session_state_callback, NULL));
    ASSERT_OK(eni_session_set_subject(&session, "e2e-subject-001"));
    ASSERT_OK(eni_session_set_description(&session, "End-to-end integration test"));
    ASSERT_OK(eni_session_set_meta(&session, "paradigm", "motor_imagery"));
    ASSERT_OK(eni_session_set_meta(&session, "device", "simulated_eeg"));
    ASSERT_OK(eni_session_set_meta(&session, "channels", "8"));

    assert(eni_session_get_state(&session) == ENI_SESSION_IDLE);
    assert(strcmp(eni_session_get_meta(&session, "paradigm"), "motor_imagery") == 0);
    assert(strcmp(eni_session_get_meta(&session, "device"), "simulated_eeg") == 0);

    /* ── Step 2: Transition to calibration ── */

    ASSERT_OK(eni_session_start_calibration(&session));
    assert(eni_session_get_state(&session) == ENI_SESSION_CALIBRATING);

    /* ── Step 3: Run 4-stage calibration ── */

    eni_calibration_t cal;
    ASSERT_OK(eni_calibration_init(&cal, NUM_CHANNELS, SAMPLE_RATE));

    g_cal_progress_count = 0;
    ASSERT_OK(eni_calibration_set_callback(&cal, calibration_progress_callback, NULL));

    /* Stage 1: Impedance check */
    ASSERT_OK(eni_calibration_start_impedance(&cal));
    assert(eni_calibration_get_stage(&cal) == ENI_CAL_IMPEDANCE_CHECK);

    float impedances[8] = {12.0f, 15.0f, 8.0f, 20.0f, 11.0f, 14.0f, 9.0f, 18.0f};
    ASSERT_OK(eni_calibration_submit_impedance(&cal, impedances, NUM_CHANNELS));

    /* Verify all impedances are under threshold (default 50 kOhm) */
    for (int i = 0; i < NUM_CHANNELS; i++) {
        assert(impedances[i] < 50.0f);
    }

    /* Stage 2: Baseline recording (simulate 30s × 256 Hz = 7680 samples) */
    ASSERT_OK(eni_calibration_start_baseline(&cal));
    assert(eni_calibration_get_stage(&cal) == ENI_CAL_BASELINE);

    int baseline_total = (int)(SAMPLE_RATE * ENI_CAL_BASELINE_SECONDS);
    for (int s = 0; s < baseline_total; s++) {
        float samples[8];
        for (int ch = 0; ch < NUM_CHANNELS; ch++) {
            samples[ch] = simulate_eeg_sample(ch, s);
        }
        ASSERT_OK(eni_calibration_push_baseline_sample(&cal, samples, NUM_CHANNELS));
    }

    ASSERT_OK(eni_calibration_finalize_baseline(&cal));

    /* Verify baseline statistics are reasonable */
    assert(cal.baseline_samples == baseline_total);
    for (int ch = 0; ch < NUM_CHANNELS; ch++) {
        /* Mean should be near 0 for a sine wave (slight offset from "noise") */
        assert(fabsf(cal.baseline_mean[ch]) < 50.0f);
        /* Std should be positive */
        assert(cal.baseline_std[ch] > 0.0f);
    }

    /* Stage 3: Threshold computation */
    ASSERT_OK(eni_calibration_compute_thresholds(&cal, 0.95f));
    assert(eni_calibration_get_stage(&cal) == ENI_CAL_THRESHOLD);

    for (int ch = 0; ch < NUM_CHANNELS; ch++) {
        /* Threshold should be mean + z*std, so > mean */
        assert(cal.thresholds[ch] > cal.baseline_mean[ch]);
    }

    /* Stage 4: Validation (simulate 20 trials, 85% accuracy) */
    ASSERT_OK(eni_calibration_start_validation(&cal));
    assert(eni_calibration_get_stage(&cal) == ENI_CAL_VALIDATION);

    for (int trial = 0; trial < 20; trial++) {
        bool correct = (trial % 20) < 17; /* 17/20 = 85% */
        ASSERT_OK(eni_calibration_submit_trial(&cal, correct));
    }

    float accuracy = eni_calibration_get_accuracy(&cal);
    assert(accuracy >= 0.80f);
    assert(accuracy <= 0.90f);

    /* ── Step 4: Finalize calibration → create profile ── */

    eni_profile_t profile;
    ASSERT_OK(eni_profile_init(&profile, "e2e-subject-001"));
    strncpy(profile.name, "E2E Test Subject", sizeof(profile.name) - 1);
    strncpy(profile.device_id, "sim-eeg-001", sizeof(profile.device_id) - 1);
    profile.num_classes = 4;
    profile.confidence_threshold = 0.85f;

    ASSERT_OK(eni_calibration_finalize(&cal, &profile));
    assert(eni_calibration_get_stage(&cal) == ENI_CAL_COMPLETE);

    /* Verify calibration data was transferred to profile */
    assert(profile.num_channels == NUM_CHANNELS);
    for (int ch = 0; ch < NUM_CHANNELS; ch++) {
        assert(profile.channel_gains[ch] > 0.0f);
        assert(profile.impedance_baseline[ch] == impedances[ch]);
    }

    /* ── Step 5: Save profile to disk ── */

    ASSERT_OK(eni_profile_save(&profile, PROFILE_PATH));

    /* ── Step 6: Transition session through lifecycle ── */

    ASSERT_OK(eni_session_start(&session));
    assert(eni_session_get_state(&session) == ENI_SESSION_RUNNING);

    /* Simulate some processing time */
    eni_platform_sleep_ms(50);

    ASSERT_OK(eni_session_pause(&session));
    assert(eni_session_get_state(&session) == ENI_SESSION_PAUSED);

    ASSERT_OK(eni_session_resume(&session));
    assert(eni_session_get_state(&session) == ENI_SESSION_RUNNING);

    /* Simulate more processing */
    eni_platform_sleep_ms(50);

    /* Record session duration in profile */
    uint64_t elapsed = eni_session_elapsed_ms(&session);
    assert(elapsed >= 50); /* At least 50ms should have passed */

    ASSERT_OK(eni_session_stop(&session));
    assert(eni_session_get_state(&session) == ENI_SESSION_STOPPED);

    eni_profile_update_session(&profile, elapsed);
    assert(profile.total_sessions == 1);
    assert(profile.total_time_ms >= 50);

    /* Save updated profile */
    ASSERT_OK(eni_profile_save(&profile, PROFILE_PATH));

    /* ── Step 7: Verify state transition history ── */

    assert(g_transition_count == 5);
    assert(g_transitions[0].from == ENI_SESSION_IDLE);
    assert(g_transitions[0].to   == ENI_SESSION_CALIBRATING);
    assert(g_transitions[1].from == ENI_SESSION_CALIBRATING);
    assert(g_transitions[1].to   == ENI_SESSION_RUNNING);
    assert(g_transitions[2].from == ENI_SESSION_RUNNING);
    assert(g_transitions[2].to   == ENI_SESSION_PAUSED);
    assert(g_transitions[3].from == ENI_SESSION_PAUSED);
    assert(g_transitions[3].to   == ENI_SESSION_RUNNING);
    assert(g_transitions[4].from == ENI_SESSION_RUNNING);
    assert(g_transitions[4].to   == ENI_SESSION_STOPPED);

    /* ── Step 8: Verify calibration progress callbacks ── */

    assert(g_cal_progress_count > 0);
    /* Should have: impedance progress, many baseline progress, threshold, validation */
    bool saw_impedance = false, saw_baseline = false, saw_threshold = false, saw_validation = false;
    for (int i = 0; i < g_cal_progress_count; i++) {
        switch (g_cal_progress[i].stage) {
        case ENI_CAL_IMPEDANCE_CHECK: saw_impedance = true; break;
        case ENI_CAL_BASELINE:        saw_baseline  = true; break;
        case ENI_CAL_THRESHOLD:       saw_threshold = true; break;
        case ENI_CAL_VALIDATION:      saw_validation = true; break;
        default: break;
        }
    }
    assert(saw_impedance);
    assert(saw_baseline);
    assert(saw_threshold);
    assert(saw_validation);

    /* ── Cleanup ── */

    eni_calibration_destroy(&cal);
    eni_session_destroy(&session);

    PASS();
}

/* ── Test: Profile Persistence Round-Trip ──────────────────────────── */

static void test_profile_persistence_roundtrip(void)
{
    TEST(profile_persistence_roundtrip);

    const char *PROFILE_PATH = "/tmp/eni_e2e_test_profile.json";

    /* Load the profile saved by the previous test */
    eni_profile_t loaded;
    ASSERT_OK(eni_profile_load(&loaded, PROFILE_PATH));

    assert(loaded.valid);
    assert(strcmp(loaded.user_id, "e2e-subject-001") == 0);
    assert(strcmp(loaded.name, "E2E Test Subject") == 0);
    assert(strcmp(loaded.device_id, "sim-eeg-001") == 0);
    assert(loaded.num_classes == 4);
    assert(loaded.num_channels == 8);
    assert(loaded.total_sessions == 1);
    assert(loaded.total_time_ms >= 50);
    assert(fabsf(loaded.confidence_threshold - 0.85f) < 0.01f);

    /* Verify calibration gains were persisted */
    for (int ch = 0; ch < loaded.num_channels; ch++) {
        assert(loaded.channel_gains[ch] > 0.0f);
    }

    PASS();
}

/* ── Test: Transfer Learning Between Sessions ──────────────────────── */

static void test_transfer_learning_cross_session(void)
{
    TEST(transfer_learning_cross_session);

    const int N_FEATURES = 5;
    const int N_SAMPLES = 100;

    /* Simulate "source" session features (calibrated session) */
    float src_features[500]; /* 100 samples × 5 features */
    for (int s = 0; s < N_SAMPLES; s++) {
        for (int f = 0; f < N_FEATURES; f++) {
            src_features[s * N_FEATURES + f] =
                sinf((float)s * 0.1f + (float)f) * (10.0f + (float)f * 5.0f);
        }
    }

    /* Simulate "target" session features (new session, different distribution) */
    float tgt_features[500];
    for (int s = 0; s < N_SAMPLES; s++) {
        for (int f = 0; f < N_FEATURES; f++) {
            tgt_features[s * N_FEATURES + f] =
                sinf((float)s * 0.1f + (float)f) * (20.0f + (float)f * 3.0f) + 5.0f;
        }
    }

    /* Fit transfer learning model */
    eni_transfer_t xfer;
    ASSERT_OK(eni_transfer_init(&xfer, N_FEATURES));
    ASSERT_OK(eni_transfer_fit_source(&xfer, src_features, N_SAMPLES));
    ASSERT_OK(eni_transfer_fit_target(&xfer, tgt_features, N_SAMPLES));
    ASSERT_OK(eni_transfer_compute_alignment(&xfer));

    assert(xfer.fitted);

    /* Apply transform to a source-domain sample */
    float input[5], output[5];
    for (int f = 0; f < N_FEATURES; f++) {
        input[f] = src_features[f]; /* First source sample */
    }

    ASSERT_OK(eni_transfer_apply(&xfer, input, output, N_FEATURES));

    /* Output should be in target distribution space:
     * Verify output mean is closer to target mean than source mean */
    float src_dist = 0.0f, tgt_dist = 0.0f;
    for (int f = 0; f < N_FEATURES; f++) {
        float ds = output[f] - xfer.src_mean[f];
        float dt = output[f] - xfer.tgt_mean[f];
        src_dist += ds * ds;
        tgt_dist += dt * dt;
    }
    /* Transformed output should be closer to target distribution */
    assert(tgt_dist < src_dist);

    PASS();
}

/* ── Test: Session Error Handling ──────────────────────────────────── */

static void test_session_invalid_transitions(void)
{
    TEST(session_invalid_transitions);

    eni_session_t session;
    ASSERT_OK(eni_session_init(&session));

    /* Can't pause from IDLE */
    assert(eni_session_pause(&session) == ENI_ERR_RUNTIME);

    /* Can't resume from IDLE */
    assert(eni_session_resume(&session) == ENI_ERR_RUNTIME);

    /* Start calibration from IDLE — OK */
    ASSERT_OK(eni_session_start_calibration(&session));

    /* Can't start calibration again while calibrating */
    assert(eni_session_start_calibration(&session) == ENI_ERR_RUNTIME);

    /* Can't pause while calibrating */
    assert(eni_session_pause(&session) == ENI_ERR_RUNTIME);

    /* Transition to running */
    ASSERT_OK(eni_session_start(&session));

    /* Can't resume while already running */
    assert(eni_session_resume(&session) == ENI_ERR_RUNTIME);

    /* Pause and try invalid double-pause */
    ASSERT_OK(eni_session_pause(&session));
    assert(eni_session_pause(&session) == ENI_ERR_RUNTIME);

    /* Resume and stop */
    ASSERT_OK(eni_session_resume(&session));
    ASSERT_OK(eni_session_stop(&session));

    /* Can't start again after stopped */
    assert(eni_session_start(&session) == ENI_ERR_RUNTIME);

    eni_session_destroy(&session);
    PASS();
}

/* ── Test: Calibration Failure (Low Accuracy) ──────────────────────── */

static void test_calibration_failure_low_accuracy(void)
{
    TEST(calibration_failure_low_accuracy);

    eni_calibration_t cal;
    ASSERT_OK(eni_calibration_init(&cal, 4, 256.0f));

    /* Rush through to validation */
    ASSERT_OK(eni_calibration_start_impedance(&cal));
    float imp[4] = {10, 10, 10, 10};
    ASSERT_OK(eni_calibration_submit_impedance(&cal, imp, 4));

    ASSERT_OK(eni_calibration_start_baseline(&cal));
    for (int s = 0; s < 100; s++) {
        float samp[4] = {1.0f, 2.0f, 1.5f, 1.2f};
        ASSERT_OK(eni_calibration_push_baseline_sample(&cal, samp, 4));
    }
    ASSERT_OK(eni_calibration_finalize_baseline(&cal));
    ASSERT_OK(eni_calibration_compute_thresholds(&cal, 0.95f));

    /* Validation with 40% accuracy (below 60% threshold) */
    ASSERT_OK(eni_calibration_start_validation(&cal));
    for (int t = 0; t < 10; t++) {
        eni_calibration_submit_trial(&cal, t < 4); /* 4/10 = 40% */
    }

    /* Finalize should fail due to low accuracy */
    eni_profile_t profile;
    eni_profile_init(&profile, "fail-user");
    eni_status_t rc = eni_calibration_finalize(&cal, &profile);
    assert(rc == ENI_ERR_RUNTIME);
    assert(eni_calibration_get_stage(&cal) == ENI_CAL_FAILED);

    eni_calibration_destroy(&cal);
    PASS();
}

/* ── Test: Session Metadata Overflow ───────────────────────────────── */

static void test_session_metadata_limits(void)
{
    TEST(session_metadata_limits);

    eni_session_t session;
    ASSERT_OK(eni_session_init(&session));

    /* Fill all metadata slots */
    for (int i = 0; i < ENI_SESSION_MAX_META; i++) {
        char key[32], value[64];
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(value, sizeof(value), "value_%d", i);
        ASSERT_OK(eni_session_set_meta(&session, key, value));
    }

    /* Next should overflow */
    assert(eni_session_set_meta(&session, "overflow_key", "overflow_val") == ENI_ERR_OVERFLOW);

    /* But updating an existing key should still work */
    ASSERT_OK(eni_session_set_meta(&session, "key_0", "updated_value"));
    assert(strcmp(eni_session_get_meta(&session, "key_0"), "updated_value") == 0);

    /* Non-existent key returns NULL */
    assert(eni_session_get_meta(&session, "nonexistent") == NULL);

    eni_session_destroy(&session);
    PASS();
}

/* ── Test: Config Load + Session Init Integration ──────────────────── */

static void test_config_driven_session(void)
{
    TEST(config_driven_session);

    /* Create a JSON config file */
    const char *cfg_path = "/tmp/eni_e2e_test_config.json";
    FILE *fp = fopen(cfg_path, "w");
    assert(fp != NULL);
    fprintf(fp, "{\n");
    fprintf(fp, "  \"variant\": \"framework\",\n");
    fprintf(fp, "  \"mode\": \"features_intent\",\n");
    fprintf(fp, "  \"confidence_threshold\": 0.90,\n");
    fprintf(fp, "  \"dsp\": {\n");
    fprintf(fp, "    \"epoch_size\": 256,\n");
    fprintf(fp, "    \"sample_rate\": 512,\n");
    fprintf(fp, "    \"artifact_threshold\": 100.0\n");
    fprintf(fp, "  },\n");
    fprintf(fp, "  \"decoder\": {\n");
    fprintf(fp, "    \"num_classes\": 4,\n");
    fprintf(fp, "    \"confidence_threshold\": 0.85\n");
    fprintf(fp, "  }\n");
    fprintf(fp, "}\n");
    fclose(fp);

    /* Load config */
    eni_config_t config;
    ASSERT_OK(eni_config_load_file(&config, cfg_path));

    assert(config.variant == ENI_VARIANT_FRAMEWORK);
    assert(config.mode == ENI_MODE_FEATURES_INTENT);
    assert(fabsf(config.filter.min_confidence - 0.90f) < 0.01f);
    assert(config.dsp.epoch_size == 256);
    assert(config.dsp.sample_rate == 512);
    assert(config.decoder.num_classes == 4);

    /* Use config to initialize a session */
    eni_session_t session;
    ASSERT_OK(eni_session_init(&session));

    char epoch_str[16];
    snprintf(epoch_str, sizeof(epoch_str), "%u", config.dsp.epoch_size);
    ASSERT_OK(eni_session_set_meta(&session, "epoch_size", epoch_str));

    char rate_str[16];
    snprintf(rate_str, sizeof(rate_str), "%u", config.dsp.sample_rate);
    ASSERT_OK(eni_session_set_meta(&session, "sample_rate", rate_str));

    assert(strcmp(eni_session_get_meta(&session, "epoch_size"), "256") == 0);
    assert(strcmp(eni_session_get_meta(&session, "sample_rate"), "512") == 0);

    eni_session_destroy(&session);
    PASS();
}

/* ── Main ──────────────────────────────────────────────────────────── */

int main(void)
{
    /* Suppress log noise during tests */
    eni_log_set_level(ENI_LOG_WARN);

    printf("=== Integration Test: Calibration + Session Workflow ===\n\n");

    test_e2e_calibration_session_workflow();
    test_profile_persistence_roundtrip();
    test_transfer_learning_cross_session();
    test_session_invalid_transitions();
    test_calibration_failure_low_accuracy();
    test_session_metadata_limits();
    test_config_driven_session();

    printf("\n── Summary ─────────────────────────────────────────────\n");
    printf("  Total:  %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_failed);
    printf("─────────────────────────────────────────────────────────\n");

    if (tests_failed > 0) {
        printf("\n*** %d TEST(S) FAILED ***\n", tests_failed);
        return 1;
    }

    printf("\n=== ALL %d TESTS PASSED ===\n", tests_passed);
    return 0;
}
