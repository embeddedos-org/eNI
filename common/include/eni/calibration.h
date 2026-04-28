// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Auto-calibration pipeline

#ifndef ENI_CALIBRATION_H
#define ENI_CALIBRATION_H

#include "eni/types.h"
#include "eni/profile.h"
#include "eni_platform/platform.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ENI_CAL_MAX_CHANNELS 64
#define ENI_CAL_BASELINE_SECONDS 30

typedef enum {
    ENI_CAL_IDLE,
    ENI_CAL_IMPEDANCE_CHECK,
    ENI_CAL_BASELINE,
    ENI_CAL_THRESHOLD,
    ENI_CAL_VALIDATION,
    ENI_CAL_COMPLETE,
    ENI_CAL_FAILED,
} eni_cal_stage_t;

typedef void (*eni_cal_progress_cb_t)(eni_cal_stage_t stage, float progress, void *user_data);

typedef struct {
    eni_cal_stage_t   stage;
    int               num_channels;
    float             sample_rate;

    /* Impedance check results */
    float             impedances[ENI_CAL_MAX_CHANNELS];
    float             impedance_threshold;

    /* Baseline (resting state) */
    float             baseline_mean[ENI_CAL_MAX_CHANNELS];
    float             baseline_std[ENI_CAL_MAX_CHANNELS];
    double            baseline_sum[ENI_CAL_MAX_CHANNELS];
    double            baseline_sum_sq[ENI_CAL_MAX_CHANNELS];
    int               baseline_samples;

    /* Threshold (percentile-based) */
    float             thresholds[ENI_CAL_MAX_CHANNELS];
    float             percentile;

    /* Validation */
    int               validation_trials;
    int               validation_correct;
    float             validation_accuracy;

    /* Callbacks */
    eni_cal_progress_cb_t progress_cb;
    void                 *cb_data;

    eni_mutex_t       lock;
    bool              initialized;
} eni_calibration_t;

eni_status_t eni_calibration_init(eni_calibration_t *cal, int num_channels, float sample_rate);
eni_status_t eni_calibration_set_callback(eni_calibration_t *cal, eni_cal_progress_cb_t cb, void *data);

/* Stage transitions */
eni_status_t eni_calibration_start_impedance(eni_calibration_t *cal);
eni_status_t eni_calibration_submit_impedance(eni_calibration_t *cal, const float *impedances, int count);
eni_status_t eni_calibration_start_baseline(eni_calibration_t *cal);
eni_status_t eni_calibration_push_baseline_sample(eni_calibration_t *cal, const float *samples, int num_channels);
eni_status_t eni_calibration_finalize_baseline(eni_calibration_t *cal);
eni_status_t eni_calibration_compute_thresholds(eni_calibration_t *cal, float percentile);
eni_status_t eni_calibration_start_validation(eni_calibration_t *cal);
eni_status_t eni_calibration_submit_trial(eni_calibration_t *cal, bool correct);
eni_status_t eni_calibration_finalize(eni_calibration_t *cal, eni_profile_t *profile);

eni_cal_stage_t eni_calibration_get_stage(const eni_calibration_t *cal);
float           eni_calibration_get_accuracy(const eni_calibration_t *cal);
void            eni_calibration_destroy(eni_calibration_t *cal);

#ifdef __cplusplus
}
#endif

#endif /* ENI_CALIBRATION_H */
