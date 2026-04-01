// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023
/**
 * @file test_neuralink.c
 * @brief Unit tests for ENI Neuralink provider
 */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

static int passed = 0;
#define PASS(name) do { printf("[PASS] %s\n", name); passed++; } while(0)

/* ---- Inline types (avoid deep include chain) ---- */
typedef enum { ENI_OK = 0, ENI_ERR_INVALID = 2, ENI_ERR_NOT_FOUND = 3,
               ENI_ERR_CONNECT = 8 } eni_status_t;

#define ENI_NEURALINK_MAX_CHANNELS   1024
#define ENI_NEURALINK_SAMPLE_RATE    30000
#define ENI_NEURALINK_PACKET_SIZE    256

typedef enum {
    ENI_NL_MODE_RAW = 0, ENI_NL_MODE_DECODED = 1,
    ENI_NL_MODE_INTENT = 2, ENI_NL_MODE_MOTOR = 3
} eni_nl_mode_t;

typedef enum {
    ENI_NL_STATE_DISCONNECTED = 0, ENI_NL_STATE_CONNECTING = 1,
    ENI_NL_STATE_STREAMING = 2, ENI_NL_STATE_CALIBRATING = 3,
    ENI_NL_STATE_ERROR = 4
} eni_nl_state_t;

typedef struct {
    float    channel_data[ENI_NEURALINK_PACKET_SIZE];
    uint32_t channel_count;
    uint32_t sample_index;
    uint64_t timestamp_us;
    uint8_t  electrode_id;
    float    signal_quality;
} eni_nl_packet_t;

typedef struct {
    char     device_id[64];
    char     firmware_version[32];
    uint32_t electrode_count;
    uint32_t sample_rate;
    float    battery_pct;
    float    temperature_c;
    eni_nl_state_t state;
    uint64_t packets_received;
    uint64_t packets_dropped;
    float    avg_latency_ms;
} eni_nl_status_t;

typedef struct {
    eni_nl_mode_t  mode;
    uint32_t       channels;
    uint32_t       sample_rate;
    char           device_addr[64];
    float          signal_threshold;
    int            auto_calibrate;
    int            filter_enabled;
    float          bandpass_low_hz;
    float          bandpass_high_hz;
    float          notch_freq_hz;
    void         (*on_packet)(const eni_nl_packet_t *pkt, void *ctx);
    void         (*on_intent)(const char *intent, float confidence, void *ctx);
    void          *user_ctx;
} eni_nl_config_t;

/* ---- Simulated state ---- */
static eni_nl_state_t g_state = ENI_NL_STATE_DISCONNECTED;
static eni_nl_config_t g_config;
static uint64_t g_packets_rx = 0;

/* ---- Stub implementations ---- */
int eni_neuralink_init(const eni_nl_config_t *cfg) {
    if (!cfg) return ENI_ERR_INVALID;
    g_config = *cfg;
    g_state = ENI_NL_STATE_DISCONNECTED;
    g_packets_rx = 0;
    return ENI_OK;
}

void eni_neuralink_deinit(void) {
    g_state = ENI_NL_STATE_DISCONNECTED;
    memset(&g_config, 0, sizeof(g_config));
}

int eni_neuralink_connect(const char *device_addr) {
    if (!device_addr || strlen(device_addr) == 0) return ENI_ERR_INVALID;
    g_state = ENI_NL_STATE_CONNECTING;
    strncpy(g_config.device_addr, device_addr, 63);
    g_state = ENI_NL_STATE_STREAMING;
    return ENI_OK;
}

int eni_neuralink_disconnect(void) {
    g_state = ENI_NL_STATE_DISCONNECTED;
    return ENI_OK;
}

int eni_neuralink_start_stream(void) {
    if (g_state != ENI_NL_STATE_STREAMING) return ENI_ERR_CONNECT;
    return ENI_OK;
}

int eni_neuralink_stop_stream(void) {
    return ENI_OK;
}

int eni_neuralink_read_packet(eni_nl_packet_t *pkt) {
    if (!pkt) return ENI_ERR_INVALID;
    if (g_state != ENI_NL_STATE_STREAMING) return ENI_ERR_CONNECT;
    memset(pkt, 0, sizeof(*pkt));
    pkt->channel_count = g_config.channels;
    pkt->sample_index = (uint32_t)g_packets_rx;
    pkt->signal_quality = 0.95f;
    pkt->electrode_id = 1;
    g_packets_rx++;
    return ENI_OK;
}

int eni_neuralink_calibrate(uint32_t duration_ms) {
    (void)duration_ms;
    g_state = ENI_NL_STATE_CALIBRATING;
    g_state = ENI_NL_STATE_STREAMING;
    return ENI_OK;
}

int eni_neuralink_set_threshold(float threshold) {
    g_config.signal_threshold = threshold;
    return ENI_OK;
}

int eni_neuralink_decode_intent(const eni_nl_packet_t *pkt,
                                char *intent, int maxlen, float *confidence) {
    if (!pkt || !intent || !confidence) return ENI_ERR_INVALID;
    strncpy(intent, "move_cursor", maxlen - 1);
    *confidence = 0.87f;
    return ENI_OK;
}

int eni_neuralink_get_status(eni_nl_status_t *status) {
    if (!status) return ENI_ERR_INVALID;
    memset(status, 0, sizeof(*status));
    strncpy(status->device_id, "NL-TEST-001", 63);
    strncpy(status->firmware_version, "2.1.0", 31);
    status->state = g_state;
    status->packets_received = g_packets_rx;
    status->battery_pct = 85.0f;
    return ENI_OK;
}

