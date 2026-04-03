// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eni/nn.h"
#include <string.h>
#include <math.h>

static void nn_relu(float *data, int n) {
    for (int i = 0; i < n; i++)
        if (data[i] < 0.0f) data[i] = 0.0f;
}

static void nn_sigmoid(float *data, int n) {
    for (int i = 0; i < n; i++)
        data[i] = 1.0f / (1.0f + expf(-data[i]));
}

static void nn_tanh_act(float *data, int n) {
    for (int i = 0; i < n; i++)
        data[i] = tanhf(data[i]);
}

static void nn_softmax(float *data, int n) {
    float max_val = data[0];
    for (int i = 1; i < n; i++)
        if (data[i] > max_val) max_val = data[i];
    float sum = 0.0f;
    for (int i = 0; i < n; i++) {
        data[i] = expf(data[i] - max_val);
        sum += data[i];
    }
    if (sum > 0.0f)
        for (int i = 0; i < n; i++) data[i] /= sum;
}

static void nn_apply_activation(float *data, int n, eni_nn_activation_t act) { // NOLINT(bugprone-easily-swappable-parameters)
    switch (act) {
    case ENI_NN_RELU:    nn_relu(data, n);     break;
    case ENI_NN_SIGMOID: nn_sigmoid(data, n);  break;
    case ENI_NN_TANH:    nn_tanh_act(data, n); break;
    case ENI_NN_SOFTMAX: nn_softmax(data, n);  break;
    case ENI_NN_LINEAR:  break;
    }
}

eni_status_t eni_nn_load(eni_nn_model_t *model, const uint8_t *data, size_t len) {
    if (!model || !data) return ENI_ERR_INVALID;
    memset(model, 0, sizeof(*model));

    /* Header: magic(4) + version(4) + layer_count(4) + input_size(4) + output_size(4) = 20 */
    if (len < 20) return ENI_ERR_INVALID;
    uint32_t magic;
    memcpy(&magic, data, 4);
    if (magic != ENI_NN_MAGIC) return ENI_ERR_INVALID;

    uint32_t version;
    memcpy(&version, data + 4, 4);
    (void)version;

    memcpy(&model->layer_count, data + 8, 4);
    memcpy(&model->input_size, data + 12, 4);
    memcpy(&model->output_size, data + 16, 4);

    if (model->layer_count > ENI_NN_MAX_LAYERS ||
        model->input_size > ENI_NN_MAX_NEURONS ||
        model->output_size > ENI_NN_MAX_NEURONS)
        return ENI_ERR_INVALID;

    /* Layer descriptors: type(4) + activation(4) + in(4) + out(4) + kernel(4) = 20 per layer */
    size_t offset = 20;
    int weight_offset = 0;
    for (int i = 0; i < model->layer_count; i++) {
        if (offset + 20 > len) return ENI_ERR_INVALID;
        eni_nn_layer_t *l = &model->layers[i];
        uint32_t tmp;
        memcpy(&tmp, data + offset, 4);      l->type = (eni_nn_layer_type_t)tmp;
        memcpy(&tmp, data + offset + 4, 4);  l->activation = (eni_nn_activation_t)tmp;
        memcpy(&l->input_size, data + offset + 8, 4);
        memcpy(&l->output_size, data + offset + 12, 4);
        memcpy(&l->kernel_size, data + offset + 16, 4);

        l->weight_offset = weight_offset;
        int num_weights = l->input_size * l->output_size;
        l->bias_offset = weight_offset + num_weights;
        weight_offset += num_weights + l->output_size;
        offset += 20;
    }
    model->weight_count = weight_offset;
    if (model->weight_count > ENI_NN_MAX_WEIGHTS) return ENI_ERR_INVALID;

    /* Weights (packed float32) */
    size_t weight_bytes = (size_t)model->weight_count * sizeof(float);
    if (offset + weight_bytes > len) return ENI_ERR_INVALID;
    memcpy(model->weights, data + offset, weight_bytes);

    return ENI_OK;
}

eni_status_t eni_nn_forward(const eni_nn_model_t *model, const float *input,
                            float *output, int max_output) {
    if (!model || !input || !output || model->layer_count <= 0)
        return ENI_ERR_INVALID;
    if (max_output < model->output_size) return ENI_ERR_INVALID;

    float buf_a[ENI_NN_MAX_NEURONS], buf_b[ENI_NN_MAX_NEURONS];
    memset(buf_b, 0, sizeof(buf_b));
    memcpy(buf_a, input, (size_t)model->input_size * sizeof(float));

    float *src = buf_a, *dst = buf_b;

    for (int l = 0; l < model->layer_count; l++) {
        const eni_nn_layer_t *layer = &model->layers[l];
        const float *W = &model->weights[layer->weight_offset];
        const float *bias = &model->weights[layer->bias_offset];

        if (layer->type == ENI_NN_DENSE) {
            for (int o = 0; o < layer->output_size; o++) {
                float sum = bias[o];
                for (int i = 0; i < layer->input_size; i++)
                    sum += src[i] * W[o * layer->input_size + i];
                dst[o] = sum;
            }
        } else if (layer->type == ENI_NN_CONV1D) {
            int out_len = layer->input_size - layer->kernel_size + 1;
            if (out_len > layer->output_size) out_len = layer->output_size;
            for (int o = 0; o < out_len; o++) {
                float sum = bias[o < layer->output_size ? o : 0];
                for (int k = 0; k < layer->kernel_size; k++)
                    sum += src[o + k] * W[k];
                dst[o] = sum;
            }
        } else if (layer->type == ENI_NN_BATCHNORM) {
            /* Simplified: (x - mean) / sqrt(var + eps) * gamma + beta */
            for (int i = 0; i < layer->output_size && i < layer->input_size; i++) {
                dst[i] = src[i] * W[i] + bias[i];
            }
        }

        nn_apply_activation(dst, layer->output_size, layer->activation);

        /* Ping-pong */
        float *tmp = src; src = dst; dst = tmp;
    }

    int out_n = model->output_size;
    if (out_n <= 0 || out_n > max_output || out_n > ENI_NN_MAX_NEURONS)
        return ENI_ERR_INVALID;
    memcpy(output, src, (size_t)out_n * sizeof(float));
    return ENI_OK;
}
