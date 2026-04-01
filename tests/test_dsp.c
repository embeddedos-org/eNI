// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include "eni/dsp.h"

static int passed = 0;
#define PASS(name) do { printf("[PASS] %s\n", name); passed++; } while(0)

static void test_fft_init(void) {
    eni_dsp_fft_ctx_t ctx;
    assert(eni_dsp_fft_init(&ctx, 256) == ENI_OK);
    assert(ctx.size == 256);
    assert(ctx.initialized == 1);
    assert(eni_dsp_fft_init(&ctx, 100) == ENI_ERR_INVALID); /* not power of 2 */
    PASS("fft_init");
}

static void test_fft_known_sine(void) {
    eni_dsp_fft_ctx_t ctx;
    eni_dsp_fft_init(&ctx, 256);
    float re[256], im[256];
    memset(im, 0, sizeof(im));
    /* 10 Hz sine at 256 Hz sample rate → bin 10 */
    for (int i = 0; i < 256; i++)
        re[i] = sinf(2.0f * 3.14159f * 10.0f * (float)i / 256.0f);
    assert(eni_dsp_fft(&ctx, re, im, 256) == ENI_OK);
    /* Bin 10 should have the highest magnitude */
    float mag10 = sqrtf(re[10]*re[10] + im[10]*im[10]);
    for (int i = 0; i < 128; i++) {
        if (i == 10) continue;
        float mag = sqrtf(re[i]*re[i] + im[i]*im[i]);
        assert(mag10 > mag);
    }
    PASS("fft_known_sine");
}

static void test_psd(void) {
    eni_dsp_fft_ctx_t ctx;
    eni_dsp_fft_init(&ctx, 256);
    float signal[256];
    for (int i = 0; i < 256; i++)
        signal[i] = sinf(2.0f * 3.14159f * 10.0f * (float)i / 256.0f);
    eni_dsp_psd_result_t psd;
    assert(eni_dsp_psd(&ctx, signal, 256, 256.0f, &psd) == ENI_OK);
    assert(psd.bin_count == 128);
    assert(psd.freq_resolution > 0.0f);
    PASS("psd");
}

static void test_band_power(void) {
    eni_dsp_fft_ctx_t ctx;
    eni_dsp_fft_init(&ctx, 256);
    float signal[256];
    /* Generate alpha (10 Hz) dominant signal */
    for (int i = 0; i < 256; i++)
        signal[i] = 40.0f * sinf(2.0f * 3.14159f * 10.0f * (float)i / 256.0f);
    eni_dsp_psd_result_t psd;
    eni_dsp_psd(&ctx, signal, 256, 256.0f, &psd);
    float alpha = eni_dsp_alpha_power(&psd);
    float delta = eni_dsp_delta_power(&psd);
    assert(alpha > delta);
    PASS("band_power");
}

static void test_artifact_detection(void) {
    float signal[64];
    memset(signal, 0, sizeof(signal));
    signal[10] = 500.0f; /* large spike */
    eni_dsp_artifact_t art = eni_dsp_artifact_detect(signal, 64, 50.0f);
    assert(art.eye_blink == true);
    assert(art.severity > 0.0f);
    PASS("artifact_detection");
}

static void test_epoch_buffer(void) {
    eni_dsp_epoch_t epoch;
    eni_dsp_epoch_init(&epoch, 4, 256.0f);
    assert(!eni_dsp_epoch_ready(&epoch));
    eni_dsp_epoch_push(&epoch, 1.0f);
    eni_dsp_epoch_push(&epoch, 2.0f);
    eni_dsp_epoch_push(&epoch, 3.0f);
    assert(!eni_dsp_epoch_ready(&epoch));
    eni_dsp_epoch_push(&epoch, 4.0f);
    assert(eni_dsp_epoch_ready(&epoch));
    eni_dsp_epoch_reset(&epoch);
    assert(!eni_dsp_epoch_ready(&epoch));
    PASS("epoch_buffer");
}

static void test_feature_extraction(void) {
    eni_dsp_fft_ctx_t ctx;
    eni_dsp_fft_init(&ctx, 256);
    float signal[256];
    for (int i = 0; i < 256; i++)
        signal[i] = 40.0f * sinf(2.0f * 3.14159f * 10.0f * (float)i / 256.0f) +
                    20.0f * sinf(2.0f * 3.14159f * 20.0f * (float)i / 256.0f);
    eni_dsp_features_t feat;
    assert(eni_dsp_extract_features(&ctx, signal, 256, 256.0f, &feat) == ENI_OK);
    assert(feat.total_power > 0.0f);
    assert(feat.hjorth_activity > 0.0f);
    assert(feat.spectral_entropy > 0.0f);
    PASS("feature_extraction");
}

int main(void) {
    printf("=== ENI DSP Tests ===\n");
    test_fft_init();
    test_fft_known_sine();
    test_psd();
    test_band_power();
    test_artifact_detection();
    test_epoch_buffer();
    test_feature_extraction();
    printf("\n=== ALL %d TESTS PASSED ===\n", passed);
    return 0;
}
