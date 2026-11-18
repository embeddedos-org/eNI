// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023
//
// NOTE — this provider is SIMULATED. It synthesises EEG-like waveforms in
// software; no hardware is attached and no electrode contact is measured.
//   - eni_eeg_connect() accepts only "sim:<name>" addresses, naming the
//     simulated device to stream from. Any other address fails closed.
//   - eni_eeg_packet_t.impedance[] is NaN ("not measured") on every packet
//     and eni_eeg_check_impedance() returns -1 until a real acquisition
//     adapter supplies measured values. Never gate on these values.
// A real headset adapter (e.g. an ADS1299-class front-end) is separate work.

#ifndef ENI_EEG_H
#define ENI_EEG_H

#include "eni/provider_contract.h"
#include "eni/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ENI_EEG_MAX_CHANNELS   64
#define ENI_EEG_SAMPLE_RATE    256
#define ENI_EEG_PACKET_SIZE    64

/* Address scheme accepted by eni_eeg_connect(): "sim:<device-name>". */
#define ENI_EEG_SIM_ADDR_PREFIX     "sim:"
#define ENI_EEG_SIM_ADDR_PREFIX_LEN 4
#define ENI_EEG_MAX_DEVICE_ADDR     64

typedef struct {
    char  label[8];
    float x, y, z;
    int   channel_idx;
} eni_eeg_electrode_t;

typedef struct {
    eni_eeg_electrode_t electrodes[ENI_EEG_MAX_CHANNELS];
    int                 count;
} eni_eeg_montage_t;

typedef enum {
    ENI_EEG_STATE_DISCONNECTED = 0,
    ENI_EEG_STATE_CONNECTING   = 1,
    ENI_EEG_STATE_STREAMING    = 2,
    ENI_EEG_STATE_IMPEDANCE_CHECK = 3,
    ENI_EEG_STATE_ERROR        = 4,
} eni_eeg_state_t;

typedef enum {
    ENI_EEG_REF_COMMON_AVG = 0,
    ENI_EEG_REF_LINKED_EAR = 1,
    ENI_EEG_REF_CZ         = 2,
} eni_eeg_reference_t;

typedef struct {
    float    channel_data[ENI_EEG_MAX_CHANNELS];
    uint32_t channel_count;
    uint32_t sample_index;
    uint64_t timestamp_us;
    /* NaN: electrode impedance is not measured by this (simulated)
     * provider. See eni_eeg_check_impedance(). */
    float    impedance[ENI_EEG_MAX_CHANNELS];
} eni_eeg_packet_t;

typedef struct {
    uint32_t            channels;
    uint32_t            sample_rate;
    eni_eeg_montage_t   montage;
    int                 filter_enabled;
    float               bandpass_low_hz;
    float               bandpass_high_hz;
    float               notch_freq_hz;
    eni_eeg_reference_t reference_type;
    void              (*on_packet)(const eni_eeg_packet_t *pkt, void *ctx);
    void              (*on_intent)(const char *intent, float confidence, void *ctx);
    void               *user_ctx;
} eni_eeg_config_t;

void eni_eeg_montage_standard_10_20(eni_eeg_montage_t *montage);

int  eni_eeg_init(const eni_eeg_config_t *cfg);
void eni_eeg_deinit(void);

/* Connect to a simulated device. device_addr must be "sim:<name>" with a
 * non-empty name. Returns 0 on success; returns -1 and leaves the provider
 * DISCONNECTED (fail closed) for a NULL, empty, malformed, or non-"sim:"
 * address, when not initialised, or when already connected. The selected
 * address is retained until disconnect; see eni_eeg_get_device_addr(). */
int  eni_eeg_connect(const char *device_addr);
int  eni_eeg_disconnect(void);
int  eni_eeg_read_packet(eni_eeg_packet_t *pkt);
int  eni_eeg_calibrate(uint32_t duration_ms);

/* No device attached to this provider measures impedance, so this always
 * returns -1. impedance_out is filled with NaN ("not measured") for
 * max_channels entries when it is safe to do so. A real adapter must
 * replace this with measured values instead of inventing them. */
int  eni_eeg_check_impedance(float *impedance_out, int max_channels);

/* Address passed to the last successful eni_eeg_connect(), or NULL when
 * disconnected. The returned string is owned by the provider. */
const char *eni_eeg_get_device_addr(void);

eni_eeg_state_t eni_eeg_get_state(void);
const eni_provider_ops_t *eni_eeg_get_provider(void);

#ifdef __cplusplus
}
#endif

#endif /* ENI_EEG_H */
