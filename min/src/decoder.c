// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
#include "eni_min/decoder.h"
#include <string.h>

eni_status_t eni_min_decoder_init(eni_min_decoder_t *dec, const eni_decoder_ops_t *ops,
                                  const eni_decoder_config_t *cfg) {
    if (!dec || !ops) return ENI_ERR_INVALID;
    memset(dec, 0, sizeof(*dec));
    eni_status_t st = eni_decoder_init(&dec->decoder, ops, cfg);
    if (st == ENI_OK) dec->initialized = true;
    return st;
}

eni_status_t eni_min_decoder_process(eni_min_decoder_t *dec,
                                     const eni_dsp_features_t *features,
                                     eni_event_t *intent_ev) {
    if (!dec || !dec->initialized || !features || !intent_ev)
        return ENI_ERR_INVALID;

    eni_decode_result_t result;
    eni_status_t st = eni_decoder_decode(&dec->decoder, features, &result);
    if (st != ENI_OK) return st;
    if (result.count <= 0 || result.best_idx < 0) return ENI_ERR_NOT_FOUND;

    memset(intent_ev, 0, sizeof(*intent_ev));
    intent_ev->type = ENI_EVENT_INTENT;
    strncpy(intent_ev->payload.intent.name,
            result.intents[result.best_idx].name, ENI_EVENT_INTENT_MAX - 1);
    intent_ev->payload.intent.confidence = result.intents[result.best_idx].confidence;
    return ENI_OK;
}

void eni_min_decoder_shutdown(eni_min_decoder_t *dec) {
    if (!dec) return;
    if (dec->initialized) eni_decoder_shutdown(&dec->decoder);
    dec->initialized = false;
}
