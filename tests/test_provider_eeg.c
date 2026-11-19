// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
//
// Covers eNI #58 (connect must use the address; no invented impedance)
// and #59 (the filter must be a real stateful filter with a working notch).
// The tone-response bounds below fail on the old constant-gain filter,
// which returned every tone at 0.7019x regardless of frequency.
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
    assert(eni_eeg_connect("sim:EEG-001") == 0);
    assert(eni_eeg_get_state() == ENI_EEG_STATE_STREAMING);
    assert(eni_eeg_get_device_addr() != NULL);
    assert(strcmp(eni_eeg_get_device_addr(), "sim:EEG-001") == 0);
    eni_eeg_deinit();
    PASS("eeg_connect");
}

/* #58: anything that is not a named simulated device must fail closed. */
static void test_eeg_connect_rejects_invalid(void) {
    eni_eeg_config_t cfg = {0};
    cfg.channels = 21;
    eni_eeg_init(&cfg);

    /* No device is selected yet, so reads must already fail. */
    eni_eeg_packet_t pkt = {0};
    assert(eni_eeg_read_packet(&pkt) == -1);
    assert(eni_eeg_get_device_addr() == NULL);

    const char *bad[] = {
        NULL, "", "EEG-001", "sim:", "ble:EEG-001", "sim",
    };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        assert(eni_eeg_connect(bad[i]) == -1);
        assert(eni_eeg_get_state() == ENI_EEG_STATE_DISCONNECTED);
        assert(eni_eeg_get_device_addr() == NULL);
        assert(eni_eeg_read_packet(&pkt) == -1);
    }

    /* Over-long address: must not truncate-and-accept. */
    char long_addr[ENI_EEG_MAX_DEVICE_ADDR + 16];
    memset(long_addr, 'x', sizeof(long_addr) - 1);
    long_addr[sizeof(long_addr) - 1] = '\0';
    memcpy(long_addr, "sim:", 4);
    assert(eni_eeg_connect(long_addr) == -1);
    assert(eni_eeg_get_state() == ENI_EEG_STATE_DISCONNECTED);

    /* Connect while already connected also fails (pre-existing rule). */
    assert(eni_eeg_connect("sim:EEG-001") == 0);
    assert(eni_eeg_connect("sim:EEG-002") == -1);
    assert(strcmp(eni_eeg_get_device_addr(), "sim:EEG-001") == 0);

    /* Disconnect clears the selection; a new device can be selected. */
    assert(eni_eeg_disconnect() == 0);
    assert(eni_eeg_get_state() == ENI_EEG_STATE_DISCONNECTED);
    assert(eni_eeg_get_device_addr() == NULL);
    assert(eni_eeg_connect("sim:EEG-002") == 0);
    assert(strcmp(eni_eeg_get_device_addr(), "sim:EEG-002") == 0);

    /* Connect before init fails. */
    eni_eeg_deinit();
    assert(eni_eeg_connect("sim:EEG-001") == -1);
    PASS("eeg_connect_rejects_invalid");
}

static void test_eeg_read_packet(void) {
    eni_eeg_config_t cfg = {0};
    cfg.channels = 21;
    eni_eeg_init(&cfg);
    eni_eeg_connect("sim:EEG-002");
    eni_eeg_packet_t pkt = {0};
    assert(eni_eeg_read_packet(&pkt) == 0);
    assert(pkt.channel_count == 21);
    /* Check non-zero data (simulated signal) */
    int has_nonzero = 0;
    for (uint32_t i = 0; i < pkt.channel_count; i++)
        if (fabsf(pkt.channel_data[i]) > 0.001f) has_nonzero = 1;
    assert(has_nonzero);
    /* #58: impedance is not measured, even while streaming. */
    for (uint32_t i = 0; i < pkt.channel_count; i++)
        assert(isnan(pkt.impedance[i]));
    eni_eeg_deinit();
    PASS("eeg_read_packet");
}

/* #58: no invented contact values, with or without a device connected. */
static void test_eeg_impedance(void) {
    eni_eeg_config_t cfg = {0};
    cfg.channels = 21;
    eni_eeg_init(&cfg);
    float imp[64];
    int rc = eni_eeg_check_impedance(imp, 64);
    assert(rc == -1);
    for (int i = 0; i < 21; i++)
        assert(isnan(imp[i]));

    assert(eni_eeg_connect("sim:EEG-001") == 0);
    rc = eni_eeg_check_impedance(imp, 64);
    assert(rc == -1);
    for (int i = 0; i < 21; i++)
        assert(isnan(imp[i]));

    assert(eni_eeg_check_impedance(NULL, 64) == -1);
    assert(eni_eeg_check_impedance(imp, 0) == -1);
    eni_eeg_deinit();
    PASS("eeg_impedance");
}

