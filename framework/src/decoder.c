// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

#include "eni_fw/decoder.h"
#include "eni/log.h"
#include <string.h>

eni_status_t eni_fw_decoder_init(eni_fw_decoder_t *dec,
                                  const eni_decoder_ops_t *ops,
                                  const eni_decoder_config_t *cfg)
{
    if (!dec || !ops || !cfg) return ENI_ERR_INVALID;

    memset(dec, 0, sizeof(*dec));

    eni_status_t rc = eni_decoder_init(&dec->decoders[0], ops, cfg);
    if (rc != ENI_OK) return rc;

    dec->ops[0] = ops;
    dec->weights[0] = 1.0f;
    dec->decoder_count = 1;
    dec->num_classes = cfg->num_classes;
    dec->ema_alpha = 0.3f;

    rc = eni_mutex_init(&dec->lock);
    if (rc != ENI_OK) {
        eni_decoder_shutdown(&dec->decoders[0]);
        return rc;
    }

    dec->initialized = true;
    ENI_LOG_INFO("fw.dec", "initialized with decoder '%s', classes=%d",
                 ops->name ? ops->name : "unknown", cfg->num_classes);
    return ENI_OK;
}

eni_status_t eni_fw_decoder_add_ensemble(eni_fw_decoder_t *dec,
                                          const eni_decoder_ops_t *ops,
                                          const eni_decoder_config_t *cfg,
                                          float weight)
{
    if (!dec || !dec->initialized || !ops || !cfg) return ENI_ERR_INVALID;

    eni_mutex_lock(&dec->lock);

    if (dec->decoder_count >= ENI_FW_DECODER_MAX_ENSEMBLE) {
        eni_mutex_unlock(&dec->lock);
        return ENI_ERR_OVERFLOW;
    }

    int idx = dec->decoder_count;
    eni_status_t rc = eni_decoder_init(&dec->decoders[idx], ops, cfg);
    if (rc != ENI_OK) {
        eni_mutex_unlock(&dec->lock);
        return rc;
    }

    dec->ops[idx] = ops;
    dec->weights[idx] = weight;
    dec->decoder_count++;

    /* Renormalize weights */
    float sum = 0.0f;
    for (int i = 0; i < dec->decoder_count; i++) sum += dec->weights[i];
    if (sum > 0.0f) {
        for (int i = 0; i < dec->decoder_count; i++) dec->weights[i] /= sum;
    }

    eni_mutex_unlock(&dec->lock);

    ENI_LOG_INFO("fw.dec", "added ensemble decoder '%s' (weight=%.2f, total=%d)",
                 ops->name ? ops->name : "unknown", dec->weights[idx], dec->decoder_count);
    return ENI_OK;
}

