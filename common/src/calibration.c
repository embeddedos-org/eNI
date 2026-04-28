// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

#include "eni/calibration.h"
#include "eni/log.h"
#include <string.h>
#include <math.h>

eni_status_t eni_calibration_init(eni_calibration_t *cal, int num_channels, float sample_rate)
{
    if (!cal || num_channels <= 0 || num_channels > ENI_CAL_MAX_CHANNELS) return ENI_ERR_INVALID;
    memset(cal, 0, sizeof(*cal));
    cal->num_channels = num_channels;
    cal->sample_rate = sample_rate;
    cal->stage = ENI_CAL_IDLE;
    cal->impedance_threshold = 50.0f; /* kOhm */
    cal->percentile = 0.95f;

    eni_status_t rc = eni_mutex_init(&cal->lock);
    if (rc != ENI_OK) return rc;

    cal->initialized = true;
    return ENI_OK;
}

eni_status_t eni_calibration_set_callback(eni_calibration_t *cal, eni_cal_progress_cb_t cb, void *data)
{
    if (!cal || !cal->initialized) return ENI_ERR_INVALID;
    cal->progress_cb = cb;
    cal->cb_data = data;
    return ENI_OK;
}

static void notify_progress(eni_calibration_t *cal, float progress)
{
    if (cal->progress_cb) cal->progress_cb(cal->stage, progress, cal->cb_data);
}

eni_status_t eni_calibration_start_impedance(eni_calibration_t *cal)
{
    if (!cal || !cal->initialized) return ENI_ERR_INVALID;
    eni_mutex_lock(&cal->lock);
    cal->stage = ENI_CAL_IMPEDANCE_CHECK;
    notify_progress(cal, 0.0f);
    eni_mutex_unlock(&cal->lock);
    ENI_LOG_INFO("cal", "stage 1/4: impedance check");
    return ENI_OK;
}

eni_status_t eni_calibration_submit_impedance(eni_calibration_t *cal, const float *impedances, int count)
{
    if (!cal || !cal->initialized || !impedances) return ENI_ERR_INVALID;
    if (cal->stage != ENI_CAL_IMPEDANCE_CHECK) return ENI_ERR_RUNTIME;

    eni_mutex_lock(&cal->lock);
    int n = (count < cal->num_channels) ? count : cal->num_channels;
    bool all_ok = true;
    for (int i = 0; i < n; i++) {
        cal->impedances[i] = impedances[i];
        if (impedances[i] > cal->impedance_threshold) all_ok = false;
    }
    notify_progress(cal, 1.0f);
    eni_mutex_unlock(&cal->lock);

    if (!all_ok) {
        ENI_LOG_WARN("cal", "impedance check: some channels exceed threshold");
    }
    return ENI_OK;
}

eni_status_t eni_calibration_start_baseline(eni_calibration_t *cal)
{
    if (!cal || !cal->initialized) return ENI_ERR_INVALID;
    eni_mutex_lock(&cal->lock);
    cal->stage = ENI_CAL_BASELINE;
    cal->baseline_samples = 0;
    memset(cal->baseline_sum, 0, sizeof(cal->baseline_sum));
    memset(cal->baseline_sum_sq, 0, sizeof(cal->baseline_sum_sq));
    notify_progress(cal, 0.0f);
    eni_mutex_unlock(&cal->lock);
    ENI_LOG_INFO("cal", "stage 2/4: baseline recording (%ds)", ENI_CAL_BASELINE_SECONDS);
    return ENI_OK;
}

eni_status_t eni_calibration_push_baseline_sample(eni_calibration_t *cal, const float *samples, int num_channels)
{
    if (!cal || !cal->initialized || !samples) return ENI_ERR_INVALID;
    if (cal->stage != ENI_CAL_BASELINE) return ENI_ERR_RUNTIME;

    eni_mutex_lock(&cal->lock);
    int n = (num_channels < cal->num_channels) ? num_channels : cal->num_channels;
    for (int ch = 0; ch < n; ch++) {
        cal->baseline_sum[ch] += (double)samples[ch];
        cal->baseline_sum_sq[ch] += (double)(samples[ch] * samples[ch]);
    }
    cal->baseline_samples++;

    int target = (int)(cal->sample_rate * ENI_CAL_BASELINE_SECONDS);
    float progress = (target > 0) ? (float)cal->baseline_samples / target : 1.0f;
    if (progress > 1.0f) progress = 1.0f;
    notify_progress(cal, progress);
    eni_mutex_unlock(&cal->lock);
    return ENI_OK;
}

