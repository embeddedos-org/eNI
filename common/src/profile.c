// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

#include "eni/profile.h"
#include "eni/json.h"
#include "eni/log.h"
#include <string.h>
#include <stdio.h>

eni_status_t eni_profile_init(eni_profile_t *profile, const char *user_id)
{
    if (!profile || !user_id) return ENI_ERR_INVALID;
    memset(profile, 0, sizeof(*profile));
    strncpy(profile->user_id, user_id, sizeof(profile->user_id) - 1);
    profile->confidence_threshold = 0.8f;
    profile->valid = true;
    return ENI_OK;
}

eni_status_t eni_profile_save(const eni_profile_t *profile, const char *path)
{
    if (!profile || !path || !profile->valid) return ENI_ERR_INVALID;

    FILE *fp = fopen(path, "w");
    if (!fp) return ENI_ERR_IO;

    fprintf(fp, "{\n");
    fprintf(fp, "  \"user_id\": \"%s\",\n", profile->user_id);
    fprintf(fp, "  \"name\": \"%s\",\n", profile->name);
    fprintf(fp, "  \"device_id\": \"%s\",\n", profile->device_id);
    fprintf(fp, "  \"num_channels\": %d,\n", profile->num_channels);
    fprintf(fp, "  \"decoder_model\": \"%s\",\n", profile->decoder_model);
    fprintf(fp, "  \"num_classes\": %d,\n", profile->num_classes);
    fprintf(fp, "  \"confidence_threshold\": %.4f,\n", profile->confidence_threshold);
    fprintf(fp, "  \"total_sessions\": %u,\n", profile->total_sessions);
    fprintf(fp, "  \"total_time_ms\": %llu,\n", (unsigned long long)profile->total_time_ms);

    fprintf(fp, "  \"channel_gains\": [");
    for (int i = 0; i < profile->num_channels; i++)
        fprintf(fp, "%s%.6f", i > 0 ? "," : "", profile->channel_gains[i]);
    fprintf(fp, "],\n");

    fprintf(fp, "  \"channel_offsets\": [");
    for (int i = 0; i < profile->num_channels; i++)
        fprintf(fp, "%s%.6f", i > 0 ? "," : "", profile->channel_offsets[i]);
    fprintf(fp, "],\n");

    fprintf(fp, "  \"impedance_baseline\": [");
    for (int i = 0; i < profile->num_channels; i++)
        fprintf(fp, "%s%.2f", i > 0 ? "," : "", profile->impedance_baseline[i]);
    fprintf(fp, "]\n");

    fprintf(fp, "}\n");
    fclose(fp);

    ENI_LOG_INFO("profile", "saved profile '%s' to %s", profile->user_id, path);
    return ENI_OK;
}

eni_status_t eni_profile_load(eni_profile_t *profile, const char *path)
{
    if (!profile || !path) return ENI_ERR_INVALID;

    FILE *fp = fopen(path, "rb");
    if (!fp) return ENI_ERR_IO;

    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (fsize <= 0 || fsize > 1024 * 1024) { fclose(fp); return ENI_ERR_IO; }

    char *buf = (char *)malloc((size_t)fsize + 1);
    if (!buf) { fclose(fp); return ENI_ERR_NOMEM; }

    fread(buf, 1, (size_t)fsize, fp);
    buf[fsize] = '\0';
    fclose(fp);

    eni_json_value_t root;
    eni_status_t rc = eni_json_parse(buf, &root);
    free(buf);
    if (rc != ENI_OK) return rc;

    memset(profile, 0, sizeof(*profile));

    const char *s = eni_json_get_string(&root, "user_id", "");
    strncpy(profile->user_id, s, sizeof(profile->user_id) - 1);

    s = eni_json_get_string(&root, "name", "");
    strncpy(profile->name, s, sizeof(profile->name) - 1);

    s = eni_json_get_string(&root, "device_id", "");
    strncpy(profile->device_id, s, sizeof(profile->device_id) - 1);

    profile->num_channels = (int)eni_json_get_number(&root, "num_channels", 0.0);

    s = eni_json_get_string(&root, "decoder_model", "");
    strncpy(profile->decoder_model, s, sizeof(profile->decoder_model) - 1);

    profile->num_classes = (int)eni_json_get_number(&root, "num_classes", 0.0);
    profile->confidence_threshold = (float)eni_json_get_number(&root, "confidence_threshold", 0.8);
    profile->total_sessions = (uint32_t)eni_json_get_number(&root, "total_sessions", 0.0);
    profile->total_time_ms = (uint64_t)eni_json_get_number(&root, "total_time_ms", 0.0);
    profile->valid = true;

    eni_json_free(&root);
    ENI_LOG_INFO("profile", "loaded profile '%s' from %s", profile->user_id, path);
    return ENI_OK;
}

eni_status_t eni_profile_set_channel_cal(eni_profile_t *profile, int channel,
                                          float gain, float offset, float impedance)
{
    if (!profile || !profile->valid) return ENI_ERR_INVALID;
    if (channel < 0 || channel >= ENI_PROFILE_MAX_CHANNELS) return ENI_ERR_INVALID;

    profile->channel_gains[channel] = gain;
    profile->channel_offsets[channel] = offset;
    profile->impedance_baseline[channel] = impedance;
    if (channel >= profile->num_channels)
        profile->num_channels = channel + 1;
    return ENI_OK;
}

void eni_profile_update_session(eni_profile_t *profile, uint64_t duration_ms)
{
    if (!profile || !profile->valid) return;
    profile->total_sessions++;
    profile->total_time_ms += duration_ms;
}
