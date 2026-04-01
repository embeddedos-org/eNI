// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include "eeg.h"

static int passed = 0;
#define PASS(name) do { printf("[PASS] %s\n", name); passed++; } while(0)

static void test_eeg_init(void) {
    eni_eeg_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.channels = 21;
    cfg.sample_rate = 256;
    assert(eni_eeg_init(&cfg) == 0);
    assert(eni_eeg_get_state() == ENI_EEG_STATE_DISCONNECTED);
    eni_eeg_deinit();
    PASS("eeg_init");
}

static void test_eeg_init_null(void) {
    assert(eni_eeg_init(NULL) == -1);
    PASS("eeg_init_null");
}

static void test_eeg_connect(void) {
    eni_eeg_config_t cfg = {0};
    cfg.channels = 21;
    eni_eeg_init(&cfg);
    assert(eni_eeg_connect("EEG-001") == 0);
    assert(eni_eeg_get_state() == ENI_EEG_STATE_STREAMING);
    eni_eeg_deinit();
    PASS("eeg_connect");
}

static void test_eeg_read_packet(void) {
    eni_eeg_config_t cfg = {0};
    cfg.channels = 21;
    eni_eeg_init(&cfg);
    eni_eeg_connect("EEG-002");
    eni_eeg_packet_t pkt;
    assert(eni_eeg_read_packet(&pkt) == 0);
    assert(pkt.channel_count == 21);
    /* Check non-zero data (simulated signal) */
    int has_nonzero = 0;
    for (uint32_t i = 0; i < pkt.channel_count; i++)
        if (fabsf(pkt.channel_data[i]) > 0.001f) has_nonzero = 1;
    assert(has_nonzero);
    eni_eeg_deinit();
    PASS("eeg_read_packet");
}

static void test_eeg_impedance(void) {
    eni_eeg_config_t cfg = {0};
    cfg.channels = 21;
    eni_eeg_init(&cfg);
    float imp[64];
    int count = eni_eeg_check_impedance(imp, 64);
    assert(count == 21);
    for (int i = 0; i < count; i++)
        assert(imp[i] > 0.0f);
    eni_eeg_deinit();
    PASS("eeg_impedance");
}

static void test_eeg_calibrate(void) {
    eni_eeg_config_t cfg = {0};
    cfg.channels = 8;
    eni_eeg_init(&cfg);
    eni_eeg_connect("EEG-003");
    assert(eni_eeg_calibrate(1000) == 0);
    eni_eeg_deinit();
    PASS("eeg_calibrate");
}

static void test_eeg_montage(void) {
    eni_eeg_montage_t montage;
    eni_eeg_montage_standard_10_20(&montage);
    assert(montage.count == 21);
    assert(strcmp(montage.electrodes[0].label, "Fp1") == 0);
    assert(strcmp(montage.electrodes[1].label, "Fp2") == 0);
    assert(strcmp(montage.electrodes[9].label, "Cz") == 0);
    PASS("eeg_montage");
}

static void test_eeg_provider_vtable(void) {
    const eni_provider_ops_t *ops = eni_eeg_get_provider();
    assert(ops != NULL);
    assert(strcmp(ops->name, "eeg") == 0);
    assert(ops->init != NULL);
    assert(ops->poll != NULL);
    assert(ops->start != NULL);
    assert(ops->stop != NULL);
    assert(ops->shutdown != NULL);
    PASS("eeg_provider_vtable");
}

int main(void) {
    printf("=== ENI EEG Provider Tests ===\n");
    test_eeg_init();
    test_eeg_init_null();
    test_eeg_connect();
    test_eeg_read_packet();
    test_eeg_impedance();
    test_eeg_calibrate();
    test_eeg_montage();
    test_eeg_provider_vtable();
    printf("\n=== ALL %d TESTS PASSED ===\n", passed);
    return 0;
}
