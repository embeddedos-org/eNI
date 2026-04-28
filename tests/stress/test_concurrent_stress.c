// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
//
// Stress tests: Multi-device sync, concurrent calibration, thread contention
//
// Tests:
//   1. Multi-device discovery, connect, sync with concurrent access
//   2. Concurrent calibration from multiple threads
//   3. Stream bus producer/consumer under heavy contention
//   4. Memory pool alloc/free under thread contention
//   5. Session state machine hammered from multiple threads
//   6. Device manager rapid connect/disconnect cycling
//   7. Calibration pipeline with high-frequency sample injection

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "eni/device_manager.h"
#include "eni/session.h"
#include "eni/calibration.h"
#include "eni/profile.h"
#include "eni/mempool.h"
#include "eni/log.h"
#include "eni_fw/stream_bus.h"
#include "eni_platform/platform.h"

/* ── Test Harness ──────────────────────────────────────────────────── */

static int tests_run = 0, tests_passed = 0, tests_failed = 0;

#define TEST(name) do { tests_run++; printf("  STRESS %-46s ", #name); } while(0)
#define PASS()     do { tests_passed++; printf("[PASS]\n"); } while(0)
#define FAIL(msg)  do { tests_failed++; printf("[FAIL] %s\n", msg); return; } while(0)

/* ── Shared State for Thread Tests ─────────────────────────────────── */

#define STRESS_ITERATIONS 1000
#define NUM_THREADS       4

/* ── Test 1: Multi-Device Discovery & Sync ─────────────────────────── */

typedef struct {
    eni_device_manager_t *mgr;
    int                   thread_id;
    int                   ops_completed;
} device_thread_ctx_t;

static void *device_scan_thread(void *arg)
{
    device_thread_ctx_t *ctx = (device_thread_ctx_t *)arg;
    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        eni_device_manager_scan(ctx->mgr);
        uint32_t count = eni_device_manager_get_device_count(ctx->mgr);
        (void)count;
        ctx->ops_completed++;
    }
    return NULL;
}

static void *device_connect_thread(void *arg)
{
    device_thread_ctx_t *ctx = (device_thread_ctx_t *)arg;
    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        /* Alternate connect/disconnect */
        if (i % 2 == 0) {
            eni_device_manager_connect(ctx->mgr, 0);
        } else {
            eni_device_manager_disconnect(ctx->mgr, 0);
        }
        ctx->ops_completed++;
    }
    return NULL;
}

static void test_multidevice_concurrent_access(void)
{
    TEST(multidevice_concurrent_access);

    eni_device_manager_t mgr;
    assert(eni_device_manager_init(&mgr) == ENI_OK);
    assert(eni_device_manager_scan(&mgr) == ENI_OK);

    device_thread_ctx_t ctxs[NUM_THREADS];
    eni_thread_t threads[NUM_THREADS];

    for (int t = 0; t < NUM_THREADS; t++) {
        ctxs[t].mgr = &mgr;
        ctxs[t].thread_id = t;
        ctxs[t].ops_completed = 0;
    }

    /* 2 threads scanning, 2 threads connecting/disconnecting — all concurrently */
    assert(eni_thread_create(&threads[0], device_scan_thread, &ctxs[0]) == ENI_OK);
    assert(eni_thread_create(&threads[1], device_scan_thread, &ctxs[1]) == ENI_OK);
    assert(eni_thread_create(&threads[2], device_connect_thread, &ctxs[2]) == ENI_OK);
    assert(eni_thread_create(&threads[3], device_connect_thread, &ctxs[3]) == ENI_OK);

    for (int t = 0; t < NUM_THREADS; t++) {
        assert(eni_thread_join(&threads[t], NULL) == ENI_OK);
    }

    /* Verify all threads completed all ops without crashing */
    for (int t = 0; t < NUM_THREADS; t++) {
        assert(ctxs[t].ops_completed == STRESS_ITERATIONS);
    }

    /* Manager should still be in a valid state */
    assert(mgr.initialized);
    eni_device_manager_shutdown(&mgr);

    PASS();
}

