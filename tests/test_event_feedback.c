// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "eni/event.h"

static int passed = 0;
#define PASS(name) do { printf("[PASS] %s\n", name); passed++; } while(0)

static void test_feedback_event_type(void) {
    assert(ENI_EVENT_FEEDBACK == 4);
    PASS("feedback_event_type");
}

static void test_feedback_payload_init(void) {
    eni_event_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = ENI_EVENT_FEEDBACK;
    ev.payload.feedback.type = ENI_STIM_HAPTIC;
    ev.payload.feedback.channel = 3;
    ev.payload.feedback.amplitude = 0.5f;
    ev.payload.feedback.duration_ms = 200;
    ev.payload.feedback.frequency_hz = 100.0f;

    assert(ev.type == ENI_EVENT_FEEDBACK);
    assert(ev.payload.feedback.type == ENI_STIM_HAPTIC);
    assert(ev.payload.feedback.channel == 3);
    assert(ev.payload.feedback.amplitude > 0.4f && ev.payload.feedback.amplitude < 0.6f);
    assert(ev.payload.feedback.duration_ms == 200);
    assert(ev.payload.feedback.frequency_hz > 99.0f && ev.payload.feedback.frequency_hz < 101.0f);
    PASS("feedback_payload_init");
}

static void test_feedback_union_access(void) {
    eni_event_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = ENI_EVENT_FEEDBACK;
    ev.payload.feedback.type = ENI_STIM_NEURAL;
    ev.payload.feedback.amplitude = 1.5f;

    /* Verify feedback doesn't corrupt when reading the correct union member */
    assert(ev.payload.feedback.type == ENI_STIM_NEURAL);
    assert(ev.payload.feedback.amplitude > 1.4f);
    PASS("feedback_union_access");
}

static void test_stim_type_enum_values(void) {
    assert(ENI_STIM_VISUAL == 0);
    assert(ENI_STIM_AUDITORY == 1);
    assert(ENI_STIM_HAPTIC == 2);
    assert(ENI_STIM_NEURAL == 3);
    assert(ENI_STIM_TACS == 4);
    assert(ENI_STIM_TDCS == 5);
    PASS("stim_type_enum_values");
}

static void test_feedback_all_stim_types(void) {
    eni_stim_type_t types[] = {
        ENI_STIM_VISUAL, ENI_STIM_AUDITORY, ENI_STIM_HAPTIC,
        ENI_STIM_NEURAL, ENI_STIM_TACS, ENI_STIM_TDCS
    };
    for (int i = 0; i < 6; i++) {
        eni_event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = ENI_EVENT_FEEDBACK;
        ev.payload.feedback.type = types[i];
        assert(ev.payload.feedback.type == types[i]);
    }
    PASS("feedback_all_stim_types");
}

int main(void) {
    printf("=== ENI Event Feedback Tests ===\n");
    test_feedback_event_type();
    test_feedback_payload_init();
    test_feedback_union_access();
    test_stim_type_enum_values();
    test_feedback_all_stim_types();
    printf("\n=== ALL %d TESTS PASSED ===\n", passed);
    return 0;
}
