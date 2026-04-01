// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eni/decoder.h"
#include "eni/nn.h"
#include <string.h>
#include <math.h>

eni_status_t eni_decoder_init(eni_decoder_t *dec, const eni_decoder_ops_t *ops,
                              const eni_decoder_config_t *cfg) {
    if (!dec || !ops) return ENI_ERR_INVALID;
    memset(dec, 0, sizeof(*dec));
    dec->ops = ops;
    if (ops->name) strncpy(dec->name, ops->name, 63);
    if (ops->init) return ops->init(dec, cfg);
    return ENI_OK;
}

eni_status_t eni_decoder_decode(eni_decoder_t *dec, const eni_dsp_features_t *features,
                                eni_decode_result_t *result) {
    if (!dec || !dec->ops || !dec->ops->decode || !features || !result)
        return ENI_ERR_INVALID;
    return dec->ops->decode(dec, features, result);
}

void eni_decoder_shutdown(eni_decoder_t *dec) {
    if (!dec || !dec->ops) return;
    if (dec->ops->shutdown) dec->ops->shutdown(dec);
}

/* === Energy Decoder === */
typedef struct {
    float threshold;
} energy_ctx_t;

static eni_status_t energy_init(eni_decoder_t *dec, const eni_decoder_config_t *cfg) {
    static energy_ctx_t ctx;
    ctx.threshold = (cfg && cfg->confidence_threshold > 0) ? cfg->confidence_threshold : 0.5f;
    dec->ctx = &ctx;
    return ENI_OK;
}

static eni_status_t energy_decode(eni_decoder_t *dec, const eni_dsp_features_t *features,
                                  eni_decode_result_t *result) {
    energy_ctx_t *ctx = (energy_ctx_t *)dec->ctx;
    memset(result, 0, sizeof(*result));

    float energy = sqrtf(features->total_power);
    float t = ctx->threshold;

    static const struct { const char *name; float mult; float conf; } classes[] = {
        {"motor_execute", 4.0f, 0.92f},
        {"motor_intent",  2.0f, 0.78f},
        {"attention",     1.0f, 0.65f},
        {"idle",          0.0f, 0.95f},
    };
    int best = 3;
    for (int i = 0; i < 3; i++) {
        if (energy > t * classes[i].mult) { best = i; break; }
    }

    result->count = 4;
    result->best_idx = best;
    for (int i = 0; i < 4; i++) {
        strncpy(result->intents[i].name, classes[i].name, ENI_EVENT_INTENT_MAX - 1);
        result->intents[i].confidence = (i == best) ? classes[i].conf : 0.05f;
    }
    return ENI_OK;
}

static void energy_shutdown(eni_decoder_t *dec) { dec->ctx = NULL; }

const eni_decoder_ops_t eni_decoder_energy_ops = {
    .name = "energy",
    .init = energy_init,
    .decode = energy_decode,
    .shutdown = energy_shutdown,
};

/* === NN Decoder === */
typedef struct {
    eni_nn_model_t model;
    int            loaded;
    int            num_classes;
} nn_ctx_t;

static nn_ctx_t g_nn_ctx;

static const char *nn_class_labels[] = {
    "idle", "attention", "motor_intent", "motor_execute",
    "relax", "focus", "imagine_left", "imagine_right",
    "imagine_forward", "imagine_back", "select", "reject",
    "confirm", "cancel", "scroll_up", "scroll_down"
};

static eni_status_t nn_init(eni_decoder_t *dec, const eni_decoder_config_t *cfg) {
    memset(&g_nn_ctx, 0, sizeof(g_nn_ctx));
    g_nn_ctx.num_classes = (cfg && cfg->num_classes > 0) ? cfg->num_classes : 4;
    if (g_nn_ctx.num_classes > ENI_DECODE_MAX_CLASSES)
        g_nn_ctx.num_classes = ENI_DECODE_MAX_CLASSES;
    dec->ctx = &g_nn_ctx;
    return ENI_OK;
}

static eni_status_t nn_decode(eni_decoder_t *dec, const eni_dsp_features_t *features,
                              eni_decode_result_t *result) {
    nn_ctx_t *ctx = (nn_ctx_t *)dec->ctx;
    memset(result, 0, sizeof(*result));

    float input[ENI_DSP_NUM_BANDS + 5];
    for (int i = 0; i < ENI_DSP_NUM_BANDS; i++) input[i] = features->band_power[i];
    input[5] = features->total_power;
    input[6] = features->spectral_entropy;
    input[7] = features->hjorth_activity;
    input[8] = features->hjorth_mobility;
    input[9] = features->hjorth_complexity;

    float output[ENI_DECODE_MAX_CLASSES];
    memset(output, 0, sizeof(output));

    if (ctx->loaded) {
        eni_nn_forward(&ctx->model, input, output, ctx->num_classes);
    } else {
        /* Fallback: simple heuristic when no model loaded */
        float alpha_ratio = (features->total_power > 1e-10f)
            ? features->band_power[2] / features->total_power : 0.0f;
        output[0] = alpha_ratio;       /* idle */
        output[1] = 1.0f - alpha_ratio; /* attention */
        if (ctx->num_classes > 2) output[2] = features->hjorth_mobility * 0.5f;
        if (ctx->num_classes > 3) output[3] = features->hjorth_activity * 0.01f;
        /* Normalize */
        float sum = 0;
        for (int i = 0; i < ctx->num_classes; i++) sum += output[i];
        if (sum > 0) for (int i = 0; i < ctx->num_classes; i++) output[i] /= sum;
    }

    result->count = ctx->num_classes;
    float best_conf = -1.0f;
    for (int i = 0; i < ctx->num_classes; i++) {
        int label_idx = i < 16 ? i : 0;
        strncpy(result->intents[i].name, nn_class_labels[label_idx], ENI_EVENT_INTENT_MAX - 1);
        result->intents[i].confidence = output[i];
        if (output[i] > best_conf) { best_conf = output[i]; result->best_idx = i; }
    }
    return ENI_OK;
}

static void nn_shutdown(eni_decoder_t *dec) {
    (void)dec;
    memset(&g_nn_ctx, 0, sizeof(g_nn_ctx));
}

const eni_decoder_ops_t eni_decoder_nn_ops = {
    .name = "nn",
    .init = nn_init,
    .decode = nn_decode,
    .shutdown = nn_shutdown,
};
