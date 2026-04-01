// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eeg.h"
#include "eni/log.h"
#include <string.h>
#include <math.h>

static struct {
    eni_eeg_config_t cfg;
    eni_eeg_state_t  state;
    int              initialized;
    uint64_t         packet_idx;
    float            calibration_baseline[ENI_EEG_MAX_CHANNELS];
    int              calibrated;
} g_eeg;

/* Standard 10-20 electrode positions (21 electrodes) */
void eni_eeg_montage_standard_10_20(eni_eeg_montage_t *montage) {
    if (!montage) return;
    memset(montage, 0, sizeof(*montage));

    static const struct { const char *label; float x, y, z; } elec[] = {
        {"Fp1", -0.31f,  0.95f, 0.0f},  {"Fp2",  0.31f,  0.95f, 0.0f},
        {"F7",  -0.81f,  0.59f, 0.0f},  {"F3",   -0.55f, 0.67f, 0.5f},
        {"Fz",   0.00f,  0.71f, 0.7f},  {"F4",    0.55f, 0.67f, 0.5f},
        {"F8",   0.81f,  0.59f, 0.0f},  {"T3",   -1.00f, 0.00f, 0.0f},
        {"C3",  -0.55f,  0.00f, 0.8f},  {"Cz",    0.00f, 0.00f, 1.0f},
        {"C4",   0.55f,  0.00f, 0.8f},  {"T4",    1.00f, 0.00f, 0.0f},
        {"T5",  -0.81f, -0.59f, 0.0f},  {"P3",   -0.55f,-0.67f, 0.5f},
        {"Pz",   0.00f, -0.71f, 0.7f},  {"P4",    0.55f,-0.67f, 0.5f},
        {"T6",   0.81f, -0.59f, 0.0f},  {"O1",   -0.31f,-0.95f, 0.0f},
        {"O2",   0.31f, -0.95f, 0.0f},  {"A1",   -1.00f, 0.00f,-0.2f},
        {"A2",   1.00f,  0.00f,-0.2f},
    };
    montage->count = 21;
    for (int i = 0; i < 21; i++) {
        strncpy(montage->electrodes[i].label, elec[i].label, 7);
        montage->electrodes[i].x = elec[i].x;
        montage->electrodes[i].y = elec[i].y;
        montage->electrodes[i].z = elec[i].z;
        montage->electrodes[i].channel_idx = i;
    }
}

static float eeg_bandpass_filter(float sample, float low, float high, float fs) {
    float dt = 1.0f / fs;
    float rc_low = 1.0f / (2.0f * 3.14159f * low);
    float rc_high = 1.0f / (2.0f * 3.14159f * high);
    float alpha_high = dt / (rc_high + dt);
    float alpha_low = rc_low / (rc_low + dt);
    return sample * alpha_high * alpha_low;
}

int eni_eeg_init(const eni_eeg_config_t *cfg) {
    if (!cfg) return -1;
    memset(&g_eeg, 0, sizeof(g_eeg));
    g_eeg.cfg = *cfg;
    g_eeg.state = ENI_EEG_STATE_DISCONNECTED;
    if (g_eeg.cfg.channels == 0) g_eeg.cfg.channels = 21;
    if (g_eeg.cfg.channels > ENI_EEG_MAX_CHANNELS) g_eeg.cfg.channels = ENI_EEG_MAX_CHANNELS;
    if (g_eeg.cfg.sample_rate == 0) g_eeg.cfg.sample_rate = ENI_EEG_SAMPLE_RATE;
    if (g_eeg.cfg.bandpass_low_hz <= 0) g_eeg.cfg.bandpass_low_hz = 0.5f;
    if (g_eeg.cfg.bandpass_high_hz <= 0) g_eeg.cfg.bandpass_high_hz = 100.0f;
    if (g_eeg.cfg.notch_freq_hz <= 0) g_eeg.cfg.notch_freq_hz = 60.0f;
    if (g_eeg.cfg.montage.count == 0) eni_eeg_montage_standard_10_20(&g_eeg.cfg.montage);
    g_eeg.initialized = 1;
    return 0;
}

void eni_eeg_deinit(void) {
    if (g_eeg.state == ENI_EEG_STATE_STREAMING) eni_eeg_disconnect();
    g_eeg.initialized = 0;
}

int eni_eeg_connect(const char *device_addr) {
    if (!g_eeg.initialized) return -1;
    if (g_eeg.state != ENI_EEG_STATE_DISCONNECTED) return -1;
    (void)device_addr;
    g_eeg.state = ENI_EEG_STATE_CONNECTING;
    g_eeg.state = ENI_EEG_STATE_STREAMING;
    return 0;
}

int eni_eeg_disconnect(void) {
    if (!g_eeg.initialized) return -1;
    g_eeg.state = ENI_EEG_STATE_DISCONNECTED;
    return 0;
}

