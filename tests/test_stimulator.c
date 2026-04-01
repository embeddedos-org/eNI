// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "eni/stimulator.h"
#include "stimulator_sim.h"

static int passed = 0;
#define PASS(name) do { printf("[PASS] %s\n", name); passed++; } while(0)

static void test_sim_init(void) {
    eni_stimulator_t stim;
    memset(&stim, 0, sizeof(stim));
    stim.ops = &eni_stimulator_sim_ops;
    assert(eni_stimulator_sim_ops.init(&stim, NULL) == ENI_OK);
    assert(stim.ctx != NULL);
    eni_stimulator_sim_ops.shutdown(&stim);
    PASS("sim_init");
}

static void test_sim_stimulate(void) {
    eni_stimulator_t stim;
    memset(&stim, 0, sizeof(stim));
    stim.ops = &eni_stimulator_sim_ops;
    eni_stimulator_sim_ops.init(&stim, NULL);

    eni_stim_params_t params = {0};
    params.type = ENI_STIM_HAPTIC;
    params.channel = 1;
    params.amplitude = 0.5f;
    params.duration_ms = 200;
    params.frequency_hz = 50.0f;
    assert(eni_stimulator_sim_ops.stimulate(&stim, &params) == ENI_OK);
    assert(stim.active == true);

    eni_stim_status_t status;
    assert(eni_stimulator_sim_ops.get_status(&stim, &status) == ENI_OK);
    assert(status.active == true);
    assert(status.total_stimulations == 1);

    eni_stimulator_sim_ops.shutdown(&stim);
    PASS("sim_stimulate");
}

static void test_sim_stop(void) {
    eni_stimulator_t stim;
    memset(&stim, 0, sizeof(stim));
    stim.ops = &eni_stimulator_sim_ops;
    eni_stimulator_sim_ops.init(&stim, NULL);

    eni_stim_params_t params = {0};
    params.type = ENI_STIM_VISUAL;
    params.amplitude = 1.0f;
    eni_stimulator_sim_ops.stimulate(&stim, &params);
    assert(stim.active == true);

    assert(eni_stimulator_sim_ops.stop(&stim) == ENI_OK);
    assert(stim.active == false);

    eni_stimulator_sim_ops.shutdown(&stim);
    PASS("sim_stop");
}

static void test_sim_all_types(void) {
    eni_stimulator_t stim;
    memset(&stim, 0, sizeof(stim));
    stim.ops = &eni_stimulator_sim_ops;
    eni_stimulator_sim_ops.init(&stim, NULL);

    eni_stim_type_t types[] = {
        ENI_STIM_VISUAL, ENI_STIM_AUDITORY, ENI_STIM_HAPTIC,
        ENI_STIM_NEURAL, ENI_STIM_TACS, ENI_STIM_TDCS
    };
    for (int i = 0; i < 6; i++) {
        eni_stim_params_t p = {0};
        p.type = types[i];
        p.amplitude = 0.1f;
        assert(eni_stimulator_sim_ops.stimulate(&stim, &p) == ENI_OK);
    }
    eni_stim_status_t status;
    eni_stimulator_sim_ops.get_status(&stim, &status);
    assert(status.total_stimulations == 6);

    eni_stimulator_sim_ops.shutdown(&stim);
    PASS("sim_all_types");
}

int main(void) {
    printf("=== ENI Stimulator Tests ===\n");
    test_sim_init();
    test_sim_stimulate();
    test_sim_stop();
    test_sim_all_types();
    printf("\n=== ALL %d TESTS PASSED ===\n", passed);
    return 0;
}
