// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef ENI_DSP_H
#define ENI_DSP_H

#include "eni/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ENI_DSP_MAX_FFT_SIZE  512
#define ENI_DSP_MAX_PSD_BINS  256
#define ENI_DSP_NUM_BANDS     5

typedef struct {
    float twiddle_re[ENI_DSP_MAX_FFT_SIZE / 2];
    float twiddle_im[ENI_DSP_MAX_FFT_SIZE / 2];
    int   size;
    int   initialized;
} eni_dsp_fft_ctx_t;

typedef struct {
    float bins[ENI_DSP_MAX_PSD_BINS];
    int   bin_count;
    float freq_resolution;
} eni_dsp_psd_result_t;

typedef struct {
    bool  eye_blink;
    bool  muscle;
    bool  movement;
    float severity;
} eni_dsp_artifact_t;

typedef struct {
    float samples[ENI_DSP_MAX_FFT_SIZE];
    int   count;
    int   capacity;
    float sample_rate;
} eni_dsp_epoch_t;

typedef struct {
    float band_power[ENI_DSP_NUM_BANDS]; /* delta, theta, alpha, beta, gamma */
    float total_power;
    float spectral_entropy;
    float hjorth_activity;
    float hjorth_mobility;
    float hjorth_complexity;
} eni_dsp_features_t;

/* FFT */
eni_status_t eni_dsp_fft_init(eni_dsp_fft_ctx_t *ctx, int size);
eni_status_t eni_dsp_fft(eni_dsp_fft_ctx_t *ctx, float *re, float *im, int n);

/* PSD */
eni_status_t eni_dsp_psd(eni_dsp_fft_ctx_t *ctx, const float *signal, int n,
                         float sample_rate, eni_dsp_psd_result_t *result);

/* Band power */
float eni_dsp_band_power(const eni_dsp_psd_result_t *psd, float low_hz, float high_hz);
float eni_dsp_delta_power(const eni_dsp_psd_result_t *psd);
float eni_dsp_theta_power(const eni_dsp_psd_result_t *psd);
float eni_dsp_alpha_power(const eni_dsp_psd_result_t *psd);
float eni_dsp_beta_power(const eni_dsp_psd_result_t *psd);
float eni_dsp_gamma_power(const eni_dsp_psd_result_t *psd);

/* Artifact detection */
eni_dsp_artifact_t eni_dsp_artifact_detect(const float *signal, int n, float threshold);

/* Epoch buffer */
void eni_dsp_epoch_init(eni_dsp_epoch_t *epoch, int capacity, float sample_rate);
void eni_dsp_epoch_push(eni_dsp_epoch_t *epoch, float sample);
bool eni_dsp_epoch_ready(const eni_dsp_epoch_t *epoch);
void eni_dsp_epoch_reset(eni_dsp_epoch_t *epoch);

/* Feature extraction */
eni_status_t eni_dsp_extract_features(eni_dsp_fft_ctx_t *ctx, const float *signal,
                                      int n, float sample_rate, eni_dsp_features_t *features);

#ifdef __cplusplus
}
#endif

#endif /* ENI_DSP_H */
