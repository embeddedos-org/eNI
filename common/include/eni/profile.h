// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// User profile system

#ifndef ENI_PROFILE_H
#define ENI_PROFILE_H

#include "eni/types.h"
#include "eni/data_format.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ENI_PROFILE_MAX_CHANNELS 64

typedef struct {
    char    user_id[64];
    char    name[128];
    char    device_id[64];

    /* Per-channel calibration */
    float   channel_gains[ENI_PROFILE_MAX_CHANNELS];
    float   channel_offsets[ENI_PROFILE_MAX_CHANNELS];
    float   impedance_baseline[ENI_PROFILE_MAX_CHANNELS];
    int     num_channels;

    /* Decoder preferences */
    char    decoder_model[256];
    int     num_classes;
    float   confidence_threshold;

    /* Session history */
    uint32_t total_sessions;
    uint64_t total_time_ms;

    bool    valid;
} eni_profile_t;

eni_status_t eni_profile_init(eni_profile_t *profile, const char *user_id);
eni_status_t eni_profile_save(const eni_profile_t *profile, const char *path);
eni_status_t eni_profile_load(eni_profile_t *profile, const char *path);
eni_status_t eni_profile_set_channel_cal(eni_profile_t *profile, int channel,
                                          float gain, float offset, float impedance);
void         eni_profile_update_session(eni_profile_t *profile, uint64_t duration_ms);

#ifdef __cplusplus
}
#endif

#endif /* ENI_PROFILE_H */
