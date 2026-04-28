// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Test: common/src/feedback.c — feedback controller

#include <stdio.h>
#include <string.h>
#include "eni/common.h"
#include "eni/feedback.h"

static int tests_run = 0, tests_passed = 0, tests_failed = 0;
#define TEST(name) do { tests_run++; printf("  TEST %-40s ", #name); } while(0)
#define PASS() do { tests_passed++; printf("[PASS]\n"); } while(0)
#define FAIL(msg) do { tests_failed++; printf("[FAIL] %s\n", msg); } while(0)

/* ── Mock stimulator ── */
static int g_stim_count = 0;
static eni_stim_params_t g_last_stim_params;

static eni_status_t mock_stim_init(eni_stimulator_t *stim, const void *config)
{
    (void)stim; (void)config;
    return ENI_OK;
}

static eni_status_t mock_stimulate(eni_stimulator_t *stim, const eni_stim_params_t *params)
{
    (void)stim;
    g_stim_count++;
    g_last_stim_params = *params;
    return ENI_OK;
}

static void mock_stim_shutdown(eni_stimulator_t *stim) { (void)stim; }

static const eni_stimulator_ops_t mock_stim_ops = {
    .name = "mock_stim",
    .init = mock_stim_init,
    .stimulate = mock_stimulate,
    .stop = NULL,
    .get_status = NULL,
    .shutdown = mock_stim_shutdown,
};

static eni_stimulator_t g_mock_stim = {
    .name = "mock",
    .ops = &mock_stim_ops,
    .ctx = NULL,
    .active = true,
};

/* ── Test: init feedback controller ── */
static void test_feedback_init(void)
{
    TEST(feedback_init);
    eni_feedback_controller_t ctrl;
    eni_status_t rc = eni_feedback_controller_init(&ctrl, &g_mock_stim, 1.0f, 500);
    if (rc != ENI_OK) { FAIL("init failed"); return; }
    if (!ctrl.enabled) { FAIL("should be enabled"); return; }
    if (ctrl.rule_count != 0) { FAIL("should have 0 rules"); return; }
    eni_feedback_controller_shutdown(&ctrl);
    PASS();
}

/* ── Test: init with NULL ── */
static void test_feedback_init_null(void)
{
    TEST(feedback_init_null);
    eni_status_t rc = eni_feedback_controller_init(NULL, &g_mock_stim, 1.0f, 500);
    if (rc != ENI_ERR_INVALID) { FAIL("should reject NULL"); return; }
    PASS();
}

/* ── Test: add rule ── */
static void test_feedback_add_rule(void)
{
    TEST(feedback_add_rule);
    eni_feedback_controller_t ctrl;
    eni_feedback_controller_init(&ctrl, &g_mock_stim, 1.0f, 500);

    eni_stim_params_t response = {
        .type = ENI_STIM_HAPTIC,
        .channel = 0,
        .amplitude = 0.5f,
        .duration_ms = 200,
        .frequency_hz = 50.0f,
    };

    eni_status_t rc = eni_feedback_controller_add_rule(&ctrl, "focus", 0.7f, &response);
    if (rc != ENI_OK) { FAIL("add_rule failed"); return; }
    if (ctrl.rule_count != 1) { FAIL("wrong rule count"); return; }
    if (strcmp(ctrl.rules[0].trigger_intent, "focus") != 0) { FAIL("wrong intent"); return; }
    eni_feedback_controller_shutdown(&ctrl);
    PASS();
}

/* ── Test: add rule invalid args ── */
static void test_feedback_add_rule_invalid(void)
{
    TEST(feedback_add_rule_invalid);
    eni_feedback_controller_t ctrl;
    eni_feedback_controller_init(&ctrl, &g_mock_stim, 1.0f, 500);
    eni_stim_params_t response = {0};

    eni_status_t rc = eni_feedback_controller_add_rule(NULL, "focus", 0.7f, &response);
    if (rc != ENI_ERR_INVALID) { FAIL("should reject NULL ctrl"); return; }
    rc = eni_feedback_controller_add_rule(&ctrl, NULL, 0.7f, &response);
    if (rc != ENI_ERR_INVALID) { FAIL("should reject NULL intent"); return; }
    rc = eni_feedback_controller_add_rule(&ctrl, "focus", 0.7f, NULL);
    if (rc != ENI_ERR_INVALID) { FAIL("should reject NULL response"); return; }
    eni_feedback_controller_shutdown(&ctrl);
    PASS();
}