static void test_eeg_calibrate(void) {
    eni_eeg_config_t cfg = {0};
    cfg.channels = 8;
    eni_eeg_init(&cfg);
    eni_eeg_connect("sim:EEG-003");
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

/* --- #59: the filter must actually filter ------------------------------ */

static void filter_cfg(eni_eeg_config_t *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->channels = 2;
    cfg->sample_rate = 256;
    cfg->filter_enabled = 1;
    cfg->bandpass_low_hz = 0.5f;
    cfg->bandpass_high_hz = 100.0f;
    cfg->notch_freq_hz = 60.0f;
}

/* Steady-state peak of a unit-amplitude tone on one channel: 2048 samples
 * at 256 Hz, peak measured over the second half (filter settled). */
static float tone_peak(float freq_hz, int channel) {
    float peak = 0.0f;
    for (int n = 0; n < 2048; n++) {
        float x = sinf(2.0f * 3.14159265f * freq_hz * (float)n / 256.0f);
        float y = eni_eeg_filter_sample(channel, x);
        if (n >= 1024 && fabsf(y) > peak) peak = fabsf(y);
    }
    return peak;
}

static void test_eeg_filter_disabled_passthrough(void) {
    eni_eeg_config_t cfg;
    filter_cfg(&cfg);
    cfg.filter_enabled = 0;
    assert(eni_eeg_init(&cfg) == 0);
    for (int n = 0; n < 64; n++) {
        float x = sinf(2.0f * 3.14159265f * 10.0f * (float)n / 256.0f);
        assert(eni_eeg_filter_sample(0, x) == x);
    }
    /* Out-of-range channels pass through untouched. */
    assert(eni_eeg_filter_sample(-1, 1.5f) == 1.5f);
    assert(eni_eeg_filter_sample(ENI_EEG_MAX_CHANNELS, 1.5f) == 1.5f);
    eni_eeg_deinit();
    PASS("eeg_filter_disabled_passthrough");
}

static void test_eeg_filter_tones(void) {
    /* Measured steady-state gains of the new chain (low 0.5 / high 100 /
     * notch 60 Hz, fs 256): 0.05 Hz -> 0.010, 2 Hz -> 0.998, 10 Hz ->
     * 1.000, 60 Hz -> 0.0002, 120 Hz -> 0.076. Bounds below keep at least
     * ~3x margin; the old constant-gain code returned 0.7019 at every
     * frequency and fails all four bounds. */
    struct { float freq, lo, hi; } cases[] = {
        {  0.05f, 0.0f, 0.10f },  /* decade below the lower corner */
        {  2.0f,  0.85f, 1.15f }, /* in-band */
        { 10.0f,  0.85f, 1.15f }, /* in-band */
        { 60.0f,  0.0f,  0.20f }, /* notch frequency */
        { 120.0f, 0.0f,  0.30f }, /* above the upper corner */
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        eni_eeg_config_t cfg;
        filter_cfg(&cfg);
        assert(eni_eeg_init(&cfg) == 0);
        float peak = tone_peak(cases[i].freq, 0);
        printf("    tone %6.2f Hz peak=%.4f (want %.2f..%.2f)\n",
               cases[i].freq, peak, cases[i].lo, cases[i].hi);
        assert(peak >= cases[i].lo && peak <= cases[i].hi);
        eni_eeg_deinit();
    }
    PASS("eeg_filter_tones");
}

static void test_eeg_filter_per_channel_state(void) {
    /* Two tones at once: 10 Hz on ch0, 60 Hz on ch1. With shared state
     * the channels would contaminate each other; with per-channel state
     * ch0 passes and ch1 is notched out. */
    eni_eeg_config_t cfg;
    filter_cfg(&cfg);
    assert(eni_eeg_init(&cfg) == 0);
    float peak0 = 0.0f, peak1 = 0.0f;
    for (int n = 0; n < 2048; n++) {
        float y0 = eni_eeg_filter_sample(0,
            sinf(2.0f * 3.14159265f * 10.0f * (float)n / 256.0f));
        float y1 = eni_eeg_filter_sample(1,
            sinf(2.0f * 3.14159265f * 60.0f * (float)n / 256.0f));
        if (n >= 1024) {
            if (fabsf(y0) > peak0) peak0 = fabsf(y0);
            if (fabsf(y1) > peak1) peak1 = fabsf(y1);
        }
    }
    printf("    ch0 (10 Hz) peak=%.4f, ch1 (60 Hz) peak=%.4f\n", peak0, peak1);
    assert(peak0 >= 0.85f && peak0 <= 1.15f);
    assert(peak1 <= 0.20f);
    eni_eeg_deinit();
    PASS("eeg_filter_per_channel_state");
}

static void test_eeg_filter_reset_on_connect(void) {
    /* The first output for a step input is fully determined by the
     * (zero) filter state; it must be identical after a fresh init and
     * after disconnect + reconnect to another simulated device. */
    eni_eeg_config_t cfg;
    filter_cfg(&cfg);
    assert(eni_eeg_init(&cfg) == 0);
    float first_fresh = eni_eeg_filter_sample(0, 1.0f);
    for (int n = 0; n < 512; n++)
        (void)eni_eeg_filter_sample(0, 1.0f);

    assert(eni_eeg_connect("sim:EEG-001") == 0);
    float first_after_connect = eni_eeg_filter_sample(0, 1.0f);
    assert(fabsf(first_after_connect - first_fresh) < 1e-6f);
    assert(eni_eeg_disconnect() == 0);
    assert(eni_eeg_connect("sim:EEG-002") == 0);
    float first_after_reconnect = eni_eeg_filter_sample(0, 1.0f);
    assert(fabsf(first_after_reconnect - first_fresh) < 1e-6f);
    eni_eeg_deinit();
    PASS("eeg_filter_reset_on_connect");
}

int main(void) {
    printf("=== ENI EEG Provider Tests ===\n");
    test_eeg_init();
    test_eeg_init_null();
    test_eeg_connect();
    test_eeg_connect_rejects_invalid();
    test_eeg_read_packet();
    test_eeg_impedance();
    test_eeg_calibrate();
    test_eeg_montage();
    test_eeg_provider_vtable();
    test_eeg_filter_disabled_passthrough();
    test_eeg_filter_tones();
    test_eeg_filter_per_channel_state();
    test_eeg_filter_reset_on_connect();
    printf("\n=== ALL %d TESTS PASSED ===\n", passed);
    return 0;
}