int eni_eeg_read_packet(eni_eeg_packet_t *pkt) {
    if (!g_eeg.initialized || !pkt || g_eeg.state != ENI_EEG_STATE_STREAMING) return -1;
    memset(pkt, 0, sizeof(*pkt));
    pkt->channel_count = g_eeg.cfg.channels;
    pkt->sample_index = (uint32_t)(g_eeg.packet_idx & 0xFFFFFFFF);
    pkt->timestamp_us = g_eeg.packet_idx * (1000000ULL / g_eeg.cfg.sample_rate);

    float fs = (float)g_eeg.cfg.sample_rate;
    float t = (float)g_eeg.packet_idx / fs;
    float pi2 = 2.0f * 3.14159f;

    for (uint32_t ch = 0; ch < pkt->channel_count; ch++) {
        float phase = (float)ch * 0.3f;

        /* Delta: 0.5–4 Hz, ~50µV */
        float delta = 50.0f * sinf(pi2 * 2.0f * t + phase);
        /* Theta: 4–8 Hz, ~30µV */
        float theta = 30.0f * sinf(pi2 * 6.0f * t + phase * 1.1f);
        /* Alpha: 8–13 Hz, ~40µV */
        float alpha = 40.0f * sinf(pi2 * 10.0f * t + phase * 0.9f);
        /* Beta: 13–30 Hz, ~20µV */
        float beta = 20.0f * sinf(pi2 * 20.0f * t + phase * 1.3f);
        /* Gamma: 30–100 Hz, ~10µV */
        float gamma = 10.0f * sinf(pi2 * 40.0f * t + phase * 0.7f);

        float raw = delta + theta + alpha + beta + gamma;

        /* Eye-blink artifact on frontal channels (Fp1=0, Fp2=1) */
        if (ch < 2) {
            uint64_t blink_period = (uint64_t)(fs * 3.0f);
            uint64_t blink_pos = g_eeg.packet_idx % blink_period;
            if (blink_pos < (uint64_t)(fs * 0.15f)) {
                float blink_t = (float)blink_pos / fs;
                raw += 200.0f * expf(-blink_t * 20.0f);
            }
        }

        /* Background noise (~5µV) */
        float noise_seed = (float)(g_eeg.packet_idx * 7 + ch * 13) / 1000.0f;
        noise_seed = noise_seed - (float)(int)noise_seed;
        raw += (noise_seed - 0.5f) * 10.0f;

        if (g_eeg.cfg.filter_enabled)
            raw = eeg_bandpass_filter(raw, g_eeg.cfg.bandpass_low_hz,
                                      g_eeg.cfg.bandpass_high_hz, fs);
        if (g_eeg.calibrated)
            raw -= g_eeg.calibration_baseline[ch];

        pkt->channel_data[ch] = raw;
        pkt->impedance[ch] = 5.0f + (float)(ch % 10) * 0.5f;
    }

    g_eeg.packet_idx++;
    if (g_eeg.cfg.on_packet)
        g_eeg.cfg.on_packet(pkt, g_eeg.cfg.user_ctx);
    return 0;
}

int eni_eeg_calibrate(uint32_t duration_ms) {
    if (!g_eeg.initialized || g_eeg.state != ENI_EEG_STATE_STREAMING) return -1;
    uint32_t samples = (g_eeg.cfg.sample_rate * duration_ms) / 1000;
    if (samples == 0) samples = 64;

    memset(g_eeg.calibration_baseline, 0, sizeof(g_eeg.calibration_baseline));
    for (uint32_t s = 0; s < samples; s++) {
        eni_eeg_packet_t pkt;
        eni_eeg_read_packet(&pkt);
        for (uint32_t ch = 0; ch < pkt.channel_count; ch++)
            g_eeg.calibration_baseline[ch] += pkt.channel_data[ch];
    }
    for (uint32_t ch = 0; ch < g_eeg.cfg.channels; ch++)
        g_eeg.calibration_baseline[ch] /= (float)samples;

    g_eeg.calibrated = 1;
    return 0;
}

int eni_eeg_check_impedance(float *impedance_out, int max_channels) {
    if (!g_eeg.initialized || !impedance_out) return -1;
    eni_eeg_state_t prev = g_eeg.state;
    g_eeg.state = ENI_EEG_STATE_IMPEDANCE_CHECK;

    int count = (int)g_eeg.cfg.channels;
    if (count > max_channels) count = max_channels;
    for (int i = 0; i < count; i++)
        impedance_out[i] = 5.0f + (float)(i % 10) * 0.5f;

    g_eeg.state = prev;
    return count;
}

eni_eeg_state_t eni_eeg_get_state(void) {
    return g_eeg.state;
}

/* Provider vtable integration */
static eni_status_t eeg_prov_init(eni_provider_t *prov, const void *config) {
    (void)prov; (void)config;
    return ENI_OK;
}

static eni_status_t eeg_prov_poll(eni_provider_t *prov, eni_event_t *ev) {
    (void)prov;
    if (!ev) return ENI_ERR_INVALID;
    eni_eeg_packet_t pkt;
    if (eni_eeg_read_packet(&pkt) != 0) return ENI_ERR_TIMEOUT;
    memset(ev, 0, sizeof(*ev));
    ev->type = ENI_EVENT_RAW;
    ev->timestamp.sec = pkt.timestamp_us / 1000000;
    ev->timestamp.nsec = (uint32_t)((pkt.timestamp_us % 1000000) * 1000);
    return ENI_OK;
}

static eni_status_t eeg_prov_start(eni_provider_t *prov) { (void)prov; return ENI_OK; }
static eni_status_t eeg_prov_stop(eni_provider_t *prov)  { (void)prov; return ENI_OK; }
static void eeg_prov_shutdown(eni_provider_t *prov)      { (void)prov; eni_eeg_deinit(); }

static const eni_provider_ops_t g_eeg_provider = {
    .name     = "eeg",
    .init     = eeg_prov_init,
    .poll     = eeg_prov_poll,
    .start    = eeg_prov_start,
    .stop     = eeg_prov_stop,
    .shutdown = eeg_prov_shutdown,
};

const eni_provider_ops_t *eni_eeg_get_provider(void) {
    return &g_eeg_provider;
}
