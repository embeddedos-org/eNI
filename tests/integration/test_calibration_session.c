// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Integration test: End-to-end calibration + session workflow

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

static int tests_run = 0, tests_passed = 0, tests_failed = 0;

#define TEST(name) do { tests_run++; printf("  TEST %-50s ", #name); } while(0)
#define PASS()     do { tests_passed++; printf("[PASS]\n"); } while(0)
#define FAIL(msg)  do { tests_failed++; printf("[FAIL] %s\n", msg); return; } while(0)

#define ASSERT_OK(expr) do { \
    eni_status_t _rc = (expr); \
    if (_rc != ENI_OK) { \
        printf("[FAIL] %s returned %d\n", #expr, _rc); \
        tests_failed++; tests_run++; return; \
    } \
} while(0)

#ifdef _WIN32
#define TEST_PROFILE_PATH  "eni_e2e_test_profile.json"
#define TEST_CONFIG_PATH   "eni_e2e_test_config.json"
#else
#define TEST_PROFILE_PATH  "/tmp/eni_e2e_test_profile.json"
#define TEST_CONFIG_PATH   "/tmp/eni_e2e_test_config.json"
#endif

/* Callback tracking */
#define MAX_TRANSITIONS 32
static struct { int from; int to; } g_transitions[MAX_TRANSITIONS];
static int g_transition_count = 0;

static void session_state_cb(eni_session_state_t old_s, eni_session_state_t new_s, void *ud)
{
    (void)ud;
    if (g_transition_count < MAX_TRANSITIONS) {
        g_transitions[g_transition_count].from = (int)old_s;
        g_transitions[g_transition_count].to   = (int)new_s;
        g_transition_count++;
    }
}

static int g_saw_imp = 0, g_saw_base = 0, g_saw_thr = 0, g_saw_val = 0, g_cal_cb_count = 0;

static void cal_progress_cb(eni_cal_stage_t stage, float progress, void *ud)
{
    (void)progress; (void)ud;
    g_cal_cb_count++;
    if (stage == ENI_CAL_IMPEDANCE_CHECK) g_saw_imp = 1;
    if (stage == ENI_CAL_BASELINE)        g_saw_base = 1;
    if (stage == ENI_CAL_THRESHOLD)       g_saw_thr = 1;
    if (stage == ENI_CAL_VALIDATION)      g_saw_val = 1;
}

static float sim_sample(int ch, int s)
{
    float freq = 10.0f + (float)ch * 2.0f;
    float t = (float)s / 256.0f;
    return 50.0f * sinf(2.0f * 3.14159f * freq * t) +
           (float)((s * 7 + ch * 13) % 100) / 100.0f * 5.0f;
}

/* All large structs are static to avoid Windows stack overflow */
static eni_session_t     s_session;
static eni_calibration_t s_cal;
static eni_profile_t     s_profile;
static eni_profile_t     s_loaded;
static eni_transfer_t    s_xfer;
static eni_session_t     s_session2;
static eni_session_t     s_session3;
static eni_session_t     s_session4;
static eni_calibration_t s_cal2;
static eni_profile_t     s_profile2;
static eni_config_t      s_config;
static eni_session_t     s_session5;

