// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Test: common/src/decoder.c — energy + nn decoders

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "eni/common.h"
#include "eni/decoder.h"
#include "eni/dsp.h"

static int tests_run = 0, tests_passed = 0, tests_failed = 0;
#define TEST(name) do { tests_run++; printf("  TEST %-40s ", #name); } while(0)
#define PASS() do { tests_passed++; printf("[PASS]\n"); } while(0)
#define FAIL(msg) do { tests_failed++; printf("[FAIL] %s\n", msg); } while(0)

/* ── Test: energy decoder init ── */
static void test_energy_decoder_init(void)
{
    TEST(energy_decoder_init);
    eni_decoder_t dec;
    eni_decoder_config_t cfg = {0};
    cfg.num_classes = 4;
    cfg.confidence_threshold = 0.5f;

    eni_status_t rc = eni_decoder_init(&dec, &eni_decoder_energy_ops, &cfg);
    if (rc != ENI_OK) { FAIL("init failed"); return; }
    if (strcmp(dec.name, "energy") != 0) { FAIL("wrong name"); return; }
    eni_decoder_shutdown(&dec);
    PASS();
}

/* ── Test: energy decoder decode with high energy → motor_execute ── */
static void test_energy_decode_high_energy(void)
{
    TEST(energy_decode_high_energy);
    eni_decoder_t dec;
    eni_decoder_config_t cfg = {0};
    cfg.num_classes = 4;
    cfg.confidence_threshold = 0.5f;
    eni_decoder_init(&dec, &eni_decoder_energy_ops, &cfg);

    eni_dsp_features_t features = {0};
    features.total_power = 100.0f; /* Very high energy → motor_execute */

    eni_decode_result_t result;
    eni_status_t rc = eni_decoder_decode(&dec, &features, &result);
    if (rc != ENI_OK) { FAIL("decode failed"); eni_decoder_shutdown(&dec); return; }
    if (result.count != 4) { FAIL("wrong class count"); eni_decoder_shutdown(&dec); return; }
    if (strcmp(result.intents[result.best_idx].name, "motor_execute") != 0) {
        FAIL("expected motor_execute for high energy"); eni_decoder_shutdown(&dec); return;
    }
    eni_decoder_shutdown(&dec);
    PASS();
}

/* ── Test: energy decoder decode with low energy → idle ── */
static void test_energy_decode_low_energy(void)
{
    TEST(energy_decode_low_energy);
    eni_decoder_t dec;
    eni_decoder_config_t cfg = {0};
    cfg.num_classes = 4;
    cfg.confidence_threshold = 0.5f;
    eni_decoder_init(&dec, &eni_decoder_energy_ops, &cfg);

    eni_dsp_features_t features = {0};
    features.total_power = 0.001f; /* Very low energy → idle */

    eni_decode_result_t result;
    eni_status_t rc = eni_decoder_decode(&dec, &features, &result);
    if (rc != ENI_OK) { FAIL("decode failed"); eni_decoder_shutdown(&dec); return; }
    if (strcmp(result.intents[result.best_idx].name, "idle") != 0) {
        FAIL("expected idle for low energy"); eni_decoder_shutdown(&dec); return;
    }
    eni_decoder_shutdown(&dec);
    PASS();
}

/* ── Test: energy decoder decode with medium energy → attention ── */
static void test_energy_decode_medium_energy(void)
{
    TEST(energy_decode_medium_energy);
    eni_decoder_t dec;
    eni_decoder_config_t cfg = {0};
    cfg.num_classes = 4;
    cfg.confidence_threshold = 0.5f;
    eni_decoder_init(&dec, &eni_decoder_energy_ops, &cfg);

    eni_dsp_features_t features = {0};
    /* energy = sqrt(total_power). threshold=0.5, attention range: >t*1.0 but <t*2.0 */
    features.total_power = 0.36f; /* sqrt(0.36)=0.6, > 0.5*1.0=0.5, < 0.5*2.0=1.0 → attention */

    eni_decode_result_t result;
    eni_decoder_decode(&dec, &features, &result);
    if (strcmp(result.intents[result.best_idx].name, "attention") != 0) {
        FAIL("expected attention for medium energy"); eni_decoder_shutdown(&dec); return;
    }
    eni_decoder_shutdown(&dec);
    PASS();
}