/* ── Test 2: Multi-Device Sync Workflow ────────────────────────────── */

static void test_multidevice_sync_workflow(void)
{
    TEST(multidevice_sync_workflow);

    eni_device_manager_t mgr;
    assert(eni_device_manager_init(&mgr) == ENI_OK);

    /* Scan — should discover simulator */
    assert(eni_device_manager_scan(&mgr) == ENI_OK);
    assert(eni_device_manager_get_device_count(&mgr) >= 1);

    /* Sync before connect — should fail (no connected devices) */
    assert(eni_device_manager_sync_start(&mgr) == ENI_ERR_NOT_FOUND);

    /* Connect device 0 */
    assert(eni_device_manager_connect(&mgr, 0) == ENI_OK);

    eni_device_info_t info;
    assert(eni_device_manager_get_device_info(&mgr, 0, &info) == ENI_OK);
    assert(info.status == ENI_DEVICE_CONNECTED);
    assert(info.channel_count == 8);
    assert(info.sample_rate == 256);

    /* Sync with 1 connected device — should succeed */
    assert(eni_device_manager_sync_start(&mgr) == ENI_OK);

    /* Double-connect should be idempotent */
    assert(eni_device_manager_connect(&mgr, 0) == ENI_OK);

    /* Disconnect and verify */
    assert(eni_device_manager_disconnect(&mgr, 0) == ENI_OK);
    assert(eni_device_manager_get_device_info(&mgr, 0, &info) == ENI_OK);
    assert(info.status == ENI_DEVICE_DISCONNECTED);

    /* Double-disconnect should be idempotent */
    assert(eni_device_manager_disconnect(&mgr, 0) == ENI_OK);

    /* Out-of-range index */
    assert(eni_device_manager_connect(&mgr, 99) == ENI_ERR_NOT_FOUND);
    assert(eni_device_manager_disconnect(&mgr, 99) == ENI_ERR_NOT_FOUND);
    assert(eni_device_manager_get_device_info(&mgr, 99, &info) == ENI_ERR_NOT_FOUND);

    /* Null checks */
    assert(eni_device_manager_init(NULL) == ENI_ERR_INVALID);
    assert(eni_device_manager_scan(NULL) == ENI_ERR_INVALID);
    assert(eni_device_manager_get_device_count(NULL) == 0);

    eni_device_manager_shutdown(&mgr);
    PASS();
}

/* ── Test 3: Concurrent Calibration ────────────────────────────────── */

typedef struct {
    int  thread_id;
    bool success;
} cal_thread_ctx_t;

static void *calibration_thread(void *arg)
{
    cal_thread_ctx_t *ctx = (cal_thread_ctx_t *)arg;
    ctx->success = false;

    eni_calibration_t cal;
    if (eni_calibration_init(&cal, 4, 256.0f) != ENI_OK) return NULL;

    /* Stage 1: Impedance */
    if (eni_calibration_start_impedance(&cal) != ENI_OK) goto cleanup;
    float imp[4] = {10, 12, 15, 11};
    if (eni_calibration_submit_impedance(&cal, imp, 4) != ENI_OK) goto cleanup;

    /* Stage 2: Baseline — push many samples quickly */
    if (eni_calibration_start_baseline(&cal) != ENI_OK) goto cleanup;
    for (int s = 0; s < 500; s++) {
        float samples[4];
        for (int ch = 0; ch < 4; ch++) {
            samples[ch] = sinf((float)s * 0.05f + (float)(ctx->thread_id * 7 + ch)) * 50.0f;
        }
        if (eni_calibration_push_baseline_sample(&cal, samples, 4) != ENI_OK) goto cleanup;
    }
    if (eni_calibration_finalize_baseline(&cal) != ENI_OK) goto cleanup;

    /* Stage 3: Thresholds */
    if (eni_calibration_compute_thresholds(&cal, 0.95f) != ENI_OK) goto cleanup;

    /* Stage 4: Validation — 90% accuracy */
    if (eni_calibration_start_validation(&cal) != ENI_OK) goto cleanup;
    for (int t = 0; t < 10; t++) {
        eni_calibration_submit_trial(&cal, t < 9);
    }

    /* Finalize */
    eni_profile_t profile;
    eni_profile_init(&profile, "concurrent-user");
    if (eni_calibration_finalize(&cal, &profile) != ENI_OK) goto cleanup;

    if (eni_calibration_get_stage(&cal) == ENI_CAL_COMPLETE &&
        eni_calibration_get_accuracy(&cal) >= 0.8f) {
        ctx->success = true;
    }

cleanup:
    eni_calibration_destroy(&cal);
    return NULL;
}

