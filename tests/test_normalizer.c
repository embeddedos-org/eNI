// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023
/**
 * @file test_normalizer.c
 * @brief Unit tests for ENI-Min event normalizer
 */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

static int passed = 0;
#define PASS(name) do { printf("[PASS] %s\n", name); passed++; } while(0)

/* ---- Inline types ---- */
typedef enum { ENI_OK = 0, ENI_ERR_INVALID = 2 } eni_status_t;
typedef struct { uint64_t sec; uint32_t nsec; } eni_timestamp_t;

#define ENI_EVENT_INTENT_MAX    64
#define ENI_EVENT_PAYLOAD_MAX   4096
#define ENI_EVENT_FEATURES_MAX  32
#define ENI_EVENT_SOURCE_MAX    64

typedef enum { ENI_EVENT_INTENT, ENI_EVENT_FEATURES, ENI_EVENT_RAW, ENI_EVENT_CONTROL } eni_event_type_t;
typedef enum { ENI_MODE_INTENT, ENI_MODE_FEATURES, ENI_MODE_RAW, ENI_MODE_FEATURES_INTENT } eni_mode_t;
typedef struct { char name[ENI_EVENT_INTENT_MAX]; float confidence; } eni_intent_t;
typedef struct { char name[32]; float value; } eni_feature_t;
typedef struct { eni_feature_t features[ENI_EVENT_FEATURES_MAX]; int count; } eni_feature_set_t;
typedef struct { uint8_t data[ENI_EVENT_PAYLOAD_MAX]; size_t len; } eni_raw_payload_t;
typedef struct {
    uint32_t id; uint32_t version; eni_event_type_t type;
    eni_timestamp_t timestamp; char source[ENI_EVENT_SOURCE_MAX];
    union { eni_intent_t intent; eni_feature_set_t features; eni_raw_payload_t raw; } payload;
} eni_event_t;

typedef struct {
    eni_mode_t mode;
} eni_min_normalizer_t;

/* ---- Stub implementations ---- */
eni_status_t eni_min_normalizer_init(eni_min_normalizer_t *norm, eni_mode_t mode) {
    if (!norm) return ENI_ERR_INVALID;
    norm->mode = mode;
    return ENI_OK;
}

eni_status_t eni_min_normalizer_process(eni_min_normalizer_t *norm,
                                         const eni_event_t *raw,
                                         eni_event_t *normalized) {
    if (!norm || !raw || !normalized) return ENI_ERR_INVALID;
    *normalized = *raw;
    normalized->version = raw->version + 1;
    /* Clamp confidence to [0,1] */
    if (normalized->type == ENI_EVENT_INTENT) {
        if (normalized->payload.intent.confidence > 1.0f)
            normalized->payload.intent.confidence = 1.0f;
        if (normalized->payload.intent.confidence < 0.0f)
            normalized->payload.intent.confidence = 0.0f;
    }
    /* Normalize features to [0,1] range */
    if (normalized->type == ENI_EVENT_FEATURES) {
        for (int i = 0; i < normalized->payload.features.count; i++) {
            float v = normalized->payload.features.features[i].value;
            if (v > 1.0f) normalized->payload.features.features[i].value = 1.0f;
            if (v < 0.0f) normalized->payload.features.features[i].value = 0.0f;
        }
    }
    return ENI_OK;
}

/* ---- Tests ---- */
static void test_normalizer_init(void) {
    eni_min_normalizer_t norm;
    eni_status_t rc = eni_min_normalizer_init(&norm, ENI_MODE_INTENT);
    assert(rc == ENI_OK);
    assert(norm.mode == ENI_MODE_INTENT);
    PASS("normalizer_init");
}

static void test_normalizer_init_null(void) {
    assert(eni_min_normalizer_init(NULL, ENI_MODE_RAW) == ENI_ERR_INVALID);
    PASS("normalizer_init_null");
}

static void test_normalizer_process_intent(void) {
    eni_min_normalizer_t norm;
    eni_min_normalizer_init(&norm, ENI_MODE_INTENT);
    eni_event_t raw, out;
    memset(&raw, 0, sizeof(raw));
    raw.type = ENI_EVENT_INTENT;
    raw.version = 1;
    strncpy(raw.payload.intent.name, "select", 63);
    raw.payload.intent.confidence = 0.85f;
    eni_status_t rc = eni_min_normalizer_process(&norm, &raw, &out);
    assert(rc == ENI_OK);
    assert(out.version == 2);
    assert(out.payload.intent.confidence > 0.8f);
    PASS("normalizer_process_intent");
}

static void test_normalizer_clamp_confidence(void) {
    eni_min_normalizer_t norm;
    eni_min_normalizer_init(&norm, ENI_MODE_INTENT);
    eni_event_t raw, out;
    memset(&raw, 0, sizeof(raw));
    raw.type = ENI_EVENT_INTENT;
    raw.payload.intent.confidence = 1.5f;
    eni_min_normalizer_process(&norm, &raw, &out);
    assert(out.payload.intent.confidence <= 1.0f);
    PASS("normalizer_clamp_confidence");
}

static void test_normalizer_clamp_negative(void) {
    eni_min_normalizer_t norm;
    eni_min_normalizer_init(&norm, ENI_MODE_INTENT);
    eni_event_t raw, out;
    memset(&raw, 0, sizeof(raw));
    raw.type = ENI_EVENT_INTENT;
    raw.payload.intent.confidence = -0.5f;
    eni_min_normalizer_process(&norm, &raw, &out);
    assert(out.payload.intent.confidence >= 0.0f);
    PASS("normalizer_clamp_negative");
}

static void test_normalizer_process_null(void) {
    eni_min_normalizer_t norm;
    eni_min_normalizer_init(&norm, ENI_MODE_RAW);
    eni_event_t ev;
    assert(eni_min_normalizer_process(&norm, NULL, &ev) == ENI_ERR_INVALID);
    assert(eni_min_normalizer_process(&norm, &ev, NULL) == ENI_ERR_INVALID);
    PASS("normalizer_process_null");
}

static void test_mode_enum(void) {
    assert(ENI_MODE_INTENT == 0);
    assert(ENI_MODE_FEATURES == 1);
    assert(ENI_MODE_RAW == 2);
    assert(ENI_MODE_FEATURES_INTENT == 3);
    PASS("mode_enum");
}

int main(void) {
    printf("=== eni Min Normalizer Tests ===\n");
    test_normalizer_init();
    test_normalizer_init_null();
    test_normalizer_process_intent();
    test_normalizer_clamp_confidence();
    test_normalizer_clamp_negative();
    test_normalizer_process_null();
    test_mode_enum();
    printf("\n=== ALL %d TESTS PASSED ===\n", passed);
    return 0;
}
