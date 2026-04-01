// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include "eni/decoder.h"
#include "eni/dsp.h"

static int passed = 0;
#define PASS(name) do { printf("[PASS] %s\n", name); passed++; } while(0)

static void test_energy_decoder_init(void) {
    eni_decoder_t dec;
    eni_decoder_config_t cfg = {0};
    cfg.confidence_threshold = 0.5f;
    assert(eni_decoder_init(&dec, &eni_decoder_energy_ops, &cfg) == ENI_OK);
    assert(strcmp(dec.name, "energy") == 0);
    eni_decoder_shutdown(&dec);
    PASS("energy_decoder_init");
}

static void test_energy_decoder_idle(void) {
    eni_decoder_t dec;
    eni_decoder_config_t cfg = {0};
    cfg.confidence_threshold = 1.0f;
    eni_decoder_init(&dec, &eni_decoder_energy_ops, &cfg);
    eni_dsp_features_t feat = {0}; /* zero features → low energy → idle */
    eni_decode_result_t result;
    assert(eni_decoder_decode(&dec, &feat, &result) == ENI_OK);
    assert(result.count == 4);
    assert(strcmp(result.intents[result.best_idx].name, "idle") == 0);
    eni_decoder_shutdown(&dec);
    PASS("energy_decoder_idle");
}

static void test_energy_decoder_high_energy(void) {
    eni_decoder_t dec;
    eni_decoder_config_t cfg = {0};
    cfg.confidence_threshold = 0.5f;
    eni_decoder_init(&dec, &eni_decoder_energy_ops, &cfg);
    eni_dsp_features_t feat = {0};
    feat.total_power = 100.0f; /* high energy → motor_execute */
    eni_decode_result_t result;
    eni_decoder_decode(&dec, &feat, &result);
    assert(strcmp(result.intents[result.best_idx].name, "motor_execute") == 0);
    eni_decoder_shutdown(&dec);
    PASS("energy_decoder_high_energy");
}

static void test_nn_decoder_init(void) {
    eni_decoder_t dec;
    eni_decoder_config_t cfg = {0};
    cfg.num_classes = 4;
    assert(eni_decoder_init(&dec, &eni_decoder_nn_ops, &cfg) == ENI_OK);
    assert(strcmp(dec.name, "nn") == 0);
    eni_decoder_shutdown(&dec);
    PASS("nn_decoder_init");
}

static void test_nn_decoder_fallback(void) {
    eni_decoder_t dec;
    eni_decoder_config_t cfg = {0};
    cfg.num_classes = 4;
    eni_decoder_init(&dec, &eni_decoder_nn_ops, &cfg);
    eni_dsp_features_t feat = {0};
    feat.band_power[2] = 0.5f; /* some alpha */
    feat.total_power = 1.0f;
    eni_decode_result_t result;
    assert(eni_decoder_decode(&dec, &feat, &result) == ENI_OK);
    assert(result.count == 4);
    assert(result.best_idx >= 0 && result.best_idx < 4);
    /* Confidences should sum to ~1 */
    float sum = 0;
    for (int i = 0; i < result.count; i++) sum += result.intents[i].confidence;
    assert(sum > 0.99f && sum < 1.01f);
    eni_decoder_shutdown(&dec);
    PASS("nn_decoder_fallback");
}

int main(void) {
    printf("=== ENI Decoder Tests ===\n");
    test_energy_decoder_init();
    test_energy_decoder_idle();
    test_energy_decoder_high_energy();
    test_nn_decoder_init();
    test_nn_decoder_fallback();
    printf("\n=== ALL %d TESTS PASSED ===\n", passed);
    return 0;
}