static void test_concurrent_calibration(void)
{
    TEST(concurrent_calibration);

    cal_thread_ctx_t ctxs[NUM_THREADS];
    eni_thread_t threads[NUM_THREADS];

    for (int t = 0; t < NUM_THREADS; t++) {
        ctxs[t].thread_id = t;
        ctxs[t].success = false;
        assert(eni_thread_create(&threads[t], calibration_thread, &ctxs[t]) == ENI_OK);
    }

    for (int t = 0; t < NUM_THREADS; t++) {
        assert(eni_thread_join(&threads[t], NULL) == ENI_OK);
    }

    /* All calibrations should have succeeded independently */
    for (int t = 0; t < NUM_THREADS; t++) {
        assert(ctxs[t].success);
    }

    PASS();
}

/* ── Test 4: Stream Bus Producer/Consumer Stress ───────────────────── */

typedef struct {
    eni_fw_stream_bus_t *bus;
    int                  count;
    eni_atomic_int_t    *produced;
    eni_atomic_int_t    *consumed;
} bus_thread_ctx_t;

static void *bus_producer_thread(void *arg)
{
    bus_thread_ctx_t *ctx = (bus_thread_ctx_t *)arg;
    for (int i = 0; i < ctx->count; i++) {
        eni_event_t ev;
        eni_event_init(&ev, ENI_EVENT_INTENT, "producer");
        eni_event_set_intent(&ev, "action", (float)i / (float)ctx->count);
        eni_status_t rc = eni_fw_stream_bus_push(ctx->bus, &ev);
        if (rc == ENI_OK) {
            eni_atomic_fetch_add(ctx->produced, 1);
        }
        /* Brief yield to allow consumers to keep up */
        if (i % 50 == 0) eni_platform_sleep_ms(1);
    }
    return NULL;
}

static void *bus_consumer_thread(void *arg)
{
    bus_thread_ctx_t *ctx = (bus_thread_ctx_t *)arg;
    int empty_streak = 0;
    while (empty_streak < 100) {
        eni_event_t ev;
        eni_status_t rc = eni_fw_stream_bus_pop(ctx->bus, &ev);
        if (rc == ENI_OK) {
            eni_atomic_fetch_add(ctx->consumed, 1);
            empty_streak = 0;
        } else {
            empty_streak++;
            eni_platform_sleep_ms(1);
        }
    }
    return NULL;
}

static void test_stream_bus_stress(void)
{
    TEST(stream_bus_producer_consumer_stress);

    eni_fw_stream_bus_t bus;
    assert(eni_fw_stream_bus_init(&bus) == ENI_OK);

    eni_atomic_int_t produced, consumed;
    eni_atomic_init(&produced, 0);
    eni_atomic_init(&consumed, 0);

    bus_thread_ctx_t prod_ctx = { .bus = &bus, .count = 500, .produced = &produced, .consumed = &consumed };
    bus_thread_ctx_t cons_ctx = { .bus = &bus, .count = 0,   .produced = &produced, .consumed = &consumed };

    eni_thread_t producers[2], consumers[2];

    /* 2 producers, 2 consumers */
    assert(eni_thread_create(&consumers[0], bus_consumer_thread, &cons_ctx) == ENI_OK);
    assert(eni_thread_create(&consumers[1], bus_consumer_thread, &cons_ctx) == ENI_OK);
    assert(eni_thread_create(&producers[0], bus_producer_thread, &prod_ctx) == ENI_OK);
    assert(eni_thread_create(&producers[1], bus_producer_thread, &prod_ctx) == ENI_OK);

    for (int i = 0; i < 2; i++) assert(eni_thread_join(&producers[i], NULL) == ENI_OK);
    for (int i = 0; i < 2; i++) assert(eni_thread_join(&consumers[i], NULL) == ENI_OK);

    int total_produced = eni_atomic_load(&produced);
    int total_consumed = eni_atomic_load(&consumed);

    /* Some events may be dropped if bus was full, but consumed should never exceed produced */
    assert(total_consumed <= total_produced);
    /* And we should have consumed a significant portion */
    assert(total_consumed > 0);

    /* Bus should be consistent — pending count matches actual */
    int pending = eni_fw_stream_bus_pending(&bus);
    assert(pending >= 0);
    assert(pending == total_produced - total_consumed - (int)bus.total_dropped);

    eni_fw_stream_bus_destroy(&bus);
    PASS();
}