/* ── Test: evaluate matching rule ── */
static void test_feedback_evaluate_match(void)
{
    TEST(feedback_evaluate_match);
    eni_feedback_controller_t ctrl;
    eni_feedback_controller_init(&ctrl, &g_mock_stim, 1.0f, 500);

    eni_stim_params_t response = {
        .type = ENI_STIM_HAPTIC, .channel = 0,
        .amplitude = 0.5f, .duration_ms = 200, .frequency_hz = 50.0f,
    };
    eni_feedback_controller_add_rule(&ctrl, "focus", 0.7f, &response);

    eni_event_t intent_ev;
    eni_event_init(&intent_ev, ENI_EVENT_INTENT, "test");
    eni_event_set_intent(&intent_ev, "focus", 0.9f);

    g_stim_count = 0;
    eni_event_t feedback_ev;
    eni_status_t rc = eni_feedback_controller_evaluate(&ctrl, &intent_ev, &feedback_ev, 1000);
    if (rc != ENI_OK) { FAIL("evaluate should succeed"); eni_feedback_controller_shutdown(&ctrl); return; }
    if (g_stim_count != 1) { FAIL("stimulator should be called once"); eni_feedback_controller_shutdown(&ctrl); return; }
    if (feedback_ev.type != ENI_EVENT_FEEDBACK) { FAIL("wrong feedback type"); eni_feedback_controller_shutdown(&ctrl); return; }
    eni_feedback_controller_shutdown(&ctrl);
    PASS();
}

/* ── Test: evaluate no match ── */
static void test_feedback_evaluate_no_match(void)
{
    TEST(feedback_evaluate_no_match);
    eni_feedback_controller_t ctrl;
    eni_feedback_controller_init(&ctrl, &g_mock_stim, 1.0f, 500);

    eni_stim_params_t response = {
        .type = ENI_STIM_HAPTIC, .amplitude = 0.5f, .duration_ms = 200,
    };
    eni_feedback_controller_add_rule(&ctrl, "focus", 0.7f, &response);

    eni_event_t intent_ev;
    eni_event_init(&intent_ev, ENI_EVENT_INTENT, "test");
    eni_event_set_intent(&intent_ev, "motor_execute", 0.9f); /* Wrong intent */

    eni_event_t feedback_ev;
    eni_status_t rc = eni_feedback_controller_evaluate(&ctrl, &intent_ev, &feedback_ev, 1000);
    if (rc != ENI_ERR_NOT_FOUND) { FAIL("should return NOT_FOUND"); eni_feedback_controller_shutdown(&ctrl); return; }
    eni_feedback_controller_shutdown(&ctrl);
    PASS();
}

/* ── Test: evaluate low confidence ── */
static void test_feedback_evaluate_low_confidence(void)
{
    TEST(feedback_evaluate_low_confidence);
    eni_feedback_controller_t ctrl;
    eni_feedback_controller_init(&ctrl, &g_mock_stim, 1.0f, 500);

    eni_stim_params_t response = {
        .type = ENI_STIM_HAPTIC, .amplitude = 0.5f, .duration_ms = 200,
    };
    eni_feedback_controller_add_rule(&ctrl, "focus", 0.7f, &response);

    eni_event_t intent_ev;
    eni_event_init(&intent_ev, ENI_EVENT_INTENT, "test");
    eni_event_set_intent(&intent_ev, "focus", 0.3f); /* Too low */

    eni_event_t feedback_ev;
    eni_status_t rc = eni_feedback_controller_evaluate(&ctrl, &intent_ev, &feedback_ev, 1000);
    if (rc != ENI_ERR_NOT_FOUND) { FAIL("should return NOT_FOUND for low conf"); eni_feedback_controller_shutdown(&ctrl); return; }
    eni_feedback_controller_shutdown(&ctrl);
    PASS();
}

/* ── Test: evaluate disabled controller ── */
static void test_feedback_evaluate_disabled(void)
{
    TEST(feedback_evaluate_disabled);
    eni_feedback_controller_t ctrl;
    eni_feedback_controller_init(&ctrl, &g_mock_stim, 1.0f, 500);
    ctrl.enabled = false;

    eni_event_t intent_ev, feedback_ev;
    eni_event_init(&intent_ev, ENI_EVENT_INTENT, "test");
    eni_event_set_intent(&intent_ev, "focus", 0.9f);

    eni_status_t rc = eni_feedback_controller_evaluate(&ctrl, &intent_ev, &feedback_ev, 1000);
    if (rc != ENI_ERR_PERMISSION) { FAIL("should reject disabled"); return; }
    eni_feedback_controller_shutdown(&ctrl);
    PASS();
}

/* ── Test: evaluate wrong event type ── */
static void test_feedback_evaluate_wrong_type(void)
{
    TEST(feedback_evaluate_wrong_type);
    eni_feedback_controller_t ctrl;
    eni_feedback_controller_init(&ctrl, &g_mock_stim, 1.0f, 500);

    eni_event_t raw_ev, feedback_ev;
    eni_event_init(&raw_ev, ENI_EVENT_RAW, "test");

    eni_status_t rc = eni_feedback_controller_evaluate(&ctrl, &raw_ev, &feedback_ev, 1000);
    if (rc != ENI_ERR_INVALID) { FAIL("should reject non-intent event"); eni_feedback_controller_shutdown(&ctrl); return; }
    eni_feedback_controller_shutdown(&ctrl);
    PASS();
}