/* ── Test: nn decoder init and decode ── */
static void test_nn_decoder_init(void)
{
    TEST(nn_decoder_init);
    eni_decoder_t dec;
    eni_decoder_config_t cfg = {0};
    cfg.num_classes = 4;

    eni_status_t rc = eni_decoder_init(&dec, &eni_decoder_nn_ops, &cfg);
    if (rc != ENI_OK) { FAIL("init failed"); return; }
    if (strcmp(dec.name, "nn") != 0) { FAIL("wrong name"); return; }
    eni_decoder_shutdown(&dec);
    PASS();
}

/* ── Test: nn decoder heuristic fallback ── */
static void test_nn_decode_heuristic(void)
{
    TEST(nn_decode_heuristic);
    eni_decoder_t dec;
    eni_decoder_config_t cfg = {0};
    cfg.num_classes = 4;
    eni_decoder_init(&dec, &eni_decoder_nn_ops, &cfg);

    eni_dsp_features_t features = {0};
    features.band_power[0] = 1.0f; /* delta */
    features.band_power[1] = 2.0f; /* theta */
    features.band_power[2] = 5.0f; /* alpha */
    features.band_power[3] = 1.0f; /* beta */
    features.band_power[4] = 0.5f; /* gamma */
    features.total_power = 9.5f;
    features.hjorth_activity = 1.0f;
    features.hjorth_mobility = 0.5f;

    eni_decode_result_t result;
    eni_status_t rc = eni_decoder_decode(&dec, &features, &result);
    if (rc != ENI_OK) { FAIL("decode failed"); eni_decoder_shutdown(&dec); return; }
    if (result.count != 4) { FAIL("wrong class count"); eni_decoder_shutdown(&dec); return; }
    /* Check that confidences sum to ~1.0 (normalized) */
    float sum = 0;
    for (int i = 0; i < result.count; i++) sum += result.intents[i].confidence;
    if (sum < 0.9f || sum > 1.1f) { FAIL("confidences not normalized"); eni_decoder_shutdown(&dec); return; }
    eni_decoder_shutdown(&dec);
    PASS();
}

/* ── Test: decoder invalid args ── */
static void test_decoder_invalid_args(void)
{
    TEST(decoder_invalid_args);
    eni_decoder_config_t cfg = {0};
    cfg.num_classes = 4;

    eni_status_t rc = eni_decoder_init(NULL, &eni_decoder_energy_ops, &cfg);
    if (rc != ENI_ERR_INVALID) { FAIL("should reject NULL dec"); return; }

    eni_decoder_t dec;
    rc = eni_decoder_init(&dec, NULL, &cfg);
    if (rc != ENI_ERR_INVALID) { FAIL("should reject NULL ops"); return; }

    /* decode with NULL args */
    eni_decoder_init(&dec, &eni_decoder_energy_ops, &cfg);
    eni_decode_result_t result;
    rc = eni_decoder_decode(&dec, NULL, &result);
    if (rc != ENI_ERR_INVALID) { FAIL("should reject NULL features"); eni_decoder_shutdown(&dec); return; }
    eni_decoder_shutdown(&dec);
    PASS();
}

/* ── Test: decoder shutdown idempotent ── */
static void test_decoder_shutdown_idempotent(void)
{
    TEST(decoder_shutdown_idempotent);
    eni_decoder_t dec;
    eni_decoder_config_t cfg = {0};
    cfg.num_classes = 4;
    eni_decoder_init(&dec, &eni_decoder_energy_ops, &cfg);
    eni_decoder_shutdown(&dec);
    /* Should not crash when called again */
    eni_decoder_shutdown(&dec);
    /* Shutdown with NULL should not crash */
    eni_decoder_shutdown(NULL);
    PASS();
}

int main(void)
{
    printf("=== ENI Decoder Tests ===\n\n");
    test_energy_decoder_init();
    test_energy_decode_high_energy();
    test_energy_decode_low_energy();
    test_energy_decode_medium_energy();
    test_nn_decoder_init();
    test_nn_decode_heuristic();
    test_decoder_invalid_args();
    test_decoder_shutdown_idempotent();
    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           tests_passed, tests_run, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