/* ── Test 5: Memory Pool Concurrent Alloc/Free ─────────────────────── */

typedef struct {
    eni_mempool_t    *pool;
    eni_atomic_int_t *alloc_count;
    eni_atomic_int_t *free_count;
    int               iterations;
} pool_thread_ctx_t;

static void *pool_stress_thread(void *arg)
{
    pool_thread_ctx_t *ctx = (pool_thread_ctx_t *)arg;
    void *ptrs[16];
    int ptr_count = 0;

    for (int i = 0; i < ctx->iterations; i++) {
        if (ptr_count < 16 && (i % 3 != 0)) {
            /* Allocate */
            void *p = eni_mempool_alloc(ctx->pool);
            if (p) {
                ptrs[ptr_count++] = p;
                eni_atomic_fetch_add(ctx->alloc_count, 1);
            }
        } else if (ptr_count > 0) {
            /* Free the most recently allocated */
            ptr_count--;
            eni_mempool_free(ctx->pool, ptrs[ptr_count]);
            eni_atomic_fetch_add(ctx->free_count, 1);
        }
    }

    /* Free remaining */
    while (ptr_count > 0) {
        ptr_count--;
        eni_mempool_free(ctx->pool, ptrs[ptr_count]);
        eni_atomic_fetch_add(ctx->free_count, 1);
    }

    return NULL;
}

static void test_mempool_concurrent_stress(void)
{
    TEST(mempool_concurrent_alloc_free);

    /* 4KB pool with 64-byte blocks = 64 blocks */
    uint8_t buffer[4096];
    eni_mempool_t pool;
    assert(eni_mempool_init(&pool, buffer, sizeof(buffer), 64) == ENI_OK);

    eni_atomic_int_t alloc_count, free_count;
    eni_atomic_init(&alloc_count, 0);
    eni_atomic_init(&free_count, 0);

    pool_thread_ctx_t ctxs[NUM_THREADS];
    eni_thread_t threads[NUM_THREADS];

    for (int t = 0; t < NUM_THREADS; t++) {
        ctxs[t].pool = &pool;
        ctxs[t].alloc_count = &alloc_count;
        ctxs[t].free_count = &free_count;
        ctxs[t].iterations = STRESS_ITERATIONS;
        assert(eni_thread_create(&threads[t], pool_stress_thread, &ctxs[t]) == ENI_OK);
    }

    for (int t = 0; t < NUM_THREADS; t++) {
        assert(eni_thread_join(&threads[t], NULL) == ENI_OK);
    }

    int total_alloc = eni_atomic_load(&alloc_count);
    int total_free = eni_atomic_load(&free_count);

    /* All allocations should have been freed */
    assert(total_alloc == total_free);

    /* Pool should be back to full capacity */
    eni_mempool_stats_t stats;
    eni_mempool_stats(&pool, &stats);
    assert(stats.allocated == 0);
    assert(stats.available == stats.num_blocks);

    eni_mempool_destroy(&pool);
    PASS();
}

/* ── Test 6: Device Manager Rapid Connect/Disconnect Cycling ───────── */

