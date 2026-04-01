// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include "eeg.h"
#include "eni/dsp.h"
#include "eni/decoder.h"
#include "eni/feedback.h"
#include "stimulator_sim.h"

static int passed = 0;
#define PASS(name) do { printf("[PASS] %s\n", name); passed++; } while(0)

static void test_eeg_to_dsp(void) {
    /* Init EEG provider */
    eni_eeg_config_t cfg = {0};
    cfg.channels = 8;
    cfg.sample_rate = 256;
    assert(eni_eeg_init(&cfg) == 0);
    assert(eni_eeg_connect("EEG-PIPELINE") == 0);

    /* Read packets and accumulate into DSP epoch */
    eni_dsp_fft_ctx_t fft_ctx;
    eni_dsp_fft_init(&fft_ctx, 256);
    eni_dsp_epoch_t epoch;
    eni_dsp_epoch_init(&epoch, 256, 256.0f);

    for (int i = 0; i < 256; i++) {
        eni_eeg_packet_t pkt;
        eni_eeg_read_packet(&pkt);
        eni_dsp_epoch_push(&epoch, pkt.channel_data[0]);
    }
    assert(eni_dsp_epoch_ready(&epoch));

    /* Extract features */
    eni_dsp_features_t features;
    assert(eni_dsp_extract_features(&fft_ctx, epoch.samples, epoch.count,
                                     epoch.sample_rate, &features) == ENI_OK);
    assert(features.total_power > 0.0f);

    eni_eeg_deinit();
    PASS("eeg_to_dsp");
}

static void test_dsp_to_decoder(void) {
    /* Create features with known alpha dominance */
    eni_dsp_features_t features = {0};
    features.band_power[0] = 0.1f; /* delta */
    features.band_power[1] = 0.1f; /* theta */
    features.band_power[2] = 0.6f; /* alpha */
    features.band_power[3] = 0.1f; /* beta */
    features.band_power[4] = 0.1f; /* gamma */
    features.total_power = 1.0f;

    /* Decode with NN decoder */
    eni_decoder_t dec;
    eni_decoder_config_t cfg = {0};
    cfg.num_classes = 4;
    eni_decoder_init(&dec, &eni_decoder_nn_ops, &cfg);

    eni_decode_result_t result;
    assert(eni_decoder_decode(&dec, &features, &result) == ENI_OK);
    assert(result.count == 4);
    assert(result.best_idx >= 0);
    assert(result.intents[result.best_idx].confidence > 0.0f);

    eni_decoder_shutdown(&dec);
    PASS("dsp_to_decoder");
}

static void test_decoder_to_feedback(void) {
    /* Setup stimulator */
    eni_stimulator_t stim;
    memset(&stim, 0, sizeof(stim));
    stim.ops = &eni_stimulator_sim_ops;
    eni_stimulator_sim_ops.init(&stim, NULL);

    /* Setup feedback controller */
    eni_feedback_controller_t ctrl;
    eni_feedback_controller_init(&ctrl, &stim, 1.5f, 60000);

    /* Add rule: motor_execute → haptic feedback */
    eni_stim_params_t response = {0};
    response.type = ENI_STIM_HAPTIC;
    response.amplitude = 0.5f;
    response.duration_ms = 200;
    response.frequency_hz = 50.0f;
    eni_feedback_controller_add_rule(&ctrl, "motor_execute", 0.5f, &response);

    /* Simulate intent event */
    eni_event_t intent_ev;
    memset(&intent_ev, 0, sizeof(intent_ev));
    intent_ev.type = ENI_EVENT_INTENT;
    strncpy(intent_ev.payload.intent.name, "motor_execute", ENI_EVENT_INTENT_MAX - 1);
    intent_ev.payload.intent.confidence = 0.92f;

    eni_event_t feedback_ev;
    assert(eni_feedback_controller_evaluate(&ctrl, &intent_ev, &feedback_ev, 10000) == ENI_OK);
    assert(feedback_ev.type == ENI_EVENT_FEEDBACK);
    assert(feedback_ev.payload.feedback.type == ENI_STIM_HAPTIC);

    eni_feedback_controller_shutdown(&ctrl);
    PASS("decoder_to_feedback");
}

static void test_full_pipeline(void) {
    /* 1. EEG Provider → read data */
    eni_eeg_config_t eeg_cfg = {0};
    eeg_cfg.channels = 8;
    eeg_cfg.sample_rate = 256;
    eni_eeg_init(&eeg_cfg);
    eni_eeg_connect("EEG-FULL");

    /* 2. DSP → extract features */
    eni_dsp_fft_ctx_t fft_ctx;
    eni_dsp_fft_init(&fft_ctx, 256);
    eni_dsp_epoch_t epoch;
    eni_dsp_epoch_init(&epoch, 256, 256.0f);
    for (int i = 0; i < 256; i++) {
        eni_eeg_packet_t pkt;
        eni_eeg_read_packet(&pkt);
        eni_dsp_epoch_push(&epoch, pkt.channel_data[0]);
    }
    eni_dsp_features_t features;
    eni_dsp_extract_features(&fft_ctx, epoch.samples, epoch.count, 256.0f, &features);

    /* 3. Decoder → classify intent */
    eni_decoder_t dec;
    eni_decoder_config_t dec_cfg = {0};
    dec_cfg.num_classes = 4;
    eni_decoder_init(&dec, &eni_decoder_nn_ops, &dec_cfg);
    eni_decode_result_t result;
    eni_decoder_decode(&dec, &features, &result);

    /* 4. Feedback → stimulate based on intent */
    eni_stimulator_t stim;
    memset(&stim, 0, sizeof(stim));
    stim.ops = &eni_stimulator_sim_ops;
    eni_stimulator_sim_ops.init(&stim, NULL);

    eni_feedback_controller_t fb;
    eni_feedback_controller_init(&fb, &stim, 1.5f, 60000);
    eni_stim_params_t resp = {0};
    resp.type = ENI_STIM_HAPTIC;
    resp.amplitude = 0.3f;
    resp.duration_ms = 100;
    /* Add rules for all possible intents */
    eni_feedback_controller_add_rule(&fb, "idle", 0.5f, &resp);
    eni_feedback_controller_add_rule(&fb, "attention", 0.5f, &resp);
    eni_feedback_controller_add_rule(&fb, "motor_intent", 0.5f, &resp);
    eni_feedback_controller_add_rule(&fb, "motor_execute", 0.5f, &resp);

    eni_event_t intent_ev;
    memset(&intent_ev, 0, sizeof(intent_ev));
    intent_ev.type = ENI_EVENT_INTENT;
    strncpy(intent_ev.payload.intent.name, result.intents[result.best_idx].name,
            ENI_EVENT_INTENT_MAX - 1);
    intent_ev.payload.intent.confidence = result.intents[result.best_idx].confidence;

    eni_event_t feedback_ev;
    eni_status_t st = eni_feedback_controller_evaluate(&fb, &intent_ev, &feedback_ev, 50000);
    assert(st == ENI_OK);
    assert(feedback_ev.type == ENI_EVENT_FEEDBACK);

    /* Cleanup */
    eni_feedback_controller_shutdown(&fb);
    eni_decoder_shutdown(&dec);
    eni_eeg_deinit();
    PASS("full_pipeline");
}

int main(void) {
    printf("=== ENI BCI Pipeline Integration Tests ===\n");
    test_eeg_to_dsp();
    test_dsp_to_decoder();
    test_decoder_to_feedback();
    test_full_pipeline();
    printf("\n=== ALL %d TESTS PASSED ===\n", passed);
    return 0;
}
