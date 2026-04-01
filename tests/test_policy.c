// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023
/**
 * @file test_policy.c
 * @brief Unit tests for ENI policy engine
 */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "eni/types.h"
#include "eni/policy.h"

static int passed = 0;
#define PASS(name) do { printf("[PASS] %s\n", name); passed++; } while(0)

/* ---- Stub implementations ---- */
const char *eni_status_str(eni_status_t s) { (void)s; return "OK"; }
eni_timestamp_t eni_timestamp_now(void) { eni_timestamp_t t = {0,0}; return t; }

eni_status_t eni_policy_init(eni_policy_engine_t *engine) {
    if (!engine) return ENI_ERR_INVALID;
    memset(engine, 0, sizeof(*engine));
    engine->default_deny = false;
    return ENI_OK;
}

eni_status_t eni_policy_add_rule(eni_policy_engine_t *engine, const char *action,
                                  eni_policy_verdict_t verdict,
                                  eni_action_class_t action_class) {
    if (!engine || !action) return ENI_ERR_INVALID;
    if (engine->count >= ENI_POLICY_MAX_RULES) return ENI_ERR_OVERFLOW;
    strncpy(engine->rules[engine->count].action, action, ENI_POLICY_ACTION_MAX - 1);
    engine->rules[engine->count].verdict = verdict;
    engine->rules[engine->count].action_class = action_class;
    engine->count++;
    return ENI_OK;
}

eni_policy_verdict_t eni_policy_evaluate(const eni_policy_engine_t *engine,
                                          const char *action) {
    if (!engine || !action) return ENI_POLICY_DENY;
    for (int i = 0; i < engine->count; i++) {
        if (strcmp(engine->rules[i].action, action) == 0)
            return engine->rules[i].verdict;
    }
    return engine->default_deny ? ENI_POLICY_DENY : ENI_POLICY_ALLOW;
}

eni_status_t eni_policy_set_default_deny(eni_policy_engine_t *engine, bool deny) {
    if (!engine) return ENI_ERR_INVALID;
    engine->default_deny = deny;
    return ENI_OK;
}

void eni_policy_dump(const eni_policy_engine_t *engine) { (void)engine; }

/* ---- Tests ---- */
static void test_policy_init(void) {
    eni_policy_engine_t engine;
    eni_status_t rc = eni_policy_init(&engine);
    assert(rc == ENI_OK);
    assert(engine.count == 0);
    assert(engine.default_deny == false);
    PASS("policy_init");
}

static void test_policy_init_null(void) {
    assert(eni_policy_init(NULL) == ENI_ERR_INVALID);
    PASS("policy_init_null");
}

static void test_policy_add_rule(void) {
    eni_policy_engine_t engine;
    eni_policy_init(&engine);
    eni_status_t rc = eni_policy_add_rule(&engine, "move_arm",
                                           ENI_POLICY_ALLOW, ENI_ACTION_SAFE);
    assert(rc == ENI_OK);
    assert(engine.count == 1);
    PASS("policy_add_rule");
}

static void test_policy_add_multiple_rules(void) {
    eni_policy_engine_t engine;
    eni_policy_init(&engine);
    eni_policy_add_rule(&engine, "move_arm", ENI_POLICY_ALLOW, ENI_ACTION_SAFE);
    eni_policy_add_rule(&engine, "inject_drug", ENI_POLICY_DENY, ENI_ACTION_RESTRICTED);
    eni_policy_add_rule(&engine, "adjust_speed", ENI_POLICY_CONFIRM, ENI_ACTION_CONTROLLED);
    assert(engine.count == 3);
    PASS("policy_add_multiple_rules");
}

static void test_policy_evaluate_allow(void) {
    eni_policy_engine_t engine;
    eni_policy_init(&engine);
    eni_policy_add_rule(&engine, "read_sensor", ENI_POLICY_ALLOW, ENI_ACTION_SAFE);
    eni_policy_verdict_t v = eni_policy_evaluate(&engine, "read_sensor");
    assert(v == ENI_POLICY_ALLOW);
    PASS("policy_evaluate_allow");
}

static void test_policy_evaluate_deny(void) {
    eni_policy_engine_t engine;
    eni_policy_init(&engine);
    eni_policy_add_rule(&engine, "shutdown_reactor", ENI_POLICY_DENY, ENI_ACTION_RESTRICTED);
    eni_policy_verdict_t v = eni_policy_evaluate(&engine, "shutdown_reactor");
    assert(v == ENI_POLICY_DENY);
    PASS("policy_evaluate_deny");
}

static void test_policy_evaluate_confirm(void) {
    eni_policy_engine_t engine;
    eni_policy_init(&engine);
    eni_policy_add_rule(&engine, "adjust_dose", ENI_POLICY_CONFIRM, ENI_ACTION_CONTROLLED);
    eni_policy_verdict_t v = eni_policy_evaluate(&engine, "adjust_dose");
    assert(v == ENI_POLICY_CONFIRM);
    PASS("policy_evaluate_confirm");
}

static void test_policy_default_allow(void) {
    eni_policy_engine_t engine;
    eni_policy_init(&engine);
    eni_policy_verdict_t v = eni_policy_evaluate(&engine, "unknown_action");
    assert(v == ENI_POLICY_ALLOW);
    PASS("policy_default_allow");
}

static void test_policy_default_deny(void) {
    eni_policy_engine_t engine;
    eni_policy_init(&engine);
    eni_policy_set_default_deny(&engine, true);
    eni_policy_verdict_t v = eni_policy_evaluate(&engine, "unknown_action");
    assert(v == ENI_POLICY_DENY);
    PASS("policy_default_deny");
}

static void test_policy_constants(void) {
    assert(ENI_POLICY_MAX_RULES == 64);
    assert(ENI_POLICY_ACTION_MAX == 64);
    PASS("policy_constants");
}

static void test_action_class_enum(void) {
    assert(ENI_ACTION_SAFE == 0);
    assert(ENI_ACTION_CONTROLLED == 1);
    assert(ENI_ACTION_RESTRICTED == 2);
    PASS("action_class_enum");
}

int main(void) {
    printf("=== eni Policy Engine Tests ===\n");
    test_policy_init();
    test_policy_init_null();
    test_policy_add_rule();
    test_policy_add_multiple_rules();
    test_policy_evaluate_allow();
    test_policy_evaluate_deny();
    test_policy_evaluate_confirm();
    test_policy_default_allow();
    test_policy_default_deny();
    test_policy_constants();
    test_action_class_enum();
    printf("\n=== ALL %d TESTS PASSED ===\n", passed);
    return 0;
}