static void test_e2e_workflow(void)
{
    TEST(e2e_calibration_session_workflow);

    eni_platform_init();
    memset(&s_session, 0, sizeof(s_session));
    ASSERT_OK(eni_session_init(&s_session));
    g_transition_count = 0;
    ASSERT_OK(eni_session_set_callback(&s_session, session_state_cb, NULL));
    ASSERT_OK(eni_session_set_subject(&s_session, "e2e-subject-001"));
    ASSERT_OK(eni_session_set_description(&s_session, "End-to-end integration test"));
    ASSERT_OK(eni_session_set_meta(&s_session, "paradigm", "motor_imagery"));
    assert(eni_session_get_state(&s_session) == ENI_SESSION_IDLE);

    ASSERT_OK(eni_session_start_calibration(&s_session));
    assert(eni_session_get_state(&s_session) == ENI_SESSION_CALIBRATING);

    /* Calibration */
    memset(&s_cal, 0, sizeof(s_cal));
    ASSERT_OK(eni_calibration_init(&s_cal, 8, 256.0f));
    g_cal_cb_count = 0; g_saw_imp = g_saw_base = g_saw_thr = g_saw_val = 0;
    ASSERT_OK(eni_calibration_set_callback(&s_cal, cal_progress_cb, NULL));

    /* Stage 1 */
    ASSERT_OK(eni_calibration_start_impedance(&s_cal));
    float imp[8] = {12,15,8,20,11,14,9,18};
    ASSERT_OK(eni_calibration_submit_impedance(&s_cal, imp, 8));

    /* Stage 2 */
    ASSERT_OK(eni_calibration_start_baseline(&s_cal));
    for (int s = 0; s < 500; s++) {
        float samp[8];
        for (int ch = 0; ch < 8; ch++) samp[ch] = sim_sample(ch, s);
        ASSERT_OK(eni_calibration_push_baseline_sample(&s_cal, samp, 8));
    }
    ASSERT_OK(eni_calibration_finalize_baseline(&s_cal));

    /* Stage 3 */
    ASSERT_OK(eni_calibration_compute_thresholds(&s_cal, 0.95f));

    /* Stage 4 */
    ASSERT_OK(eni_calibration_start_validation(&s_cal));
    for (int t = 0; t < 20; t++)
        ASSERT_OK(eni_calibration_submit_trial(&s_cal, t < 17));
    assert(eni_calibration_get_accuracy(&s_cal) >= 0.80f);

    /* Finalize */
    memset(&s_profile, 0, sizeof(s_profile));
    ASSERT_OK(eni_profile_init(&s_profile, "e2e-subject-001"));
    s_profile.num_classes = 4;
    s_profile.confidence_threshold = 0.85f;
    ASSERT_OK(eni_calibration_finalize(&s_cal, &s_profile));
    assert(eni_calibration_get_stage(&s_cal) == ENI_CAL_COMPLETE);
    assert(s_profile.num_channels == 8);
    ASSERT_OK(eni_profile_save(&s_profile, TEST_PROFILE_PATH));

    /* Session lifecycle */
    ASSERT_OK(eni_session_start(&s_session));
    eni_platform_sleep_ms(20);
    ASSERT_OK(eni_session_pause(&s_session));
    ASSERT_OK(eni_session_resume(&s_session));
    eni_platform_sleep_ms(20);
    ASSERT_OK(eni_session_stop(&s_session));

    assert(g_transition_count == 5);
    assert(g_cal_cb_count > 0);
    assert(g_saw_imp && g_saw_base && g_saw_thr && g_saw_val);

    eni_calibration_destroy(&s_cal);
    eni_session_destroy(&s_session);
    PASS();
}

static void test_profile_roundtrip(void)
{
    TEST(profile_persistence_roundtrip);
    memset(&s_loaded, 0, sizeof(s_loaded));
    ASSERT_OK(eni_profile_load(&s_loaded, TEST_PROFILE_PATH));
    assert(s_loaded.valid);
    assert(strcmp(s_loaded.user_id, "e2e-subject-001") == 0);
    assert(s_loaded.num_channels == 8);
    PASS();
}

static void test_transfer_learning(void)
{
    TEST(transfer_learning_cross_session);
    float src[500], tgt[500];
    for (int i = 0; i < 500; i++) { src[i] = (float)i * 0.1f; tgt[i] = (float)i * 0.2f + 1.0f; }

    memset(&s_xfer, 0, sizeof(s_xfer));
    ASSERT_OK(eni_transfer_init(&s_xfer, 5));
    ASSERT_OK(eni_transfer_fit_source(&s_xfer, src, 100));
    ASSERT_OK(eni_transfer_fit_target(&s_xfer, tgt, 100));
    ASSERT_OK(eni_transfer_compute_alignment(&s_xfer));
    assert(s_xfer.fitted);

    float in[5] = {0.5f,1.0f,1.5f,2.0f,2.5f}, out[5];
    ASSERT_OK(eni_transfer_apply(&s_xfer, in, out, 5));
    PASS();
}

static void test_invalid_transitions(void)
{
    TEST(session_invalid_transitions);
    memset(&s_session2, 0, sizeof(s_session2));
    ASSERT_OK(eni_session_init(&s_session2));
    assert(eni_session_pause(&s_session2) == ENI_ERR_RUNTIME);
    assert(eni_session_resume(&s_session2) == ENI_ERR_RUNTIME);
    ASSERT_OK(eni_session_start_calibration(&s_session2));
    assert(eni_session_start_calibration(&s_session2) == ENI_ERR_RUNTIME);
    ASSERT_OK(eni_session_start(&s_session2));
    assert(eni_session_resume(&s_session2) == ENI_ERR_RUNTIME);
    ASSERT_OK(eni_session_pause(&s_session2));
    ASSERT_OK(eni_session_resume(&s_session2));
    ASSERT_OK(eni_session_stop(&s_session2));
    assert(eni_session_start(&s_session2) == ENI_ERR_RUNTIME);
    eni_session_destroy(&s_session2);
    PASS();
}

