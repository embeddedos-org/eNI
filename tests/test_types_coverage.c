// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023
//
// Coverage tests for common/src/types.c
// Exercises: all 15 eni_status_str switch cases + default + eni_timestamp_now

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "eni/common.h"

static int tests_run    = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    do { \
        tests_run++; \
        printf("  TEST %-40s ", #name); \
    } while (0)

#define PASS() \
    do { \
        tests_passed++; \
        printf("[PASS]\n"); \
    } while (0)

#define FAIL(msg) \
    do { \
        tests_failed++; \
        printf("[FAIL] %s\n", msg); \
    } while (0)

/* ---------- eni_status_str: every case ---------- */

static void test_status_str_ok(void)
{
    TEST(status_str_ok);
    if (strcmp(eni_status_str(ENI_OK), "OK") != 0) { FAIL("ENI_OK"); return; }
    PASS();
}

static void test_status_str_nomem(void)
{
    TEST(status_str_nomem);
    if (strcmp(eni_status_str(ENI_ERR_NOMEM), "ERR_NOMEM") != 0) { FAIL("NOMEM"); return; }
    PASS();
}

static void test_status_str_invalid(void)
{
    TEST(status_str_invalid);
    if (strcmp(eni_status_str(ENI_ERR_INVALID), "ERR_INVALID") != 0) { FAIL("INVALID"); return; }
    PASS();
}

static void test_status_str_not_found(void)
{
    TEST(status_str_not_found);
    if (strcmp(eni_status_str(ENI_ERR_NOT_FOUND), "ERR_NOT_FOUND") != 0) { FAIL("NOT_FOUND"); return; }
    PASS();
}

static void test_status_str_io(void)
{
    TEST(status_str_io);
    if (strcmp(eni_status_str(ENI_ERR_IO), "ERR_IO") != 0) { FAIL("IO"); return; }
    PASS();
}

static void test_status_str_timeout(void)
{
    TEST(status_str_timeout);
    if (strcmp(eni_status_str(ENI_ERR_TIMEOUT), "ERR_TIMEOUT") != 0) { FAIL("TIMEOUT"); return; }
    PASS();
}

static void test_status_str_permission(void)
{
    TEST(status_str_permission);
    if (strcmp(eni_status_str(ENI_ERR_PERMISSION), "ERR_PERMISSION") != 0) { FAIL("PERMISSION"); return; }
    PASS();
}

static void test_status_str_runtime(void)
{
    TEST(status_str_runtime);
    if (strcmp(eni_status_str(ENI_ERR_RUNTIME), "ERR_RUNTIME") != 0) { FAIL("RUNTIME"); return; }
    PASS();
}

static void test_status_str_connect(void)
{
    TEST(status_str_connect);
    if (strcmp(eni_status_str(ENI_ERR_CONNECT), "ERR_CONNECT") != 0) { FAIL("CONNECT"); return; }
    PASS();
}

static void test_status_str_protocol(void)
{
    TEST(status_str_protocol);
    if (strcmp(eni_status_str(ENI_ERR_PROTOCOL), "ERR_PROTOCOL") != 0) { FAIL("PROTOCOL"); return; }
    PASS();
}

static void test_status_str_config(void)
{
    TEST(status_str_config);
    if (strcmp(eni_status_str(ENI_ERR_CONFIG), "ERR_CONFIG") != 0) { FAIL("CONFIG"); return; }
    PASS();
}

static void test_status_str_unsupported(void)
{
    TEST(status_str_unsupported);
    if (strcmp(eni_status_str(ENI_ERR_UNSUPPORTED), "ERR_UNSUPPORTED") != 0) { FAIL("UNSUPPORTED"); return; }
    PASS();
}

static void test_status_str_policy_denied(void)
{
    TEST(status_str_policy_denied);
    if (strcmp(eni_status_str(ENI_ERR_POLICY_DENIED), "ERR_POLICY_DENIED") != 0) { FAIL("POLICY_DENIED"); return; }
    PASS();
}

static void test_status_str_provider(void)
{
    TEST(status_str_provider);
    if (strcmp(eni_status_str(ENI_ERR_PROVIDER), "ERR_PROVIDER") != 0) { FAIL("PROVIDER"); return; }
    PASS();
}

static void test_status_str_overflow(void)
{
    TEST(status_str_overflow);
    if (strcmp(eni_status_str(ENI_ERR_OVERFLOW), "ERR_OVERFLOW") != 0) { FAIL("OVERFLOW"); return; }
    PASS();
}

static void test_status_str_unknown(void)
{
    TEST(status_str_unknown_default);
    /* Cast an out-of-range value to exercise the default branch */
    if (strcmp(eni_status_str((eni_status_t)9999), "UNKNOWN") != 0) { FAIL("default"); return; }
    PASS();
}

/* ---------- eni_timestamp_now ---------- */

static void test_timestamp_now(void)
{
    TEST(timestamp_now);
    eni_timestamp_t ts = eni_timestamp_now();
    /* On any real system the second count should be > 0 (since the Unix epoch
       or monotonic origin). nsec must be < 1 000 000 000. */
    if (ts.sec == 0 && ts.nsec == 0) {
        /* Bare-metal stub returns zero — still valid */
        PASS();
        return;
    }
    if (ts.nsec >= 1000000000U) { FAIL("nsec out of range"); return; }
    PASS();
}

static void test_timestamp_monotonic(void)
{
    TEST(timestamp_monotonic);
    eni_timestamp_t t1 = eni_timestamp_now();
    /* Spin briefly to let some time elapse */
    volatile int spin = 0;
    for (int i = 0; i < 100000; i++) spin++;
    (void)spin;
    eni_timestamp_t t2 = eni_timestamp_now();
    /* t2 should be >= t1 */
    if (t2.sec < t1.sec) { FAIL("time went backwards"); return; }
    if (t2.sec == t1.sec && t2.nsec < t1.nsec) { FAIL("nsec went backwards"); return; }
    PASS();
}

/* ---------- main ---------- */

int main(void)
{
    printf("=== types.c Coverage Tests ===\n\n");

    /* eni_status_str: all 15 cases + default */
    test_status_str_ok();
    test_status_str_nomem();
    test_status_str_invalid();
    test_status_str_not_found();
    test_status_str_io();
    test_status_str_timeout();
    test_status_str_permission();
    test_status_str_runtime();
    test_status_str_connect();
    test_status_str_protocol();
    test_status_str_config();
    test_status_str_unsupported();
    test_status_str_policy_denied();
    test_status_str_provider();
    test_status_str_overflow();
    test_status_str_unknown();

    /* eni_timestamp_now */
    test_timestamp_now();
    test_timestamp_monotonic();

    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           tests_passed, tests_run, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
