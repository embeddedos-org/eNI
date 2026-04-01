// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
#ifndef ENI_MIN_DECODER_H
#define ENI_MIN_DECODER_H

#include "eni/decoder.h"
#include "eni/dsp.h"
#include "eni/event.h"

typedef struct {
    eni_decoder_t decoder;
    bool          initialized;
} eni_min_decoder_t;

eni_status_t eni_min_decoder_init(eni_min_decoder_t *dec, const eni_decoder_ops_t *ops,
                                  const eni_decoder_config_t *cfg);
eni_status_t eni_min_decoder_process(eni_min_decoder_t *dec,
                                     const eni_dsp_features_t *features,
                                     eni_event_t *intent_ev);
void         eni_min_decoder_shutdown(eni_min_decoder_t *dec);

#endif