static void test_cal_failure(void)
{
    TEST(calibration_failure_low_accuracy);
    memset(&s_cal2, 0, sizeof(s_cal2));
    ASSERT_OK(eni_calibration_init(&s_cal2, 4, 256.0f));
    ASSERT_OK(eni_calibration_start_impedance(&s_cal2));
    float imp[4] = {10,10,10,10};
    ASSERT_OK(eni_calibration_submit_impedance(&s_cal2, imp, 4));
    ASSERT_OK(eni_calibration_start_baseline(&s_cal2));
    for (int s = 0; s < 100; s++) {
        float samp[4] = {1,2,1.5f,1.2f};
        ASSERT_OK(eni_calibration_push_baseline_sample(&s_cal2, samp, 4));
    }
    ASSERT_OK(eni_calibration_finalize_baseline(&s_cal2));
    ASSERT_OK(eni_calibration_compute_thresholds(&s_cal2, 0.95f));
    ASSERT_OK(eni_calibration_start_validation(&s_cal2));
    for (int t = 0; t < 10; t++) eni_calibration_submit_trial(&s_cal2, t < 4);
    memset(&s_profile2, 0, sizeof(s_profile2));
    eni_profile_init(&s_profile2, "fail");
    assert(eni_calibration_finalize(&s_cal2, &s_profile2) == ENI_ERR_RUNTIME);
    assert(eni_calibration_get_stage(&s_cal2) == ENI_CAL_FAILED);
    eni_calibration_destroy(&s_cal2);
    PASS();
}

static void test_metadata_limits(void)
{
    TEST(session_metadata_limits);
    memset(&s_session3, 0, sizeof(s_session3));
    ASSERT_OK(eni_session_init(&s_session3));
    for (int i = 0; i < ENI_SESSION_MAX_META; i++) {
        char key[32], val[64];
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(val, sizeof(val), "val_%d", i);
        ASSERT_OK(eni_session_set_meta(&s_session3, key, val));
    }
    assert(eni_session_set_meta(&s_session3, "over", "flow") == ENI_ERR_OVERFLOW);
    ASSERT_OK(eni_session_set_meta(&s_session3, "key_0", "updated"));
    assert(strcmp(eni_session_get_meta(&s_session3, "key_0"), "updated") == 0);
    assert(eni_session_get_meta(&s_session3, "nonexistent") == NULL);
    eni_session_destroy(&s_session3);
    PASS();
}

static void test_config_driven(void)
{
    TEST(config_driven_session);
    FILE *fp = fopen(TEST_CONFIG_PATH, "w");
    assert(fp != NULL);
    fprintf(fp, "{\"variant\":\"framework\",\"mode\":\"features_intent\","
                "\"confidence_threshold\":0.90,"
                "\"dsp\":{\"epoch_size\":256,\"sample_rate\":512},"
                "\"decoder\":{\"num_classes\":4}}\n");
    fclose(fp);

    memset(&s_config, 0, sizeof(s_config));
    ASSERT_OK(eni_config_load_file(&s_config, TEST_CONFIG_PATH));
    assert(s_config.variant == ENI_VARIANT_FRAMEWORK);
    assert(s_config.dsp.epoch_size == 256);

    memset(&s_session5, 0, sizeof(s_session5));
    ASSERT_OK(eni_session_init(&s_session5));
    char buf[16];
    snprintf(buf, sizeof(buf), "%u", s_config.dsp.epoch_size);
    ASSERT_OK(eni_session_set_meta(&s_session5, "epoch_size", buf));
    assert(strcmp(eni_session_get_meta(&s_session5, "epoch_size"), "256") == 0);
    eni_session_destroy(&s_session5);
    PASS();
}

int main(void)
{
    eni_log_set_level(ENI_LOG_WARN);
    printf("=== Integration Test: Calibration + Session Workflow ===\n\n");

    test_e2e_workflow();
    test_profile_roundtrip();
    test_transfer_learning();
    test_invalid_transitions();
    test_cal_failure();
    test_metadata_limits();
    test_config_driven();

    printf("\nTotal: %d  Passed: %d  Failed: %d\n", tests_run, tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
