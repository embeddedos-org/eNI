// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023
/**
 * @file test_filter.c
 * @brief Unit tests for ENI-Min event filter
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
    float       min_confidence;
    uint32_t    debounce_ms;
    eni_timestamp_t last_event_time;
    char        last_intent[ENI_EVENT_INTENT_MAX];
} eni_min_filter_t;

/* ---- Stub implementations ---- */
eni_status_t eni_min_filter_init(eni_min_filter_t *filter,
                                  float min_confidence, uint32_t debounce_ms) {
    if (!filter) return ENI_ERR_INVALID;
    memset(filter, 0, sizeof(*filter));
    filter->min_confidence = min_confidence;
    filter->debounce_ms = debounce_ms;
    return ENI_OK;
}

bool eni_min_filter_accept(eni_min_filter_t *filter, const eni_event_t *ev) {
    if (!filter || !ev) return false;
    if (ev->type != ENI_EVENT_INTENT) return true;
    if (ev->payload.intent.confidence < filter->min_confidence) return false;
    /* Debounce: reject same intent within debounce window */
    uint64_t ev_ms = ev->timestamp.sec * 1000 + ev->timestamp.nsec / 1000000;
    uint64_t last_ms = filter->last_event_time.sec * 1000 + filter->last_event_time.nsec / 1000000;
    if (strcmp(filter->last_intent, ev->payload.intent.name) == 0 &&
        (ev_ms - last_ms) < filter->debounce_ms) {
        return false;
    }
    filter->last_event_time = ev->timestamp;
    strncpy(filter->last_intent, ev->payload.intent.name, ENI_EVENT_INTENT_MAX - 1);
    return true;
}

/* ---- Helper ---- */
static eni_event_t make_intent_event(const char *intent, float confidence,
                                      uint64_t sec, uint32_t nsec) {
    eni_event_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = ENI_EVENT_INTENT;
    ev.timestamp.sec = sec;
    ev.timestamp.nsec = nsec;
    strncpy(ev.payload.intent.name, intent, ENI_EVENT_INTENT_MAX - 1);
    ev.payload.intent.confidence = confidence;
    return ev;
}

/* ---- Tests ---- */
static void test_filter_init(void) {
    eni_min_filter_t f;
    eni_status_t rc = eni_min_filter_init(&f, 0.5f, 200);
    assert(rc == ENI_OK);
    assert(f.min_confidence > 0.49f);
    assert(f.debounce_ms == 200);
    PASS("filter_init");
}

static void test_filter_init_null(void) {
    assert(eni_min_filter_init(NULL, 0.5f, 200) == ENI_ERR_INVALID);
    PASS("filter_init_null");
}

static void test_filter_accept_high_confidence(void) {
    eni_min_filter_t f;
    eni_min_filter_init(&f, 0.5f, 200);
    eni_event_t ev = make_intent_event("click", 0.9f, 1, 0);
    assert(eni_min_filter_accept(&f, &ev) == true);
    PASS("filter_accept_high_confidence");
}

static void test_filter_reject_low_confidence(void) {
    eni_min_filter_t f;
    eni_min_filter_init(&f, 0.7f, 200);
    eni_event_t ev = make_intent_event("click", 0.3f, 1, 0);
    assert(eni_min_filter_accept(&f, &ev) == false);
    PASS("filter_reject_low_confidence");
}

static void test_filter_debounce(void) {
    eni_min_filter_t f;
    eni_min_filter_init(&f, 0.5f, 500);
    eni_event_t ev1 = make_intent_event("scroll", 0.8f, 1, 0);
    eni_event_t ev2 = make_intent_event("scroll", 0.8f, 1, 100000000);
    assert(eni_min_filter_accept(&f, &ev1) == true);
    assert(eni_min_filter_accept(&f, &ev2) == false);
    PASS("filter_debounce");
}

static void test_filter_different_intents_no_debounce(void) {
    eni_min_filter_t f;
    eni_min_filter_init(&f, 0.5f, 500);
    eni_event_t ev1 = make_intent_event("click", 0.8f, 1, 0);
    eni_event_t ev2 = make_intent_event("scroll", 0.8f, 1, 100000000);
    assert(eni_min_filter_accept(&f, &ev1) == true);
    assert(eni_min_filter_accept(&f, &ev2) == true);
    PASS("filter_different_intents_no_debounce");
}

static void test_filter_pass_non_intent(void) {
    eni_min_filter_t f;
    eni_min_filter_init(&f, 0.9f, 1000);
    eni_event_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = ENI_EVENT_RAW;
    assert(eni_min_filter_accept(&f, &ev) == true);
    PASS("filter_pass_non_intent");
}

int main(void) {
    printf("=== eni Min Filter Tests ===\n");
    test_filter_init();
    test_filter_init_null();
    test_filter_accept_high_confidence();
    test_filter_reject_low_confidence();
    test_filter_debounce();
    test_filter_different_intents_no_debounce();
    test_filter_pass_non_intent();
    printf("\n=== ALL %d TESTS PASSED ===\n", passed);
    return 0;
}
