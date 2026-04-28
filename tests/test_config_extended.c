// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Test: common/src/config.c — KV config loading, JSON config, new keys

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "eni/common.h"
#include "eni/config.h"

static int tests_run = 0, tests_passed = 0, tests_failed = 0;
#define TEST(name) do { tests_run++; printf("  TEST %-40s ", #name); } while(0)
#define PASS() do { tests_passed++; printf("[PASS]\n"); } while(0)
#define FAIL(msg) do { tests_failed++; printf("[FAIL] %s\n", msg); } while(0)

/* Helper: write a temp file with given content */
static int write_temp_file(const char *path, const char *content)
{
    FILE *fp = fopen(path, "w");
    if (!fp) return -1;
    fputs(content, fp);
    fclose(fp);
    return 0;
}

/* ── Test: config init defaults ── */
static void test_config_init(void)
{
    TEST(config_init);
    eni_config_t cfg;
    eni_status_t rc = eni_config_init(&cfg);
    if (rc != ENI_OK) { FAIL("init failed"); return; }
    if (cfg.variant != ENI_VARIANT_MIN) { FAIL("wrong variant"); return; }
    if (cfg.mode != ENI_MODE_INTENT) { FAIL("wrong mode"); return; }
    PASS();
}

/* ── Test: config init null ── */
static void test_config_init_null(void)
{
    TEST(config_init_null);
    if (eni_config_init(NULL) != ENI_ERR_INVALID) { FAIL("should reject NULL"); return; }
    PASS();
}

/* ── Test: load KV config file ── */
static void test_config_load_kv(void)
{
    TEST(config_load_kv);
    const char *path = "/tmp/eni_test_config.cfg";
    const char *content =
        "# Test config\n"
        "variant = framework\n"
        "mode = features_intent\n"
        "confidence_threshold = 0.75\n"
        "debounce_ms = 200\n"
        "max_providers = 3\n"
        "default_deny = true\n"
        "epoch_size = 256\n"
        "sample_rate = 500\n"
        "artifact_threshold = 100.0\n"
        "model_path = /models/test.bin\n"
        "num_classes = 8\n";

    if (write_temp_file(path, content) != 0) { FAIL("cannot write temp file"); return; }

    eni_config_t cfg;
    eni_status_t rc = eni_config_load_file(&cfg, path);
    if (rc != ENI_OK) { FAIL("load_file failed"); remove(path); return; }
    if (cfg.variant != ENI_VARIANT_FRAMEWORK) { FAIL("wrong variant"); remove(path); return; }
    if (cfg.mode != ENI_MODE_FEATURES_INTENT) { FAIL("wrong mode"); remove(path); return; }
    if (cfg.filter.debounce_ms != 200) { FAIL("wrong debounce"); remove(path); return; }
    if (cfg.provider_count != 3) { FAIL("wrong provider count"); remove(path); return; }
    if (!cfg.policy.require_confirmation) { FAIL("default_deny not set"); remove(path); return; }
    if (cfg.dsp.epoch_size != 256) { FAIL("wrong epoch_size"); remove(path); return; }
    if (cfg.dsp.sample_rate != 500) { FAIL("wrong sample_rate"); remove(path); return; }
    if (cfg.decoder.num_classes != 8) { FAIL("wrong num_classes"); remove(path); return; }
    if (strcmp(cfg.decoder.model_path, "/models/test.bin") != 0) { FAIL("wrong model_path"); remove(path); return; }
    remove(path);
    PASS();
}

/* ── Test: KV config with mode variants ── */
static void test_config_kv_modes(void)
{
    TEST(config_kv_modes);
    const char *path = "/tmp/eni_test_modes.cfg";

    /* Test mode=features */
    write_temp_file(path, "mode = features\n");
    eni_config_t cfg;
    eni_config_load_file(&cfg, path);
    if (cfg.mode != ENI_MODE_FEATURES) { FAIL("features mode"); remove(path); return; }

    /* Test mode=raw */
    write_temp_file(path, "mode = raw\n");
    eni_config_load_file(&cfg, path);
    if (cfg.mode != ENI_MODE_RAW) { FAIL("raw mode"); remove(path); return; }

    /* Test mode=intent */
    write_temp_file(path, "mode = intent\n");
    eni_config_load_file(&cfg, path);
    if (cfg.mode != ENI_MODE_INTENT) { FAIL("intent mode"); remove(path); return; }

    /* Test variant=min */
    write_temp_file(path, "variant = min\n");
    eni_config_load_file(&cfg, path);
    if (cfg.variant != ENI_VARIANT_MIN) { FAIL("min variant"); remove(path); return; }

    remove(path);
    PASS();
}

