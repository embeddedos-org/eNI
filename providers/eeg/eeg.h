// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

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
int  eni_eeg_connect(const char *device_addr);
int  eni_eeg_disconnect(void);
int  eni_eeg_read_packet(eni_eeg_packet_t *pkt);
int  eni_eeg_calibrate(uint32_t duration_ms);
int  eni_eeg_check_impedance(float *impedance_out, int max_channels);

eni_eeg_state_t eni_eeg_get_state(void);
const eni_provider_ops_t *eni_eeg_get_provider(void);

#ifdef __cplusplus
}
#endif

#endif /* ENI_EEG_H */