eni_status_t eni_fw_decoder_process(eni_fw_decoder_t *dec,
                                     const eni_dsp_features_t *features,
                                     eni_event_t *intent_ev)
{
    if (!dec || !dec->initialized || !features || !intent_ev) return ENI_ERR_INVALID;

    eni_mutex_lock(&dec->lock);

    /* Weighted ensemble decode */
    float combined_confidence[ENI_DECODE_MAX_CLASSES];
    char  class_names[ENI_DECODE_MAX_CLASSES][ENI_EVENT_INTENT_MAX];
    int   total_classes = 0;
    memset(combined_confidence, 0, sizeof(combined_confidence));

    for (int d = 0; d < dec->decoder_count; d++) {
        eni_decode_result_t result;
        memset(&result, 0, sizeof(result));
        eni_status_t rc = eni_decoder_decode(&dec->decoders[d], features, &result);
        if (rc != ENI_OK) continue;

        if (total_classes == 0) {
            total_classes = result.count;
            for (int c = 0; c < result.count && c < ENI_DECODE_MAX_CLASSES; c++) {
                strncpy(class_names[c], result.intents[c].name, ENI_EVENT_INTENT_MAX - 1);
                class_names[c][ENI_EVENT_INTENT_MAX - 1] = '\0';
            }
        }

        for (int c = 0; c < result.count && c < ENI_DECODE_MAX_CLASSES; c++) {
            combined_confidence[c] += result.intents[c].confidence * dec->weights[d];
        }
    }

    if (total_classes == 0) {
        eni_mutex_unlock(&dec->lock);
        return ENI_ERR_RUNTIME;
    }

    /* Apply EMA smoothing */
    int best_idx = 0;
    float best_conf = 0.0f;

    for (int c = 0; c < total_classes && c < dec->num_classes; c++) {
        dec->smoothed_confidence[c] =
            dec->ema_alpha * combined_confidence[c] +
            (1.0f - dec->ema_alpha) * dec->smoothed_confidence[c];

        if (dec->smoothed_confidence[c] > best_conf) {
            best_conf = dec->smoothed_confidence[c];
            best_idx = c;
        }
    }

    /* Update decision history */
    dec->history[dec->history_idx] = best_idx;
    dec->history_idx = (dec->history_idx + 1) % ENI_FW_DECODER_HISTORY_LEN;
    if (dec->history_count < ENI_FW_DECODER_HISTORY_LEN)
        dec->history_count++;

    /* Check temporal consistency — use majority vote from history */
    if (dec->history_count >= 3) {
        int votes[ENI_DECODE_MAX_CLASSES];
        memset(votes, 0, sizeof(votes));
        for (int h = 0; h < dec->history_count; h++) {
            int cls = dec->history[h];
            if (cls >= 0 && cls < ENI_DECODE_MAX_CLASSES)
                votes[cls]++;
        }
        int majority_idx = 0;
        int max_votes = 0;
        for (int c = 0; c < total_classes; c++) {
            if (votes[c] > max_votes) {
                max_votes = votes[c];
                majority_idx = c;
            }
        }
        best_idx = majority_idx;
        best_conf = dec->smoothed_confidence[best_idx];
    }

    dec->inferences_run++;

    eni_mutex_unlock(&dec->lock);

    /* Build output event */
    eni_event_init(intent_ev, ENI_EVENT_INTENT, "fw.decoder");
    eni_event_set_intent(intent_ev, class_names[best_idx], best_conf);

    return ENI_OK;
}

eni_status_t eni_fw_decoder_swap_model(eni_fw_decoder_t *dec,
                                        int index,
                                        const eni_decoder_ops_t *new_ops,
                                        const eni_decoder_config_t *new_cfg)
{
    if (!dec || !dec->initialized || !new_ops || !new_cfg) return ENI_ERR_INVALID;
    if (index < 0 || index >= dec->decoder_count) return ENI_ERR_INVALID;

    eni_mutex_lock(&dec->lock);

    /* Init new decoder first (before shutting down old) */
    eni_decoder_t new_dec;
    memset(&new_dec, 0, sizeof(new_dec));
    eni_status_t rc = eni_decoder_init(&new_dec, new_ops, new_cfg);
    if (rc != ENI_OK) {
        eni_mutex_unlock(&dec->lock);
        return rc;
    }

    /* Shutdown old, swap in new */
    eni_decoder_shutdown(&dec->decoders[index]);
    dec->decoders[index] = new_dec;
    dec->ops[index] = new_ops;
    dec->hot_swaps++;

    eni_mutex_unlock(&dec->lock);

    ENI_LOG_INFO("fw.dec", "hot-swapped decoder[%d] to '%s' (total swaps=%llu)",
                 index, new_ops->name ? new_ops->name : "unknown",
                 (unsigned long long)dec->hot_swaps);
    return ENI_OK;
}

void eni_fw_decoder_set_ema_alpha(eni_fw_decoder_t *dec, float alpha)
{
    if (!dec || !dec->initialized) return;
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    eni_mutex_lock(&dec->lock);
    dec->ema_alpha = alpha;
    eni_mutex_unlock(&dec->lock);
}

void eni_fw_decoder_shutdown(eni_fw_decoder_t *dec)
{
    if (!dec || !dec->initialized) return;

    ENI_LOG_INFO("fw.dec", "shutdown: inferences=%llu hot_swaps=%llu",
                 (unsigned long long)dec->inferences_run,
                 (unsigned long long)dec->hot_swaps);

    for (int d = 0; d < dec->decoder_count; d++) {
        eni_decoder_shutdown(&dec->decoders[d]);
    }
    eni_mutex_destroy(&dec->lock);
    dec->initialized = false;
}