static void test_device_rapid_cycle(void)
{
    TEST(device_rapid_connect_disconnect_cycle);

    eni_device_manager_t mgr;
    assert(eni_device_manager_init(&mgr) == ENI_OK);
    assert(eni_device_manager_scan(&mgr) == ENI_OK);

    /* Rapidly connect and disconnect 1000 times */
    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        assert(eni_device_manager_connect(&mgr, 0) == ENI_OK);

        eni_device_info_t info;
        assert(eni_device_manager_get_device_info(&mgr, 0, &info) == ENI_OK);
        assert(info.status == ENI_DEVICE_CONNECTED);

        assert(eni_device_manager_disconnect(&mgr, 0) == ENI_OK);
        assert(eni_device_manager_get_device_info(&mgr, 0, &info) == ENI_OK);
        assert(info.status == ENI_DEVICE_DISCONNECTED);
    }

    /* Rescan mid-cycle to stress the device list reset */
    assert(eni_device_manager_scan(&mgr) == ENI_OK);
    assert(eni_device_manager_get_device_count(&mgr) >= 1);

    eni_device_manager_shutdown(&mgr);
    PASS();
}

/* ── Test 7: High-Frequency Calibration Sample Injection ───────────── */

static void test_calibration_high_frequency_samples(void)
{
    TEST(calibration_high_freq_sample_injection);

    eni_calibration_t cal;
    assert(eni_calibration_init(&cal, 16, 1024.0f) == ENI_OK); /* 16ch, 1024 Hz */

    assert(eni_calibration_start_impedance(&cal) == ENI_OK);
    float imp[16];
    for (int i = 0; i < 16; i++) imp[i] = 5.0f + (float)i;
    assert(eni_calibration_submit_impedance(&cal, imp, 16) == ENI_OK);

    assert(eni_calibration_start_baseline(&cal) == ENI_OK);

    /* Inject 30 seconds × 1024 Hz = 30720 samples at max channel count */
    int total_samples = 1024 * 30;
    for (int s = 0; s < total_samples; s++) {
        float samples[16];
        for (int ch = 0; ch < 16; ch++) {
            samples[ch] = sinf((float)s * 0.01f * (float)(ch + 1)) * 100.0f;
        }
        assert(eni_calibration_push_baseline_sample(&cal, samples, 16) == ENI_OK);
    }

    assert(eni_calibration_finalize_baseline(&cal) == ENI_OK);
    assert(cal.baseline_samples == total_samples);

    /* Verify statistics are reasonable for all 16 channels */
    for (int ch = 0; ch < 16; ch++) {
        assert(cal.baseline_std[ch] > 0.0f);
        assert(isfinite(cal.baseline_mean[ch]));
        assert(isfinite(cal.baseline_std[ch]));
    }

    assert(eni_calibration_compute_thresholds(&cal, 0.99f) == ENI_OK);

    /* Thresholds at 99th percentile should be well above mean */
    for (int ch = 0; ch < 16; ch++) {
        assert(cal.thresholds[ch] > cal.baseline_mean[ch]);
    }

    eni_calibration_destroy(&cal);
    PASS();
}

/* ── Main ──────────────────────────────────────────────────────────── */

int main(void)
{
    /* Suppress log noise */
    eni_log_set_level(ENI_LOG_ERROR);
    eni_platform_init();

    printf("=== Stress Tests: Multi-Device Sync & Concurrent Calibration ===\n\n");

    test_multidevice_sync_workflow();
    test_multidevice_concurrent_access();
    test_concurrent_calibration();
    test_stream_bus_stress();
    test_mempool_concurrent_stress();
    test_device_rapid_cycle();
    test_calibration_high_frequency_samples();

    printf("\n── Summary ─────────────────────────────────────────────\n");
    printf("  Total:  %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_failed);
    printf("─────────────────────────────────────────────────────────\n");

    if (tests_failed > 0) {
        printf("\n*** %d STRESS TEST(S) FAILED ***\n", tests_failed);
        return 1;
    }

    printf("\n=== ALL %d STRESS TESTS PASSED ===\n", tests_passed);
    return 0;
}
