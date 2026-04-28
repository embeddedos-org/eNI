// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
#ifndef ENI_FW_DECODER_H
#define ENI_FW_DECODER_H

#include "eni/decoder.h"
#include "eni/dsp.h"
#include "eni/event.h"
#include "eni_platform/platform.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ENI_FW_DECODER_MAX_ENSEMBLE   4
#define ENI_FW_DECODER_HISTORY_LEN    8

typedef struct {
    eni_decoder_t           decoders[ENI_FW_DECODER_MAX_ENSEMBLE];
    const eni_decoder_ops_t *ops[ENI_FW_DECODER_MAX_ENSEMBLE];
    int                     decoder_count;

    /* Ensemble weights (sum to 1.0) */
    float                   weights[ENI_FW_DECODER_MAX_ENSEMBLE];

    /* Confidence smoothing (exponential moving average) */
    float                   ema_alpha;
    float                   smoothed_confidence[ENI_DECODE_MAX_CLASSES];
    int                     num_classes;

    /* Decision history for temporal consistency */
    int                     history[ENI_FW_DECODER_HISTORY_LEN];
    int                     history_idx;
    int                     history_count;

    /* Thread safety */
    eni_mutex_t             lock;
    bool                    initialized;

    /* Stats */
    uint64_t                inferences_run;
    uint64_t                hot_swaps;
} eni_fw_decoder_t;

eni_status_t eni_fw_decoder_init(eni_fw_decoder_t *dec,
                                  const eni_decoder_ops_t *ops,
                                  const eni_decoder_config_t *cfg);

eni_status_t eni_fw_decoder_add_ensemble(eni_fw_decoder_t *dec,
                                          const eni_decoder_ops_t *ops,
                                          const eni_decoder_config_t *cfg,
                                          float weight);

eni_status_t eni_fw_decoder_process(eni_fw_decoder_t *dec,
                                     const eni_dsp_features_t *features,
                                     eni_event_t *intent_ev);

eni_status_t eni_fw_decoder_swap_model(eni_fw_decoder_t *dec,
                                        int index,
                                        const eni_decoder_ops_t *new_ops,
                                        const eni_decoder_config_t *new_cfg);

void eni_fw_decoder_set_ema_alpha(eni_fw_decoder_t *dec, float alpha);
void eni_fw_decoder_shutdown(eni_fw_decoder_t *dec);

#ifdef __cplusplus
}
#endif

#endif /* ENI_FW_DECODER_H */