/* ── Test: KV config with comments and whitespace ── */
static void test_config_kv_whitespace(void)
{
    TEST(config_kv_whitespace);
    const char *path = "/tmp/eni_test_ws.cfg";
    const char *content =
        "# comment line\n"
        "; another comment\n"
        "\n"
        "  variant = min  \n"
        "\tmode = intent\t\n";

    write_temp_file(path, content);
    eni_config_t cfg;
    eni_status_t rc = eni_config_load_file(&cfg, path);
    if (rc != ENI_OK) { FAIL("load failed"); remove(path); return; }
    if (cfg.variant != ENI_VARIANT_MIN) { FAIL("wrong variant"); remove(path); return; }
    remove(path);
    PASS();
}

/* ── Test: load nonexistent file ── */
static void test_config_load_nonexistent(void)
{
    TEST(config_load_nonexistent);
    eni_config_t cfg;
    eni_status_t rc = eni_config_load_file(&cfg, "/tmp/eni_does_not_exist_12345.cfg");
    if (rc != ENI_ERR_IO) { FAIL("should return IO error"); return; }
    PASS();
}

/* ── Test: load file with NULL args ── */
static void test_config_load_null_args(void)
{
    TEST(config_load_null_args);
    eni_config_t cfg;
    if (eni_config_load_file(NULL, "/tmp/test.cfg") != ENI_ERR_INVALID) { FAIL("null cfg"); return; }
    if (eni_config_load_file(&cfg, NULL) != ENI_ERR_INVALID) { FAIL("null path"); return; }
    PASS();
}

/* ── Test: load defaults min ── */
static void test_config_defaults_min(void)
{
    TEST(config_defaults_min);
    eni_config_t cfg;
    eni_config_load_defaults(&cfg, ENI_VARIANT_MIN);
    if (cfg.variant != ENI_VARIANT_MIN) { FAIL("wrong variant"); return; }
    if (cfg.mode != ENI_MODE_INTENT) { FAIL("wrong mode"); return; }
    if (cfg.provider_count != 1) { FAIL("wrong count"); return; }
    PASS();
}

/* ── Test: load defaults framework ── */
static void test_config_defaults_framework(void)
{
    TEST(config_defaults_framework);
    eni_config_t cfg;
    eni_config_load_defaults(&cfg, ENI_VARIANT_FRAMEWORK);
    if (cfg.variant != ENI_VARIANT_FRAMEWORK) { FAIL("wrong variant"); return; }
    if (cfg.mode != ENI_MODE_FEATURES_INTENT) { FAIL("wrong mode"); return; }
    if (!cfg.observability.metrics) { FAIL("metrics should be on"); return; }
    if (!cfg.observability.audit) { FAIL("audit should be on"); return; }
    PASS();
}

/* ── Test: load defaults null ── */
static void test_config_defaults_null(void)
{
    TEST(config_defaults_null);
    if (eni_config_load_defaults(NULL, ENI_VARIANT_MIN) != ENI_ERR_INVALID) { FAIL("should reject NULL"); return; }
    PASS();
}

/* ── Test: config dump (just verify no crash) ── */
static void test_config_dump(void)
{
    TEST(config_dump);
    eni_config_t cfg;
    eni_config_load_defaults(&cfg, ENI_VARIANT_FRAMEWORK);
    eni_config_dump(&cfg);
    eni_config_dump(NULL); /* Should not crash */
    PASS();
}

int main(void)
{
    printf("=== ENI Config Extended Tests ===\n\n");
    test_config_init();
    test_config_init_null();
    test_config_load_kv();
    test_config_kv_modes();
    test_config_kv_whitespace();
    test_config_load_nonexistent();
    test_config_load_null_args();
    test_config_defaults_min();
    test_config_defaults_framework();
    test_config_defaults_null();
    test_config_dump();
    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           tests_passed, tests_run, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
