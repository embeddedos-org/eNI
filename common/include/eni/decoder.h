// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef ENI_DECODER_H
#define ENI_DECODER_H

#include "eni/types.h"
#include "eni/event.h"
#include "eni/dsp.h"
#include "eni/config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ENI_DECODE_MAX_CLASSES 16

typedef struct {
    eni_intent_t intents[ENI_DECODE_MAX_CLASSES];
    int          count;
    int          best_idx;
} eni_decode_result_t;

typedef struct eni_decoder_s eni_decoder_t;

typedef struct {
    const char  *name;
    eni_status_t (*init)(eni_decoder_t *dec, const eni_decoder_config_t *cfg);
    eni_status_t (*decode)(eni_decoder_t *dec, const eni_dsp_features_t *features,
                           eni_decode_result_t *result);
    void         (*shutdown)(eni_decoder_t *dec);
} eni_decoder_ops_t;

struct eni_decoder_s {
    char                    name[64];
    const eni_decoder_ops_t *ops;
    void                    *ctx;
};

/* Built-in decoders */
extern const eni_decoder_ops_t eni_decoder_energy_ops;
extern const eni_decoder_ops_t eni_decoder_nn_ops;

eni_status_t eni_decoder_init(eni_decoder_t *dec, const eni_decoder_ops_t *ops,
                              const eni_decoder_config_t *cfg);
eni_status_t eni_decoder_decode(eni_decoder_t *dec, const eni_dsp_features_t *features,
                                eni_decode_result_t *result);
void         eni_decoder_shutdown(eni_decoder_t *dec);

#ifdef __cplusplus
}
#endif

#endif /* ENI_DECODER_H */