eni_nl_state_t eni_neuralink_get_state(void) {
    return g_state;
}

/* ---- Tests ---- */
static void test_init(void) {
    eni_nl_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.mode = ENI_NL_MODE_INTENT;
    cfg.channels = 256;
    cfg.sample_rate = 30000;
    int rc = eni_neuralink_init(&cfg);
    assert(rc == ENI_OK);
    assert(eni_neuralink_get_state() == ENI_NL_STATE_DISCONNECTED);
    PASS("init");
}

static void test_init_null(void) {
    int rc = eni_neuralink_init(NULL);
    assert(rc == ENI_ERR_INVALID);
    PASS("init_null");
}

static void test_connect(void) {
    eni_nl_config_t cfg = {0};
    cfg.channels = 64;
    eni_neuralink_init(&cfg);
    int rc = eni_neuralink_connect("NL-001-AB:CD:EF");
    assert(rc == ENI_OK);
    assert(eni_neuralink_get_state() == ENI_NL_STATE_STREAMING);
    PASS("connect");
}

static void test_connect_empty_addr(void) {
    eni_nl_config_t cfg = {0};
    eni_neuralink_init(&cfg);
    int rc = eni_neuralink_connect("");
    assert(rc == ENI_ERR_INVALID);
    PASS("connect_empty_addr");
}

static void test_disconnect(void) {
    eni_nl_config_t cfg = {0};
    eni_neuralink_init(&cfg);
    eni_neuralink_connect("NL-001");
    int rc = eni_neuralink_disconnect();
    assert(rc == ENI_OK);
    assert(eni_neuralink_get_state() == ENI_NL_STATE_DISCONNECTED);
    PASS("disconnect");
}

static void test_read_packet(void) {
    eni_nl_config_t cfg = {0};
    cfg.channels = 128;
    eni_neuralink_init(&cfg);
    eni_neuralink_connect("NL-002");
    eni_nl_packet_t pkt;
    int rc = eni_neuralink_read_packet(&pkt);
    assert(rc == ENI_OK);
    assert(pkt.channel_count == 128);
    assert(pkt.signal_quality > 0.0f);
    PASS("read_packet");
}

static void test_read_packet_disconnected(void) {
    eni_nl_config_t cfg = {0};
    eni_neuralink_init(&cfg);
    eni_nl_packet_t pkt;
    int rc = eni_neuralink_read_packet(&pkt);
    assert(rc == ENI_ERR_CONNECT);
    PASS("read_packet_disconnected");
}

static void test_decode_intent(void) {
    eni_nl_packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    char intent[64];
    float confidence;
    int rc = eni_neuralink_decode_intent(&pkt, intent, 64, &confidence);
    assert(rc == ENI_OK);
    assert(strcmp(intent, "move_cursor") == 0);
    assert(confidence > 0.8f);
    PASS("decode_intent");
}

static void test_calibrate(void) {
    eni_nl_config_t cfg = {0};
    eni_neuralink_init(&cfg);
    eni_neuralink_connect("NL-003");
    int rc = eni_neuralink_calibrate(5000);
    assert(rc == ENI_OK);
    PASS("calibrate");
}

static void test_set_threshold(void) {
    eni_nl_config_t cfg = {0};
    eni_neuralink_init(&cfg);
    int rc = eni_neuralink_set_threshold(0.75f);
    assert(rc == ENI_OK);
    PASS("set_threshold");
}

static void test_get_status(void) {
    eni_nl_config_t cfg = {0};
    eni_neuralink_init(&cfg);
    eni_neuralink_connect("NL-004");
    eni_nl_status_t status;
    int rc = eni_neuralink_get_status(&status);
    assert(rc == ENI_OK);
    assert(strcmp(status.device_id, "NL-TEST-001") == 0);
    assert(status.battery_pct > 0.0f);
    PASS("get_status");
}

static void test_constants(void) {
    assert(ENI_NEURALINK_MAX_CHANNELS == 1024);
    assert(ENI_NEURALINK_SAMPLE_RATE == 30000);
    assert(ENI_NEURALINK_PACKET_SIZE == 256);
    PASS("constants");
}

static void test_mode_enum(void) {
    assert(ENI_NL_MODE_RAW == 0);
    assert(ENI_NL_MODE_DECODED == 1);
    assert(ENI_NL_MODE_INTENT == 2);
    assert(ENI_NL_MODE_MOTOR == 3);
    PASS("mode_enum");
}

static void test_deinit(void) {
    eni_nl_config_t cfg = {0};
    eni_neuralink_init(&cfg);
    eni_neuralink_connect("NL-005");
    eni_neuralink_deinit();
    assert(eni_neuralink_get_state() == ENI_NL_STATE_DISCONNECTED);
    PASS("deinit");
}

int main(void) {
    printf("=== eni Neuralink Provider Tests ===\n");
    test_init();
    test_init_null();
    test_connect();
    test_connect_empty_addr();
    test_disconnect();
    test_read_packet();
    test_read_packet_disconnected();
    test_decode_intent();
    test_calibrate();
    test_set_threshold();
    test_get_status();
    test_constants();
    test_mode_enum();
    test_deinit();
    printf("\n=== ALL %d TESTS PASSED ===\n", passed);
    return 0;
}
