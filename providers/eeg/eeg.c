// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023
//
// SIMULATED provider: the waveforms below are synthesised in software.
// No hardware is attached; see eeg.h. eni_eeg_connect() accepts only
// "sim:<name>" addresses and fails closed for anything else, and no
// impedance value is ever invented (packets carry NaN, the impedance
// check returns -1).

#include "eeg.h"
#include "eni/log.h"
#include <string.h>
#include <math.h>

#define EEG_PI 3.14159265f

/* One 2nd-order (biquad) section, RBJ cookbook, a0 normalised to 1. */
typedef struct {
    float b0, b1, b2;
    float a1, a2;
} eeg_biquad_t;

typedef struct {
    float x1, x2; /* previous inputs */
    float y1, y2; /* previous outputs */
} eeg_biquad_state_t;

static struct {
    eni_eeg_config_t cfg;
    eni_eeg_state_t  state;
    int              initialized;
    uint64_t         packet_idx;
    float            calibration_baseline[ENI_EEG_MAX_CHANNELS];
    int              calibrated;
    char             device_addr[ENI_EEG_MAX_DEVICE_ADDR];
    /* Filter chain: high-pass at bandpass_low_hz, low-pass at
     * bandpass_high_hz, notch at notch_freq_hz. Coefficients are fixed
     * at init; each channel keeps its own section state. */
    eeg_biquad_t       filt_hp, filt_lp, filt_notch;
    int               filt_notch_enabled;
    eeg_biquad_state_t filt_hp_state[ENI_EEG_MAX_CHANNELS];
    eeg_biquad_state_t filt_lp_state[ENI_EEG_MAX_CHANNELS];
    eeg_biquad_state_t filt_notch_state[ENI_EEG_MAX_CHANNELS];
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

/* --- Stateful filter chain (RBJ cookbook biquads) ----------------------- */
/*
 * The previous implementation kept no state and returned
 * sample * alpha_high * alpha_low — a constant gain, not a filter. The
 * chain below is a real filter: each section carries its own per-channel
 * delay line, so out-of-band tones are attenuated and the configured
 * notch frequency is actually removed. common/dsp was checked first: it
 * offers adaptive (LMS/NLMS/RLS) filters and artifact removal, but no
 * plain IIR band-pass/notch building block, so the biquads live here.
 * Q = 1/sqrt(2) (Butterworth) for the band edges; Q = 30 for the notch.
 */

static void eeg_biquad_lowpass(eeg_biquad_t *c, float fc, float fs) {
    float w0 = 2.0f * EEG_PI * fc / fs;
    float cosw = cosf(w0);
    float alpha = sinf(w0) / (2.0f * 0.70710678f);
    float a0 = 1.0f + alpha;
    c->b0 = ((1.0f - cosw) / 2.0f) / a0;
    c->b1 = (1.0f - cosw) / a0;
    c->b2 = ((1.0f - cosw) / 2.0f) / a0;
    c->a1 = (-2.0f * cosw) / a0;
    c->a2 = (1.0f - alpha) / a0;
}

static void eeg_biquad_highpass(eeg_biquad_t *c, float fc, float fs) {
    float w0 = 2.0f * EEG_PI * fc / fs;
    float cosw = cosf(w0);
    float alpha = sinf(w0) / (2.0f * 0.70710678f);
    float a0 = 1.0f + alpha;
    c->b0 = ((1.0f + cosw) / 2.0f) / a0;
    c->b1 = -(1.0f + cosw) / a0;
    c->b2 = ((1.0f + cosw) / 2.0f) / a0;
    c->a1 = (-2.0f * cosw) / a0;
    c->a2 = (1.0f - alpha) / a0;
}

static void eeg_biquad_notch(eeg_biquad_t *c, float f0, float fs) {
    float w0 = 2.0f * EEG_PI * f0 / fs;
    float cosw = cosf(w0);
    float alpha = sinf(w0) / (2.0f * 30.0f); /* Q = 30: narrow mains notch */
    float a0 = 1.0f + alpha;
    c->b0 = 1.0f / a0;
    c->b1 = (-2.0f * cosw) / a0;
    c->b2 = 1.0f / a0;
    c->a1 = (-2.0f * cosw) / a0;
    c->a2 = (1.0f - alpha) / a0;
}

static float eeg_biquad_run(const eeg_biquad_t *c, eeg_biquad_state_t *s, float x) {
    float y = c->b0 * x + c->b1 * s->x1 + c->b2 * s->x2
            - c->a1 * s->y1 - c->a2 * s->y2;
    s->x2 = s->x1;
    s->x1 = x;
    s->y2 = s->y1;
    s->y1 = y;
    return y;
}

static void eeg_filter_reset(void) {
    memset(g_eeg.filt_hp_state, 0, sizeof(g_eeg.filt_hp_state));
    memset(g_eeg.filt_lp_state, 0, sizeof(g_eeg.filt_lp_state));
    memset(g_eeg.filt_notch_state, 0, sizeof(g_eeg.filt_notch_state));
}

static void eeg_filter_configure(void) {
    float fs = (float)g_eeg.cfg.sample_rate;
    float nyquist = fs / 2.0f;
    eeg_biquad_highpass(&g_eeg.filt_hp, g_eeg.cfg.bandpass_low_hz, fs);
    eeg_biquad_lowpass(&g_eeg.filt_lp, g_eeg.cfg.bandpass_high_hz, fs);
    /* A notch at or above Nyquist cannot exist; disable it there. */
    g_eeg.filt_notch_enabled =
        (g_eeg.cfg.notch_freq_hz > 0.0f && g_eeg.cfg.notch_freq_hz < nyquist);
    if (g_eeg.filt_notch_enabled)
        eeg_biquad_notch(&g_eeg.filt_notch, g_eeg.cfg.notch_freq_hz, fs);
    eeg_filter_reset();
}

float eni_eeg_filter_sample(int channel, float sample) {
    if (!g_eeg.initialized || !g_eeg.cfg.filter_enabled)
        return sample;
    if (channel < 0 || channel >= (int)ENI_EEG_MAX_CHANNELS)
        return sample;
    float y = eeg_biquad_run(&g_eeg.filt_hp,
                             &g_eeg.filt_hp_state[channel], sample);
    y = eeg_biquad_run(&g_eeg.filt_lp,
                       &g_eeg.filt_lp_state[channel], y);
    if (g_eeg.filt_notch_enabled)
        y = eeg_biquad_run(&g_eeg.filt_notch,
                           &g_eeg.filt_notch_state[channel], y);
    return y;
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
    /* Keep the band well-formed and below Nyquist. */
    if (g_eeg.cfg.bandpass_high_hz >= (float)g_eeg.cfg.sample_rate / 2.0f)
        g_eeg.cfg.bandpass_high_hz = (float)g_eeg.cfg.sample_rate * 0.45f;
    if (g_eeg.cfg.bandpass_low_hz >= g_eeg.cfg.bandpass_high_hz)
        g_eeg.cfg.bandpass_low_hz = g_eeg.cfg.bandpass_high_hz / 2.0f;
    if (g_eeg.cfg.notch_freq_hz <= 0) g_eeg.cfg.notch_freq_hz = 60.0f;
    if (g_eeg.cfg.montage.count == 0) eni_eeg_montage_standard_10_20(&g_eeg.cfg.montage);
    eeg_filter_configure();
    g_eeg.initialized = 1;
    return 0;
}

void eni_eeg_deinit(void) {
    if (g_eeg.state == ENI_EEG_STATE_STREAMING) eni_eeg_disconnect();
    g_eeg.initialized = 0;
    g_eeg.device_addr[0] = '\0';
}

int eni_eeg_connect(const char *device_addr) {
    if (!g_eeg.initialized) return -1;
    if (g_eeg.state != ENI_EEG_STATE_DISCONNECTED) return -1;
    /* Fail closed: only a named simulated device can be selected. There
     * is no hardware transport behind this provider, so a bare name, a
     * NULL/empty address, or any other scheme must not report success. */
    if (!device_addr
        || strncmp(device_addr, ENI_EEG_SIM_ADDR_PREFIX,
                   ENI_EEG_SIM_ADDR_PREFIX_LEN) != 0
        || device_addr[ENI_EEG_SIM_ADDR_PREFIX_LEN] == '\0') {
        ENI_LOG_WARN("eeg", "connect rejected: address must be \"sim:<name>\"");
        return -1;
    }
    if (strlen(device_addr) >= sizeof(g_eeg.device_addr)) {
        ENI_LOG_WARN("eeg", "connect rejected: address too long");
        return -1;
    }
    g_eeg.state = ENI_EEG_STATE_CONNECTING;
    strncpy(g_eeg.device_addr, device_addr, sizeof(g_eeg.device_addr) - 1);
    g_eeg.device_addr[sizeof(g_eeg.device_addr) - 1] = '\0';
    /* A new device means a new stream: fresh filter state and timestamps. */
    eeg_filter_reset();
    g_eeg.packet_idx = 0;
    g_eeg.state = ENI_EEG_STATE_STREAMING;
    ENI_LOG_INFO("eeg", "connected to simulated device %s", g_eeg.device_addr);
    return 0;
}

const char *eni_eeg_get_device_addr(void) {
    if (!g_eeg.initialized || g_eeg.state == ENI_EEG_STATE_DISCONNECTED)
        return NULL;
    return g_eeg.device_addr;
}

int eni_eeg_disconnect(void) {
    if (!g_eeg.initialized) return -1;
    g_eeg.state = ENI_EEG_STATE_DISCONNECTED;
    g_eeg.device_addr[0] = '\0';
    eeg_filter_reset();
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
            raw = eni_eeg_filter_sample((int)ch, raw);
        if (g_eeg.calibrated)
            raw -= g_eeg.calibration_baseline[ch];

        pkt->channel_data[ch] = raw;
        /* Not measured: this provider has no electrodes. NAN, never an
         * invented 5.0–9.5 kOhm value. */
        pkt->impedance[ch] = NAN;
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
    if (!g_eeg.initialized || !impedance_out || max_channels <= 0) return -1;
    /* No device behind this provider measures impedance. Report "not
     * measured" (NaN) instead of the invented 5.0–9.5 kOhm values this
     * used to return, including with nothing connected. */
    int count = (int)g_eeg.cfg.channels;
    if (count > max_channels) count = max_channels;
    for (int i = 0; i < count; i++)
        impedance_out[i] = NAN;
    return -1;
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
    return eni_event_set_raw(ev, (const uint8_t *)pkt.channel_data,
                             pkt.channel_count * sizeof(float));
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
