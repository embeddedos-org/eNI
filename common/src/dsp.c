// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eni/dsp.h"
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

static int is_power_of_2(int n) {
    return n > 0 && (n & (n - 1)) == 0;
}

eni_status_t eni_dsp_fft_init(eni_dsp_fft_ctx_t *ctx, int size) {
    if (!ctx || size > ENI_DSP_MAX_FFT_SIZE || !is_power_of_2(size))
        return ENI_ERR_INVALID;
    ctx->size = size;
    int half = size / 2;
    for (int i = 0; i < half; i++) {
        float angle = -2.0f * (float)M_PI * (float)i / (float)size;
        ctx->twiddle_re[i] = cosf(angle);
        ctx->twiddle_im[i] = sinf(angle);
    }
    ctx->initialized = 1;
    return ENI_OK;
}

eni_status_t eni_dsp_fft(eni_dsp_fft_ctx_t *ctx, float *re, float *im, int n) {
    if (!ctx || !ctx->initialized || !re || !im || n != ctx->size)
        return ENI_ERR_INVALID;

    /* Bit-reversal permutation */
    int bits = 0;
    for (int tmp = n; tmp > 1; tmp >>= 1) bits++;
    for (int i = 0; i < n; i++) {
        int j = 0;
        for (int b = 0; b < bits; b++)
            if (i & (1 << b)) j |= (1 << (bits - 1 - b));
        if (j > i) {
            float tmp_re = re[i]; re[i] = re[j]; re[j] = tmp_re;
            float tmp_im = im[i]; im[i] = im[j]; im[j] = tmp_im;
        }
    }

    /* Iterative Cooley-Tukey */
    for (int len = 2; len <= n; len <<= 1) {
        int half = len / 2;
        int step = n / len;
        for (int i = 0; i < n; i += len) {
            for (int j = 0; j < half; j++) {
                int tw_idx = j * step;
                float tw_re = ctx->twiddle_re[tw_idx];
                float tw_im = ctx->twiddle_im[tw_idx];
                int a = i + j;
                int b = i + j + half;
                float t_re = re[b] * tw_re - im[b] * tw_im;
                float t_im = re[b] * tw_im + im[b] * tw_re;
                re[b] = re[a] - t_re;
                im[b] = im[a] - t_im;
                re[a] = re[a] + t_re;
                im[a] = im[a] + t_im;
            }
        }
    }
    return ENI_OK;
}

eni_status_t eni_dsp_psd(eni_dsp_fft_ctx_t *ctx, const float *signal, int n,
                         float sample_rate, eni_dsp_psd_result_t *result) {
    if (!ctx || !signal || !result || n > ENI_DSP_MAX_FFT_SIZE)
        return ENI_ERR_INVALID;

    float re[ENI_DSP_MAX_FFT_SIZE], im_buf[ENI_DSP_MAX_FFT_SIZE];
    memset(im_buf, 0, sizeof(im_buf));

    /* Hann window */
    for (int i = 0; i < n; i++) {
        float w = 0.5f * (1.0f - cosf(2.0f * (float)M_PI * (float)i / (float)(n - 1)));
        re[i] = signal[i] * w;
    }

    eni_status_t st = eni_dsp_fft(ctx, re, im_buf, n);
    if (st != ENI_OK) return st;

    int bins = n / 2;
    if (bins > ENI_DSP_MAX_PSD_BINS) bins = ENI_DSP_MAX_PSD_BINS;
    result->bin_count = bins;
    result->freq_resolution = sample_rate / (float)n;
    float scale = 1.0f / ((float)n * (float)n);
    for (int i = 0; i < bins; i++) {
        result->bins[i] = (re[i] * re[i] + im_buf[i] * im_buf[i]) * scale;
    }
    return ENI_OK;
}

float eni_dsp_band_power(const eni_dsp_psd_result_t *psd, float low_hz, float high_hz) {
    if (!psd || psd->freq_resolution <= 0.0f) return 0.0f;
    float power = 0.0f;
    for (int i = 0; i < psd->bin_count; i++) {
        float freq = (float)i * psd->freq_resolution;
        if (freq >= low_hz && freq <= high_hz)
            power += psd->bins[i];
    }
    return power;
}

float eni_dsp_delta_power(const eni_dsp_psd_result_t *psd) { return eni_dsp_band_power(psd, 0.5f, 4.0f); }
float eni_dsp_theta_power(const eni_dsp_psd_result_t *psd) { return eni_dsp_band_power(psd, 4.0f, 8.0f); }
float eni_dsp_alpha_power(const eni_dsp_psd_result_t *psd) { return eni_dsp_band_power(psd, 8.0f, 13.0f); }
float eni_dsp_beta_power(const eni_dsp_psd_result_t *psd)  { return eni_dsp_band_power(psd, 13.0f, 30.0f); }
float eni_dsp_gamma_power(const eni_dsp_psd_result_t *psd) { return eni_dsp_band_power(psd, 30.0f, 100.0f); }

