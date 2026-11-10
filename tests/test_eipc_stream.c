// SPDX-License-Identifier: MIT
// Tests for the EIPC streaming integration (integrations/eipc_streaming)

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "eni/eipc_stream.h"

static int tests_run = 0, tests_passed = 0, tests_failed = 0;
#define TEST(name) do { tests_run++; printf("  TEST %-40s ", #name); } while(0)
#define PASS() do { tests_passed++; printf("[PASS]\n"); } while(0)

static eni_stream_session_t session; /* large ring buffer: keep it off the stack */

static void test_local_buffer_raw(void)
{
    TEST(stream_local_buffer_raw);
    eni_stream_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.compression = ENI_STREAM_COMPRESS_NONE;

    assert(eni_stream_init(&session, &cfg) == ENI_STREAM_OK);
    assert(eni_stream_connect(&session) == ENI_STREAM_OK); /* no endpoint: local buffer mode */

    const float samples[8] = {0.f, 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f};
    assert(eni_stream_send(&session, samples, 8, 1000u) == ENI_STREAM_OK);
    assert(eni_stream_send(&session, samples, 8, 2000u) == ENI_STREAM_OK);

    uint64_t bytes = 0;
    uint32_t packets = 0, drops = 0;
    eni_stream_get_stats(&session, &bytes, &packets, &drops);
    assert(bytes == 2u * 8u * sizeof(float));
    assert(packets == 2u);
    assert(drops == 0u);

    eni_stream_deinit(&session);
    PASS();
}

static void test_local_buffer_delta(void)
{
    TEST(stream_local_buffer_delta);
    eni_stream_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.compression = ENI_STREAM_COMPRESS_DELTA;

    assert(eni_stream_init(&session, &cfg) == ENI_STREAM_OK);
    assert(eni_stream_connect(&session) == ENI_STREAM_OK);

    const float samples[4] = {1.f, 3.f, 6.f, 10.f};
    assert(eni_stream_send(&session, samples, 4, 1u) == ENI_STREAM_OK);

    /* First sample as-is, then the deltas 2, 3, 4 */
    const eni_stream_packet_t *pkt = &session.ring[0];
    float decoded[4];
    assert(pkt->data_len == sizeof(decoded));
    memcpy(decoded, pkt->data, sizeof(decoded));
    assert(decoded[0] == 1.f && decoded[1] == 2.f && decoded[2] == 3.f && decoded[3] == 4.f);

    eni_stream_deinit(&session);
    PASS();
}

static void test_connect_refused(void)
{
    TEST(stream_connect_refused);
    eni_stream_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.endpoint = "127.0.0.1";
    cfg.port = 1; /* nothing listens on loopback port 1 */

    assert(eni_stream_init(&session, &cfg) == ENI_STREAM_OK);
    assert(eni_stream_connect(&session) == ENI_STREAM_ERR_CONNECT);
    assert(session.connected == 0);

    eni_stream_deinit(&session);
    PASS();
}

static void test_null_args(void)
{
    TEST(stream_null_args);
    eni_stream_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    assert(eni_stream_init(NULL, &cfg) == ENI_STREAM_ERR_INIT);
    assert(eni_stream_init(&session, NULL) == ENI_STREAM_ERR_INIT);
    assert(eni_stream_connect(NULL) == ENI_STREAM_ERR_INIT);
    PASS();
}

int main(void)
{
    printf("=== EIPC Streaming Tests ===\n");
    test_local_buffer_raw();
    test_local_buffer_delta();
    test_connect_refused();
    test_null_args();
    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return tests_failed > 0 ? 1 : 0;
}