eni_status_t eni_calibration_finalize_baseline(eni_calibration_t *cal)
{
    if (!cal || !cal->initialized) return ENI_ERR_INVALID;
    if (cal->stage != ENI_CAL_BASELINE || cal->baseline_samples == 0) return ENI_ERR_RUNTIME;

    eni_mutex_lock(&cal->lock);
    double n = (double)cal->baseline_samples;
    for (int ch = 0; ch < cal->num_channels; ch++) {
        cal->baseline_mean[ch] = (float)(cal->baseline_sum[ch] / n);
        double var = cal->baseline_sum_sq[ch] / n - (cal->baseline_sum[ch] / n) * (cal->baseline_sum[ch] / n);
        cal->baseline_std[ch] = (float)sqrt(var > 0.0 ? var : 0.0);
    }
    eni_mutex_unlock(&cal->lock);

    ENI_LOG_INFO("cal", "baseline complete: %d samples", cal->baseline_samples);
    return ENI_OK;
}

eni_status_t eni_calibration_compute_thresholds(eni_calibration_t *cal, float percentile)
{
    if (!cal || !cal->initialized) return ENI_ERR_INVALID;

    eni_mutex_lock(&cal->lock);
    cal->stage = ENI_CAL_THRESHOLD;
    cal->percentile = percentile;

    /* Z-score based threshold from baseline statistics */
    /* For 95th percentile, z ≈ 1.645; for 99th percentile z ≈ 2.326 */
    float z;
    if (percentile >= 0.99f) z = 2.326f;
    else if (percentile >= 0.95f) z = 1.645f;
    else if (percentile >= 0.90f) z = 1.282f;
    else z = 1.0f;

    for (int ch = 0; ch < cal->num_channels; ch++) {
        cal->thresholds[ch] = cal->baseline_mean[ch] + z * cal->baseline_std[ch];
    }

    notify_progress(cal, 1.0f);
    eni_mutex_unlock(&cal->lock);

    ENI_LOG_INFO("cal", "stage 3/4: thresholds computed (percentile=%.2f)", percentile);
    return ENI_OK;
}

eni_status_t eni_calibration_start_validation(eni_calibration_t *cal)
{
    if (!cal || !cal->initialized) return ENI_ERR_INVALID;
    eni_mutex_lock(&cal->lock);
    cal->stage = ENI_CAL_VALIDATION;
    cal->validation_trials = 0;
    cal->validation_correct = 0;
    notify_progress(cal, 0.0f);
    eni_mutex_unlock(&cal->lock);
    ENI_LOG_INFO("cal", "stage 4/4: validation");
    return ENI_OK;
}

eni_status_t eni_calibration_submit_trial(eni_calibration_t *cal, bool correct)
{
    if (!cal || !cal->initialized || cal->stage != ENI_CAL_VALIDATION) return ENI_ERR_RUNTIME;

    eni_mutex_lock(&cal->lock);
    cal->validation_trials++;
    if (correct) cal->validation_correct++;
    cal->validation_accuracy = (cal->validation_trials > 0)
        ? (float)cal->validation_correct / (float)cal->validation_trials
        : 0.0f;
    notify_progress(cal, cal->validation_accuracy);
    eni_mutex_unlock(&cal->lock);
    return ENI_OK;
}

eni_status_t eni_calibration_finalize(eni_calibration_t *cal, eni_profile_t *profile)
{
    if (!cal || !cal->initialized) return ENI_ERR_INVALID;

    eni_mutex_lock(&cal->lock);
    if (cal->validation_accuracy < 0.6f && cal->validation_trials > 0) {
        cal->stage = ENI_CAL_FAILED;
        eni_mutex_unlock(&cal->lock);
        ENI_LOG_WARN("cal", "calibration failed: accuracy=%.2f%%", cal->validation_accuracy * 100.0f);
        return ENI_ERR_RUNTIME;
    }

    cal->stage = ENI_CAL_COMPLETE;

    /* Transfer calibration to profile */
    if (profile) {
        for (int ch = 0; ch < cal->num_channels && ch < ENI_PROFILE_MAX_CHANNELS; ch++) {
            float gain = (cal->baseline_std[ch] > 0.0f) ? (1.0f / cal->baseline_std[ch]) : 1.0f;
            eni_profile_set_channel_cal(profile, ch, gain, -cal->baseline_mean[ch], cal->impedances[ch]);
        }
    }

    eni_mutex_unlock(&cal->lock);
    ENI_LOG_INFO("cal", "calibration complete: accuracy=%.1f%% (%d/%d trials)",
                 cal->validation_accuracy * 100.0f,
                 cal->validation_correct, cal->validation_trials);
    return ENI_OK;
}

eni_cal_stage_t eni_calibration_get_stage(const eni_calibration_t *cal)
{
    return cal ? cal->stage : ENI_CAL_IDLE;
}

float eni_calibration_get_accuracy(const eni_calibration_t *cal)
{
    return cal ? cal->validation_accuracy : 0.0f;
}

void eni_calibration_destroy(eni_calibration_t *cal)
{
    if (!cal || !cal->initialized) return;
    eni_mutex_destroy(&cal->lock);
    cal->initialized = false;
}
