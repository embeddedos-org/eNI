// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Transfer learning — cross-session alignment

#ifndef ENI_TRANSFER_H
#define ENI_TRANSFER_H

#include "eni/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ENI_TRANSFER_MAX_FEATURES 32

typedef struct {
    /* Source and target covariance matrices (flattened) */
    float src_cov[ENI_TRANSFER_MAX_FEATURES * ENI_TRANSFER_MAX_FEATURES];
    float tgt_cov[ENI_TRANSFER_MAX_FEATURES * ENI_TRANSFER_MAX_FEATURES];
    float transform[ENI_TRANSFER_MAX_FEATURES * ENI_TRANSFER_MAX_FEATURES];

    /* Feature space normalization */
    float src_mean[ENI_TRANSFER_MAX_FEATURES];
    float src_std[ENI_TRANSFER_MAX_FEATURES];
    float tgt_mean[ENI_TRANSFER_MAX_FEATURES];
    float tgt_std[ENI_TRANSFER_MAX_FEATURES];

    int   num_features;
    bool  fitted;
} eni_transfer_t;

eni_status_t eni_transfer_init(eni_transfer_t *xfer, int num_features);
eni_status_t eni_transfer_fit_source(eni_transfer_t *xfer, const float *features,
                                      int num_samples);
eni_status_t eni_transfer_fit_target(eni_transfer_t *xfer, const float *features,
                                      int num_samples);
eni_status_t eni_transfer_compute_alignment(eni_transfer_t *xfer);
eni_status_t eni_transfer_apply(const eni_transfer_t *xfer, const float *input,
                                 float *output, int num_features);

#ifdef __cplusplus
}
#endif

#endif /* ENI_TRANSFER_H */
