// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include "eni/nn.h"

static int passed = 0;
#define PASS(name) do { printf("[PASS] %s\n", name); passed++; } while(0)

/* Build a simple 2-input → 2-output dense network with known weights */
static void build_test_model(uint8_t *buf, size_t *len) {
    size_t off = 0;
    /* Header: magic, version, layer_count, input_size, output_size */
    uint32_t magic = ENI_NN_MAGIC;      memcpy(buf + off, &magic, 4); off += 4;
    uint32_t version = 1;                memcpy(buf + off, &version, 4); off += 4;
    uint32_t layers = 1;                 memcpy(buf + off, &layers, 4); off += 4;
    uint32_t input_sz = 2;               memcpy(buf + off, &input_sz, 4); off += 4;
    uint32_t output_sz = 2;              memcpy(buf + off, &output_sz, 4); off += 4;
    /* Layer: type=DENSE, act=SOFTMAX, in=2, out=2, kernel=0 */
    uint32_t type = ENI_NN_DENSE;        memcpy(buf + off, &type, 4); off += 4;
    uint32_t act = ENI_NN_SOFTMAX;       memcpy(buf + off, &act, 4); off += 4;
    uint32_t in2 = 2;                    memcpy(buf + off, &in2, 4); off += 4;
    uint32_t out2 = 2;                   memcpy(buf + off, &out2, 4); off += 4;
    uint32_t kern = 0;                   memcpy(buf + off, &kern, 4); off += 4;
    /* Weights: W[0][0]=1, W[0][1]=0, W[1][0]=0, W[1][1]=1 (identity) */
    float w[] = {1.0f, 0.0f, 0.0f, 1.0f};
    memcpy(buf + off, w, sizeof(w)); off += sizeof(w);
    /* Biases: b[0]=0, b[1]=0 */
    float b[] = {0.0f, 0.0f};
    memcpy(buf + off, b, sizeof(b)); off += sizeof(b);
    *len = off;
}

static void test_nn_load(void) {
    uint8_t buf[512];
    size_t len;
    build_test_model(buf, &len);
    eni_nn_model_t model;
    assert(eni_nn_load(&model, buf, len) == ENI_OK);
    assert(model.layer_count == 1);
    assert(model.input_size == 2);
    assert(model.output_size == 2);
    PASS("nn_load");
}

static void test_nn_load_bad_magic(void) {
    uint8_t buf[64] = {0};
    eni_nn_model_t model;
    assert(eni_nn_load(&model, buf, 64) == ENI_ERR_INVALID);
    PASS("nn_load_bad_magic");
}

static void test_nn_forward_identity(void) {
    uint8_t buf[512];
    size_t len;
    build_test_model(buf, &len);
    eni_nn_model_t model;
    eni_nn_load(&model, buf, len);
    float input[] = {2.0f, 1.0f};
    float output[2];
    assert(eni_nn_forward(&model, input, output, 2) == ENI_OK);
    /* Identity weights → softmax([2,1]) → output[0] > output[1] */
    assert(output[0] > output[1]);
    /* Softmax sums to ~1 */
    float sum = output[0] + output[1];
    assert(sum > 0.99f && sum < 1.01f);
    PASS("nn_forward_identity");
}

static void test_nn_softmax_normalization(void) {
    uint8_t buf[512];
    size_t len;
    build_test_model(buf, &len);
    eni_nn_model_t model;
    eni_nn_load(&model, buf, len);
    float input[] = {0.0f, 0.0f};
    float output[2];
    eni_nn_forward(&model, input, output, 2);
    /* Equal inputs → equal outputs ≈ 0.5 each */
    assert(fabsf(output[0] - 0.5f) < 0.01f);
    assert(fabsf(output[1] - 0.5f) < 0.01f);
    PASS("nn_softmax_normalization");
}

int main(void) {
    printf("=== ENI NN Tests ===\n");
    test_nn_load();
    test_nn_load_bad_magic();
    test_nn_forward_identity();
    test_nn_softmax_normalization();
    printf("\n=== ALL %d TESTS PASSED ===\n", passed);
    return 0;
}
