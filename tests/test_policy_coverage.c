// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023
//
// Coverage tests for common/src/policy.c
// Exercises: eni_policy_dump with all 3 verdicts, all 3 action classes,
//            null engine guard, overflow guard (ENI_POLICY_MAX_RULES),
//            long action name truncation

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

/* ---------- eni_policy_dump: null guard ---------- */

static void test_policy_dump_null(void)
{
    TEST(policy_dump_null);
    /* Should not crash; just returns immediately */
    eni_policy_dump(NULL);
    PASS();
}

/* ---------- eni_policy_dump: empty engine ---------- */

static void test_policy_dump_empty(void)
{
    TEST(policy_dump_empty);
    eni_policy_engine_t eng;
    eni_policy_init(&eng);

    /* Exercises the header line: "0 rules (default=allow)" */
    eni_policy_dump(&eng);
    PASS();
}

/* ---------- eni_policy_dump: default deny ---------- */

static void test_policy_dump_default_deny(void)
{
    TEST(policy_dump_default_deny);
    eni_policy_engine_t eng;
    eni_policy_init(&eng);
    eni_policy_set_default_deny(&eng, true);

    /* Exercises "default=deny" in the header */
    eni_policy_dump(&eng);
    PASS();
}

/* ---------- eni_policy_dump: verdict=allow + action_class=safe ---------- */

static void test_policy_dump_allow_safe(void)
{
    TEST(policy_dump_allow_safe);
    eni_policy_engine_t eng;
    eni_policy_init(&eng);
    eni_policy_add_rule(&eng, "ui.cursor.move", ENI_POLICY_ALLOW, ENI_ACTION_SAFE);

    /* Exercises verdict_str="allow", class_str="safe" in dump loop */
    eni_policy_dump(&eng);
    PASS();
}

/* ---------- eni_policy_dump: verdict=deny + action_class=restricted ---------- */

static void test_policy_dump_deny_restricted(void)
{
    TEST(policy_dump_deny_restricted);
    eni_policy_engine_t eng;
    eni_policy_init(&eng);
    eni_policy_add_rule(&eng, "system.shutdown", ENI_POLICY_DENY, ENI_ACTION_RESTRICTED);

    /* Exercises verdict_str="deny", class_str="restricted" */
    eni_policy_dump(&eng);
    PASS();
}

/* ---------- eni_policy_dump: verdict=confirm + action_class=controlled ---------- */

static void test_policy_dump_confirm_controlled(void)
{
    TEST(policy_dump_confirm_controlled);
    eni_policy_engine_t eng;
    eni_policy_init(&eng);
    eni_policy_add_rule(&eng, "device.pair", ENI_POLICY_CONFIRM, ENI_ACTION_CONTROLLED);

    /* Exercises verdict_str="confirm", class_str="controlled" */
    eni_policy_dump(&eng);
    PASS();
}

/* ---------- eni_policy_dump: all 3 verdicts + all 3 classes together ---------- */

static void test_policy_dump_all_combos(void)
{
    TEST(policy_dump_all_combos);
    eni_policy_engine_t eng;
    eni_policy_init(&eng);
    eni_policy_add_rule(&eng, "action.a", ENI_POLICY_ALLOW,   ENI_ACTION_SAFE);
    eni_policy_add_rule(&eng, "action.b", ENI_POLICY_DENY,    ENI_ACTION_CONTROLLED);
    eni_policy_add_rule(&eng, "action.c", ENI_POLICY_CONFIRM, ENI_ACTION_RESTRICTED);

    /* Exercises all branches of verdict_str and class_str ternaries in the loop */
    eni_policy_dump(&eng);
    PASS();
}

/* ---------- overflow: ENI_POLICY_MAX_RULES ---------- */

static void test_policy_overflow(void)
{
    TEST(policy_overflow);
    eni_policy_engine_t eng;
    eni_policy_init(&eng);

    char action[ENI_POLICY_ACTION_MAX];
    for (int i = 0; i < ENI_POLICY_MAX_RULES; i++) {
        snprintf(action, sizeof(action), "rule.%d", i);
        eni_status_t st = eni_policy_add_rule(&eng, action, ENI_POLICY_ALLOW, ENI_ACTION_SAFE);
        if (st != ENI_OK) { FAIL("add_rule should succeed"); return; }
    }

    /* One more should overflow */
    eni_status_t st = eni_policy_add_rule(&eng, "overflow", ENI_POLICY_DENY, ENI_ACTION_RESTRICTED);
    if (st != ENI_ERR_OVERFLOW) { FAIL("expected overflow"); return; }
    PASS();
}

/* ---------- null guards ---------- */

static void test_policy_init_null(void)
{
    TEST(policy_init_null);
    eni_status_t st = eni_policy_init(NULL);
    if (st != ENI_ERR_INVALID) { FAIL("expected INVALID"); return; }
    PASS();
}

static void test_policy_add_rule_null_engine(void)
{
    TEST(policy_add_rule_null_engine);
    eni_status_t st = eni_policy_add_rule(NULL, "action", ENI_POLICY_ALLOW, ENI_ACTION_SAFE);
    if (st != ENI_ERR_INVALID) { FAIL("expected INVALID"); return; }
    PASS();
}

static void test_policy_add_rule_null_action(void)
{
    TEST(policy_add_rule_null_action);
    eni_policy_engine_t eng;
    eni_policy_init(&eng);
    eni_status_t st = eni_policy_add_rule(&eng, NULL, ENI_POLICY_ALLOW, ENI_ACTION_SAFE);
    if (st != ENI_ERR_INVALID) { FAIL("expected INVALID"); return; }
    PASS();
}

