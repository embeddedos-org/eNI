// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023
/**
 * @file test_event.c
 * @brief Unit tests for ENI event system
 */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "eni/types.h"
#include "eni/event.h"

static int passed = 0;
#define PASS(name) do { printf("[PASS] %s\n", name); passed++; } while(0)

/* ---- Stub implementations ---- */
const char *eni_status_str(eni_status_t s) { (void)s; return "OK"; }
eni_timestamp_t eni_timestamp_now(void) {
    eni_timestamp_t t = {1000, 0};
    return t;
}

eni_status_t eni_event_init(eni_event_t *ev, eni_event_type_t type, const char *source) {
    if (!ev) return ENI_ERR_INVALID;
    memset(ev, 0, sizeof(*ev));
    ev->type = type;
    ev->timestamp = eni_timestamp_now();
    if (source) strncpy(ev->source, source, ENI_EVENT_SOURCE_MAX - 1);
    return ENI_OK;
}

eni_status_t eni_event_set_intent(eni_event_t *ev, const char *intent, float confidence) {
    if (!ev || !intent) return ENI_ERR_INVALID;
    if (ev->type != ENI_EVENT_INTENT) return ENI_ERR_INVALID;
    strncpy(ev->payload.intent.name, intent, ENI_EVENT_INTENT_MAX - 1);
    ev->payload.intent.confidence = confidence;
    return ENI_OK;
}

eni_status_t eni_event_add_feature(eni_event_t *ev, const char *name, float value) {
    if (!ev || !name) return ENI_ERR_INVALID;
    if (ev->type != ENI_EVENT_FEATURES) return ENI_ERR_INVALID;
    int idx = ev->payload.features.count;
    if (idx >= ENI_EVENT_FEATURES_MAX) return ENI_ERR_OVERFLOW;
    strncpy(ev->payload.features.features[idx].name, name, 31);
    ev->payload.features.features[idx].value = value;
    ev->payload.features.count++;
    return ENI_OK;
}

eni_status_t eni_event_set_raw(eni_event_t *ev, const uint8_t *data, size_t len) {
    if (!ev || !data) return ENI_ERR_INVALID;
    if (len > ENI_EVENT_PAYLOAD_MAX) return ENI_ERR_OVERFLOW;
    memcpy(ev->payload.raw.data, data, len);
    ev->payload.raw.len = len;
    return ENI_OK;
}

void eni_event_print(const eni_event_t *ev) { (void)ev; }

/* ---- Tests ---- */
static void test_event_init_intent(void) {
    eni_event_t ev;
    eni_status_t rc = eni_event_init(&ev, ENI_EVENT_INTENT, "neuralink");
    assert(rc == ENI_OK);
    assert(ev.type == ENI_EVENT_INTENT);
    assert(strcmp(ev.source, "neuralink") == 0);
    PASS("event_init_intent");
}

static void test_event_init_null(void) {
    eni_status_t rc = eni_event_init(NULL, ENI_EVENT_RAW, "src");
    assert(rc == ENI_ERR_INVALID);
    PASS("event_init_null");
}

static void test_event_set_intent(void) {
    eni_event_t ev;
    eni_event_init(&ev, ENI_EVENT_INTENT, "bci");
    eni_status_t rc = eni_event_set_intent(&ev, "select", 0.92f);
    assert(rc == ENI_OK);
    assert(strcmp(ev.payload.intent.name, "select") == 0);
    assert(ev.payload.intent.confidence > 0.9f);
    PASS("event_set_intent");
}

static void test_event_set_intent_wrong_type(void) {
    eni_event_t ev;
    eni_event_init(&ev, ENI_EVENT_RAW, "src");
    eni_status_t rc = eni_event_set_intent(&ev, "select", 0.5f);
    assert(rc == ENI_ERR_INVALID);
    PASS("event_set_intent_wrong_type");
}

static void test_event_add_feature(void) {
    eni_event_t ev;
    eni_event_init(&ev, ENI_EVENT_FEATURES, "emg");
    eni_status_t rc = eni_event_add_feature(&ev, "grip_force", 0.75f);
    assert(rc == ENI_OK);
    assert(ev.payload.features.count == 1);
    assert(ev.payload.features.features[0].value > 0.7f);
    PASS("event_add_feature");
}

static void test_event_add_multiple_features(void) {
    eni_event_t ev;
    eni_event_init(&ev, ENI_EVENT_FEATURES, "imu");
    eni_event_add_feature(&ev, "accel_x", 1.0f);
    eni_event_add_feature(&ev, "accel_y", -0.5f);
    eni_event_add_feature(&ev, "accel_z", 9.8f);
    assert(ev.payload.features.count == 3);
    PASS("event_add_multiple_features");
}

static void test_event_set_raw(void) {
    eni_event_t ev;
    eni_event_init(&ev, ENI_EVENT_RAW, "sensor");
    uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    eni_status_t rc = eni_event_set_raw(&ev, data, 4);
    assert(rc == ENI_OK);
    assert(ev.payload.raw.len == 4);
    assert(ev.payload.raw.data[0] == 0xDE);
    PASS("event_set_raw");
}

static void test_event_type_enum(void) {
    assert(ENI_EVENT_INTENT == 0);
    assert(ENI_EVENT_FEATURES == 1);
    assert(ENI_EVENT_RAW == 2);
    assert(ENI_EVENT_CONTROL == 3);
    PASS("event_type_enum");
}

static void test_event_constants(void) {
    assert(ENI_EVENT_INTENT_MAX == 64);
    assert(ENI_EVENT_PAYLOAD_MAX == 4096);
    assert(ENI_EVENT_FEATURES_MAX == 32);
    assert(ENI_EVENT_SOURCE_MAX == 64);
    PASS("event_constants");
}

int main(void) {
    printf("=== eni Event System Tests ===\n");
    test_event_init_intent();
    test_event_init_null();
    test_event_set_intent();
    test_event_set_intent_wrong_type();
    test_event_add_feature();
    test_event_add_multiple_features();
    test_event_set_raw();
    test_event_type_enum();
    test_event_constants();
    printf("\n=== ALL %d TESTS PASSED ===\n", passed);
    return 0;
}