/* ── Test: evaluate NULL args ── */
static void test_feedback_evaluate_null(void)
{
    TEST(feedback_evaluate_null);
    eni_feedback_controller_t ctrl;
    eni_feedback_controller_init(&ctrl, &g_mock_stim, 1.0f, 500);
    eni_event_t ev, fb;
    eni_event_init(&ev, ENI_EVENT_INTENT, "test");

    if (eni_feedback_controller_evaluate(NULL, &ev, &fb, 0) != ENI_ERR_INVALID) { FAIL("null ctrl"); return; }
    if (eni_feedback_controller_evaluate(&ctrl, NULL, &fb, 0) != ENI_ERR_INVALID) { FAIL("null ev"); return; }
    if (eni_feedback_controller_evaluate(&ctrl, &ev, NULL, 0) != ENI_ERR_INVALID) { FAIL("null fb"); return; }
    eni_feedback_controller_shutdown(&ctrl);
    PASS();
}

/* ── Test: rule overflow ── */
static void test_feedback_rule_overflow(void)
{
    TEST(feedback_rule_overflow);
    eni_feedback_controller_t ctrl;
    eni_feedback_controller_init(&ctrl, &g_mock_stim, 1.0f, 500);
    eni_stim_params_t response = { .type = ENI_STIM_HAPTIC, .amplitude = 0.1f, .duration_ms = 100 };

    for (int i = 0; i < ENI_FEEDBACK_MAX_RULES; i++) {
        char name[32];
        snprintf(name, sizeof(name), "rule_%d", i);
        eni_status_t rc = eni_feedback_controller_add_rule(&ctrl, name, 0.5f, &response);
        if (rc != ENI_OK) { FAIL("add_rule should succeed"); eni_feedback_controller_shutdown(&ctrl); return; }
    }
    /* Next add should overflow */
    eni_status_t rc = eni_feedback_controller_add_rule(&ctrl, "overflow", 0.5f, &response);
    if (rc != ENI_ERR_OVERFLOW) { FAIL("should overflow"); eni_feedback_controller_shutdown(&ctrl); return; }
    eni_feedback_controller_shutdown(&ctrl);
    PASS();
}

/* ── Test: shutdown null ── */
static void test_feedback_shutdown_null(void)
{
    TEST(feedback_shutdown_null);
    eni_feedback_controller_shutdown(NULL); /* Should not crash */
    PASS();
}

/* ── Test: multiple rules, match second ── */
static void test_feedback_multiple_rules(void)
{
    TEST(feedback_multiple_rules);
    eni_feedback_controller_t ctrl;
    eni_feedback_controller_init(&ctrl, &g_mock_stim, 1.0f, 500);

    eni_stim_params_t resp1 = { .type = ENI_STIM_HAPTIC, .amplitude = 0.3f, .duration_ms = 100 };
    eni_stim_params_t resp2 = { .type = ENI_STIM_AUDITORY, .amplitude = 0.5f, .duration_ms = 200 };
    eni_feedback_controller_add_rule(&ctrl, "motor", 0.7f, &resp1);
    eni_feedback_controller_add_rule(&ctrl, "focus", 0.6f, &resp2);

    eni_event_t intent_ev;
    eni_event_init(&intent_ev, ENI_EVENT_INTENT, "test");
    eni_event_set_intent(&intent_ev, "focus", 0.8f);

    g_stim_count = 0;
    eni_event_t feedback_ev;
    eni_status_t rc = eni_feedback_controller_evaluate(&ctrl, &intent_ev, &feedback_ev, 2000);
    if (rc != ENI_OK) { FAIL("should match focus rule"); eni_feedback_controller_shutdown(&ctrl); return; }
    if (feedback_ev.payload.feedback.type != ENI_STIM_AUDITORY) { FAIL("wrong stim type"); eni_feedback_controller_shutdown(&ctrl); return; }
    eni_feedback_controller_shutdown(&ctrl);
    PASS();
}

int main(void)
{
    printf("=== ENI Feedback Controller Tests ===\n\n");
    test_feedback_init();
    test_feedback_init_null();
    test_feedback_add_rule();
    test_feedback_add_rule_invalid();
    test_feedback_evaluate_match();
    test_feedback_evaluate_no_match();
    test_feedback_evaluate_low_confidence();
    test_feedback_evaluate_disabled();
    test_feedback_evaluate_wrong_type();
    test_feedback_evaluate_null();
    test_feedback_rule_overflow();
    test_feedback_shutdown_null();
    test_feedback_multiple_rules();
    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           tests_passed, tests_run, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