static void test_policy_evaluate_null_engine(void)
{
    TEST(policy_evaluate_null_engine);
    eni_policy_verdict_t v = eni_policy_evaluate(NULL, "action");
    if (v != ENI_POLICY_DENY) { FAIL("expected DENY"); return; }
    PASS();
}

static void test_policy_evaluate_null_action(void)
{
    TEST(policy_evaluate_null_action);
    eni_policy_engine_t eng;
    eni_policy_init(&eng);
    eni_policy_verdict_t v = eni_policy_evaluate(&eng, NULL);
    if (v != ENI_POLICY_DENY) { FAIL("expected DENY"); return; }
    PASS();
}

static void test_policy_set_default_deny_null(void)
{
    TEST(policy_set_default_deny_null);
    eni_status_t st = eni_policy_set_default_deny(NULL, true);
    if (st != ENI_ERR_INVALID) { FAIL("expected INVALID"); return; }
    PASS();
}

/* ---------- long action name truncation ---------- */

static void test_policy_long_action(void)
{
    TEST(policy_long_action);
    eni_policy_engine_t eng;
    eni_policy_init(&eng);

    char long_action[ENI_POLICY_ACTION_MAX + 32];
    memset(long_action, 'X', sizeof(long_action) - 1);
    long_action[sizeof(long_action) - 1] = '\0';

    eni_status_t st = eni_policy_add_rule(&eng, long_action, ENI_POLICY_ALLOW, ENI_ACTION_SAFE);
    if (st != ENI_OK) { FAIL("add_rule failed"); return; }
    if (strlen(eng.rules[0].action) >= ENI_POLICY_ACTION_MAX) { FAIL("action not truncated"); return; }
    PASS();
}

/* ---------- evaluate: known action (exercises strcmp match → return verdict) -- */

static void test_policy_evaluate_known_action(void)
{
    TEST(policy_evaluate_known_action);
    eni_policy_engine_t eng;
    eni_policy_init(&eng);
    eni_policy_add_rule(&eng, "ui.cursor.move", ENI_POLICY_ALLOW, ENI_ACTION_SAFE);
    eni_policy_add_rule(&eng, "system.shutdown", ENI_POLICY_DENY, ENI_ACTION_RESTRICTED);
    eni_policy_add_rule(&eng, "device.pair", ENI_POLICY_CONFIRM, ENI_ACTION_CONTROLLED);

    eni_policy_verdict_t v1 = eni_policy_evaluate(&eng, "ui.cursor.move");
    if (v1 != ENI_POLICY_ALLOW) { FAIL("expected ALLOW for cursor.move"); return; }

    eni_policy_verdict_t v2 = eni_policy_evaluate(&eng, "system.shutdown");
    if (v2 != ENI_POLICY_DENY) { FAIL("expected DENY for shutdown"); return; }

    eni_policy_verdict_t v3 = eni_policy_evaluate(&eng, "device.pair");
    if (v3 != ENI_POLICY_CONFIRM) { FAIL("expected CONFIRM for pair"); return; }

    PASS();
}

/* ---------- evaluate: unknown action with default_deny=false ---------- */

static void test_policy_evaluate_unknown_allow(void)
{
    TEST(policy_evaluate_unknown_allow);
    eni_policy_engine_t eng;
    eni_policy_init(&eng);
    eni_policy_add_rule(&eng, "known", ENI_POLICY_DENY, ENI_ACTION_SAFE);

    eni_policy_verdict_t v = eni_policy_evaluate(&eng, "unknown.action");
    if (v != ENI_POLICY_ALLOW) { FAIL("expected ALLOW for unknown"); return; }
    PASS();
}

/* ---------- evaluate: unknown action with default_deny=true ---------- */

static void test_policy_evaluate_unknown_deny(void)
{
    TEST(policy_evaluate_unknown_deny);
    eni_policy_engine_t eng;
    eni_policy_init(&eng);
    eni_policy_set_default_deny(&eng, true);
    eni_policy_add_rule(&eng, "known", ENI_POLICY_ALLOW, ENI_ACTION_SAFE);

    eni_policy_verdict_t v = eni_policy_evaluate(&eng, "unknown.action");
    if (v != ENI_POLICY_DENY) { FAIL("expected DENY for unknown"); return; }
    PASS();
}

/* ---------- main ---------- */

int main(void)
{
    printf("=== policy.c Coverage Tests ===\n\n");

    /* Suppress log output from eni_policy_add_rule (it uses ENI_LOG_DEBUG) */
    eni_log_set_level(ENI_LOG_FATAL);

    /* eni_policy_dump */
    test_policy_dump_null();
    test_policy_dump_empty();
    test_policy_dump_default_deny();
    test_policy_dump_allow_safe();
    test_policy_dump_deny_restricted();
    test_policy_dump_confirm_controlled();
    test_policy_dump_all_combos();

    /* Overflow */
    test_policy_overflow();

    /* Null guards */
    test_policy_init_null();
    test_policy_add_rule_null_engine();
    test_policy_add_rule_null_action();
    test_policy_evaluate_null_engine();
    test_policy_evaluate_null_action();
    test_policy_set_default_deny_null();

    /* Long action truncation */
    test_policy_long_action();

    /* Evaluate behavior */
    test_policy_evaluate_known_action();
    test_policy_evaluate_unknown_allow();
    test_policy_evaluate_unknown_deny();

    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           tests_passed, tests_run, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