eni_dsp_artifact_t eni_dsp_artifact_detect(const float *signal, int n, float threshold) {
    eni_dsp_artifact_t art = {false, false, false, 0.0f};
    if (!signal || n <= 0) return art;

    float max_amp = 0.0f;
    float max_diff = 0.0f;
    for (int i = 0; i < n; i++) {
        float a = fabsf(signal[i]);
        if (a > max_amp) max_amp = a;
        if (i > 0) {
            float d = fabsf(signal[i] - signal[i - 1]);
            if (d > max_diff) max_diff = d;
        }
    }
    if (max_amp > threshold * 3.0f) { art.eye_blink = true; art.severity += 0.4f; }
    if (max_diff > threshold * 2.0f) { art.muscle = true; art.severity += 0.3f; }
    if (max_amp > threshold * 5.0f) { art.movement = true; art.severity += 0.3f; }
    if (art.severity > 1.0f) art.severity = 1.0f;
    return art;
}

void eni_dsp_epoch_init(eni_dsp_epoch_t *epoch, int capacity, float sample_rate) {
    if (!epoch) return;
    memset(epoch, 0, sizeof(*epoch));
    if (capacity > ENI_DSP_MAX_FFT_SIZE) capacity = ENI_DSP_MAX_FFT_SIZE;
    epoch->capacity = capacity;
    epoch->sample_rate = sample_rate;
}

void eni_dsp_epoch_push(eni_dsp_epoch_t *epoch, float sample) {
    if (!epoch || epoch->count >= epoch->capacity) return;
    epoch->samples[epoch->count++] = sample;
}

bool eni_dsp_epoch_ready(const eni_dsp_epoch_t *epoch) {
    return epoch && epoch->count >= epoch->capacity;
}

void eni_dsp_epoch_reset(eni_dsp_epoch_t *epoch) {
    if (epoch) epoch->count = 0;
}

eni_status_t eni_dsp_extract_features(eni_dsp_fft_ctx_t *ctx, const float *signal,
                                      int n, float sample_rate, eni_dsp_features_t *features) {
    if (!ctx || !signal || !features || n <= 0) return ENI_ERR_INVALID;
    memset(features, 0, sizeof(*features));

    eni_dsp_psd_result_t psd;
    eni_status_t st = eni_dsp_psd(ctx, signal, n, sample_rate, &psd);
    if (st != ENI_OK) return st;

    features->band_power[0] = eni_dsp_delta_power(&psd);
    features->band_power[1] = eni_dsp_theta_power(&psd);
    features->band_power[2] = eni_dsp_alpha_power(&psd);
    features->band_power[3] = eni_dsp_beta_power(&psd);
    features->band_power[4] = eni_dsp_gamma_power(&psd);

    features->total_power = 0.0f;
    for (int i = 0; i < ENI_DSP_NUM_BANDS; i++)
        features->total_power += features->band_power[i];

    /* Spectral entropy */
    features->spectral_entropy = 0.0f;
    if (features->total_power > 1e-10f) {
        for (int i = 0; i < ENI_DSP_NUM_BANDS; i++) {
            float p = features->band_power[i] / features->total_power;
            if (p > 1e-10f)
                features->spectral_entropy -= p * log2f(p);
        }
    }

    /* Hjorth parameters */
    float sum = 0.0f, sum_sq = 0.0f;
    for (int i = 0; i < n; i++) { sum += signal[i]; }
    float mean = sum / (float)n;
    for (int i = 0; i < n; i++) {
        float d = signal[i] - mean;
        sum_sq += d * d;
    }
    features->hjorth_activity = sum_sq / (float)n;

    float d1_sq = 0.0f, d2_sq = 0.0f;
    for (int i = 1; i < n; i++) {
        float diff1 = signal[i] - signal[i - 1];
        d1_sq += diff1 * diff1;
    }
    d1_sq /= (float)(n - 1);

    for (int i = 2; i < n; i++) {
        float diff2 = (signal[i] - signal[i - 1]) - (signal[i - 1] - signal[i - 2]);
        d2_sq += diff2 * diff2;
    }
    if (n > 2) d2_sq /= (float)(n - 2);

    features->hjorth_mobility = (features->hjorth_activity > 1e-10f)
        ? sqrtf(d1_sq / features->hjorth_activity) : 0.0f;
    features->hjorth_complexity = (d1_sq > 1e-10f)
        ? sqrtf(d2_sq / d1_sq) / features->hjorth_mobility : 0.0f;

    return ENI_OK;
}
