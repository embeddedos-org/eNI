// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Tests for JSON parser

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "eni/json.h"

static int tests_run = 0, tests_passed = 0, tests_failed = 0;
#define TEST(name) do { tests_run++; printf("  TEST %-40s ", #name); } while(0)
#define PASS() do { tests_passed++; printf("[PASS]\n"); } while(0)
#define FAIL(msg) do { tests_failed++; printf("[FAIL] %s\n", msg); } while(0)

static void test_parse_object(void)
{
    TEST(json_parse_object);
    eni_json_value_t root;
    const char *json = "{\"name\": \"test\", \"value\": 42, \"flag\": true}";
    eni_status_t rc = eni_json_parse(json, &root);
    assert(rc == ENI_OK);
    assert(root.type == ENI_JSON_OBJECT);

    const char *name = eni_json_get_string(&root, "name", NULL);
    assert(name != NULL && strcmp(name, "test") == 0);

    double val = eni_json_get_number(&root, "value", 0.0);
    assert(val == 42.0);

    bool flag = eni_json_get_bool(&root, "flag", false);
    assert(flag == true);

    eni_json_free(&root);
    PASS();
}

static void test_parse_nested(void)
{
    TEST(json_parse_nested);
    const char *json = "{\"dsp\": {\"epoch_size\": 256, \"sample_rate\": 512}}";
    eni_json_value_t root;
    eni_status_t rc = eni_json_parse(json, &root);
    assert(rc == ENI_OK);

    const eni_json_value_t *dsp = eni_json_get(&root, "dsp");
    assert(dsp != NULL && dsp->type == ENI_JSON_OBJECT);

    double epoch = eni_json_get_number(dsp, "epoch_size", 0.0);
    assert(epoch == 256.0);

    eni_json_free(&root);
    PASS();
}

static void test_parse_array(void)
{
    TEST(json_parse_array);
    const char *json = "{\"values\": [1, 2, 3]}";
    eni_json_value_t root;
    eni_status_t rc = eni_json_parse(json, &root);
    assert(rc == ENI_OK);

    const eni_json_value_t *arr = eni_json_get(&root, "values");
    assert(arr != NULL && arr->type == ENI_JSON_ARRAY);
    assert(arr->data.container.count == 3);

    eni_json_free(&root);
    PASS();
}

static void test_parse_null(void)
{
    TEST(json_parse_null);
    const char *json = "{\"empty\": null}";
    eni_json_value_t root;
    eni_status_t rc = eni_json_parse(json, &root);
    assert(rc == ENI_OK);

    const eni_json_value_t *v = eni_json_get(&root, "empty");
    assert(v != NULL && v->type == ENI_JSON_NULL);

    eni_json_free(&root);
    PASS();
}

static void test_parse_escape(void)
{
    TEST(json_parse_escape);
    const char *json = "{\"path\": \"C:\\\\Users\\\\test\"}";
    eni_json_value_t root;
    eni_status_t rc = eni_json_parse(json, &root);
    assert(rc == ENI_OK);

    const char *path = eni_json_get_string(&root, "path", NULL);
    assert(path != NULL && strcmp(path, "C:\\Users\\test") == 0);

    eni_json_free(&root);
    PASS();
}

static void test_parse_defaults(void)
{
    TEST(json_get_defaults);
    eni_json_value_t root;
    eni_json_parse("{}", &root);

    assert(strcmp(eni_json_get_string(&root, "missing", "default"), "default") == 0);
    assert(eni_json_get_number(&root, "missing", 99.0) == 99.0);
    assert(eni_json_get_bool(&root, "missing", true) == true);

    eni_json_free(&root);
    PASS();
}

int main(void)
{
    printf("=== JSON Parser Tests ===\n");
    test_parse_object();
    test_parse_nested();
    test_parse_array();
    test_parse_null();
    test_parse_escape();
    test_parse_defaults();
    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return tests_failed > 0 ? 1 : 0;
}
