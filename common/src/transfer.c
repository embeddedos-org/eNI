// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

#include "eni/transfer.h"
#include <string.h>
#include <math.h>

eni_status_t eni_transfer_init(eni_transfer_t *xfer, int num_features)
{
    if (!xfer || num_features <= 0 || num_features > ENI_TRANSFER_MAX_FEATURES)
        return ENI_ERR_INVALID;
    memset(xfer, 0, sizeof(*xfer));
    xfer->num_features = num_features;

    /* Initialize transform to identity */
    for (int i = 0; i < num_features; i++)
        xfer->transform[i * num_features + i] = 1.0f;

    return ENI_OK;
}

static void compute_stats(const float *features, int num_samples, int num_features,
                           float *mean, float *std, float *cov)
{
    /* Compute mean */
    memset(mean, 0, sizeof(float) * (size_t)num_features);
    for (int s = 0; s < num_samples; s++) {
        for (int f = 0; f < num_features; f++)
            mean[f] += features[s * num_features + f];
    }
    for (int f = 0; f < num_features; f++)
        mean[f] /= (float)num_samples;

    /* Compute std */
    memset(std, 0, sizeof(float) * (size_t)num_features);
    for (int s = 0; s < num_samples; s++) {
        for (int f = 0; f < num_features; f++) {
            float d = features[s * num_features + f] - mean[f];
            std[f] += d * d;
        }
    }
    for (int f = 0; f < num_features; f++) {
        std[f] = sqrtf(std[f] / (float)num_samples);
        if (std[f] < 1e-8f) std[f] = 1e-8f;
    }

    /* Compute covariance */
    memset(cov, 0, sizeof(float) * (size_t)(num_features * num_features));
    for (int s = 0; s < num_samples; s++) {
        for (int i = 0; i < num_features; i++) {
            float di = features[s * num_features + i] - mean[i];
            for (int j = 0; j < num_features; j++) {
                float dj = features[s * num_features + j] - mean[j];
                cov[i * num_features + j] += di * dj;
            }
        }
    }
    for (int i = 0; i < num_features * num_features; i++)
        cov[i] /= (float)num_samples;
}

eni_status_t eni_transfer_fit_source(eni_transfer_t *xfer, const float *features,
                                      int num_samples)
{
    if (!xfer || !features || num_samples <= 0) return ENI_ERR_INVALID;
    compute_stats(features, num_samples, xfer->num_features,
                  xfer->src_mean, xfer->src_std, xfer->src_cov);
    return ENI_OK;
}

eni_status_t eni_transfer_fit_target(eni_transfer_t *xfer, const float *features,
                                      int num_samples)
{
    if (!xfer || !features || num_samples <= 0) return ENI_ERR_INVALID;
    compute_stats(features, num_samples, xfer->num_features,
                  xfer->tgt_mean, xfer->tgt_std, xfer->tgt_cov);
    return ENI_OK;
}

eni_status_t eni_transfer_compute_alignment(eni_transfer_t *xfer)
{
    if (!xfer) return ENI_ERR_INVALID;

    int n = xfer->num_features;

    /* Simple alignment: scale by target_std / source_std (diagonal transform)
     * This is a simplified version of covariance alignment.
     * Full Euclidean alignment would require matrix square root. */
    memset(xfer->transform, 0, sizeof(float) * (size_t)(n * n));
    for (int i = 0; i < n; i++) {
        xfer->transform[i * n + i] = xfer->tgt_std[i] / xfer->src_std[i];
    }

    xfer->fitted = true;
    return ENI_OK;
}

eni_status_t eni_transfer_apply(const eni_transfer_t *xfer, const float *input,
                                 float *output, int num_features)
{
    if (!xfer || !xfer->fitted || !input || !output) return ENI_ERR_INVALID;
    if (num_features != xfer->num_features) return ENI_ERR_INVALID;

    int n = xfer->num_features;

    /* Center using source stats, apply transform, shift to target */
    for (int i = 0; i < n; i++) {
        float val = 0.0f;
        for (int j = 0; j < n; j++) {
            val += xfer->transform[i * n + j] * (input[j] - xfer->src_mean[j]);
        }
        output[i] = val + xfer->tgt_mean[i];
    }

    return ENI_OK;
}
