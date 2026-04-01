// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef ENI_NN_H
#define ENI_NN_H

#include "eni/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ENI_NN_MAX_LAYERS    16
#define ENI_NN_MAX_NEURONS   128
#define ENI_NN_MAX_WEIGHTS   16384
#define ENI_NN_MAGIC         0x454E4E31  /* "ENN1" */

typedef enum {
    ENI_NN_DENSE,
    ENI_NN_CONV1D,
    ENI_NN_BATCHNORM,
} eni_nn_layer_type_t;

typedef enum {
    ENI_NN_RELU,
    ENI_NN_SIGMOID,
    ENI_NN_TANH,
    ENI_NN_SOFTMAX,
    ENI_NN_LINEAR,
} eni_nn_activation_t;

typedef struct {
    eni_nn_layer_type_t type;
    eni_nn_activation_t activation;
    int input_size;
    int output_size;
    int weight_offset;
    int bias_offset;
    int kernel_size;   /* for conv1d */
} eni_nn_layer_t;

typedef struct {
    eni_nn_layer_t layers[ENI_NN_MAX_LAYERS];
    float          weights[ENI_NN_MAX_WEIGHTS];
    int            layer_count;
    int            weight_count;
    int            input_size;
    int            output_size;
} eni_nn_model_t;

eni_status_t eni_nn_load(eni_nn_model_t *model, const uint8_t *data, size_t len);
eni_status_t eni_nn_forward(const eni_nn_model_t *model, const float *input,
                            float *output, int max_output);

#ifdef __cplusplus
}
#endif

#endif /* ENI_NN_H */
