// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023
//
// Coverage tests for common/src/log.c
// Exercises: all 6 log levels + default, level filtering,
//            null module, output redirect via eni_log_set_output

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

/* Helper: redirect log output to a temp file, write a message, read it back.
   Returns 1 if `needle` was found in the output, 0 otherwise. */
static int log_contains(eni_log_level_t level, const char *module,
                         const char *msg, const char *needle)
{
    FILE *tmp = tmpfile();
    if (!tmp) return 0;

    eni_log_set_output(tmp);
    eni_log_write(level, module, "%s", msg);

    /* Read back */
    long sz = ftell(tmp);
    if (sz <= 0) { fclose(tmp); return 0; }
    rewind(tmp);
    char buf[2048];
    if (sz >= (long)sizeof(buf)) sz = (long)sizeof(buf) - 1;
    size_t rd = fread(buf, 1, (size_t)sz, tmp);
    buf[rd] = '\0';
    fclose(tmp);

    return strstr(buf, needle) != NULL ? 1 : 0;
}

/* ---------- Level strings ---------- */

static void test_log_level_trace(void)
{
    TEST(log_level_trace);
    eni_log_set_level(ENI_LOG_TRACE);
    if (!log_contains(ENI_LOG_TRACE, "mod", "hello", "TRACE")) { FAIL("TRACE not found"); return; }
    PASS();
}

static void test_log_level_debug(void)
{
    TEST(log_level_debug);
    eni_log_set_level(ENI_LOG_TRACE);
    if (!log_contains(ENI_LOG_DEBUG, "mod", "hello", "DEBUG")) { FAIL("DEBUG not found"); return; }
    PASS();
}

static void test_log_level_info(void)
{
    TEST(log_level_info);
    eni_log_set_level(ENI_LOG_TRACE);
    if (!log_contains(ENI_LOG_INFO, "mod", "hello", "INFO")) { FAIL("INFO not found"); return; }
    PASS();
}

static void test_log_level_warn(void)
{
    TEST(log_level_warn);
    eni_log_set_level(ENI_LOG_TRACE);
    if (!log_contains(ENI_LOG_WARN, "mod", "hello", "WARN")) { FAIL("WARN not found"); return; }
    PASS();
}

static void test_log_level_error(void)
{
    TEST(log_level_error);
    eni_log_set_level(ENI_LOG_TRACE);
    if (!log_contains(ENI_LOG_ERROR, "mod", "hello", "ERROR")) { FAIL("ERROR not found"); return; }
    PASS();
}

static void test_log_level_fatal(void)
{
    TEST(log_level_fatal);
    eni_log_set_level(ENI_LOG_TRACE);
    if (!log_contains(ENI_LOG_FATAL, "mod", "hello", "FATAL")) { FAIL("FATAL not found"); return; }
    PASS();
}

static void test_log_level_default(void)
{
    TEST(log_level_default_branch);
    eni_log_set_level(ENI_LOG_TRACE);
    /* Pass an out-of-range level to hit the default: "???" */
    if (!log_contains((eni_log_level_t)99, "mod", "hello", "???")) { FAIL("??? not found"); return; }
    PASS();
}

/* ---------- Filtering ---------- */

static void test_log_filtering_suppressed(void)
{
    TEST(log_filtering_suppressed);
    /* Set level to WARN; TRACE should be suppressed (no output) */
    eni_log_set_level(ENI_LOG_WARN);

    FILE *tmp = tmpfile();
    if (!tmp) { FAIL("tmpfile failed"); return; }
    eni_log_set_output(tmp);

    eni_log_write(ENI_LOG_TRACE, "mod", "should not appear");

    long sz = ftell(tmp);
    fclose(tmp);

    if (sz != 0) { FAIL("suppressed message appeared"); return; }
    PASS();
}

static void test_log_filtering_passed(void)
{
    TEST(log_filtering_passed);
    /* Set level to WARN; ERROR should pass through */
    eni_log_set_level(ENI_LOG_WARN);
    if (!log_contains(ENI_LOG_ERROR, "mod", "critical", "ERROR")) { FAIL("ERROR not found"); return; }
    PASS();
}

/* ---------- Null module ---------- */

static void test_log_null_module(void)
{
    TEST(log_null_module);
    eni_log_set_level(ENI_LOG_TRACE);
    /* module == NULL should fall back to "eni" in the formatted output */
    if (!log_contains(ENI_LOG_INFO, NULL, "test msg", "eni")) { FAIL("null module fallback not found"); return; }
    PASS();
}

/* ---------- Output redirect ---------- */

static void test_log_output_redirect(void)
{
    TEST(log_output_redirect);
    eni_log_set_level(ENI_LOG_TRACE);

    FILE *tmp = tmpfile();
    if (!tmp) { FAIL("tmpfile failed"); return; }
    eni_log_set_output(tmp);

    eni_log_write(ENI_LOG_INFO, "redirect", "redirected message");

    rewind(tmp);
    char buf[512];
    size_t rd = fread(buf, 1, sizeof(buf) - 1, tmp);
    buf[rd] = '\0';
    fclose(tmp);

    if (!strstr(buf, "redirected message")) { FAIL("message not in redirected output"); return; }
    if (!strstr(buf, "redirect"))           { FAIL("module not in redirected output"); return; }
    PASS();
}

/* ---------- stderr default (s_log_out == NULL) ---------- */

static void test_log_stderr_default(void)
{
    TEST(log_stderr_default);
    /* Reset output to NULL → should write to stderr (just verify no crash) */
    eni_log_set_output(NULL);
    eni_log_set_level(ENI_LOG_TRACE);
    eni_log_write(ENI_LOG_INFO, "test", "stderr default check");
    /* If we get here without crashing, it works */
    PASS();
}

/* ---------- Log macro wrappers ---------- */

static void test_log_macros(void)
{
    TEST(log_macros);
    eni_log_set_level(ENI_LOG_TRACE);

    FILE *tmp = tmpfile();
    if (!tmp) { FAIL("tmpfile failed"); return; }
    eni_log_set_output(tmp);

    ENI_LOG_TRACE("mac", "trace %d", 1);
    ENI_LOG_DEBUG("mac", "debug %d", 2);
    ENI_LOG_INFO("mac", "info %d", 3);
    ENI_LOG_WARN("mac", "warn %d", 4);
    ENI_LOG_ERROR("mac", "error %d", 5);
    ENI_LOG_FATAL("mac", "fatal %d", 6);

    rewind(tmp);
    char buf[4096];
    size_t rd = fread(buf, 1, sizeof(buf) - 1, tmp);
    buf[rd] = '\0';
    fclose(tmp);

    if (!strstr(buf, "trace 1")) { FAIL("TRACE macro"); return; }
    if (!strstr(buf, "debug 2")) { FAIL("DEBUG macro"); return; }
    if (!strstr(buf, "info 3"))  { FAIL("INFO macro"); return; }
    if (!strstr(buf, "warn 4"))  { FAIL("WARN macro"); return; }
    if (!strstr(buf, "error 5")) { FAIL("ERROR macro"); return; }
    if (!strstr(buf, "fatal 6")) { FAIL("FATAL macro"); return; }
    PASS();
}

/* ---------- main ---------- */

int main(void)
{
    printf("=== log.c Coverage Tests ===\n\n");

    test_log_level_trace();
    test_log_level_debug();
    test_log_level_info();
    test_log_level_warn();
    test_log_level_error();
    test_log_level_fatal();
    test_log_level_default();

    test_log_filtering_suppressed();
    test_log_filtering_passed();

    test_log_null_module();
    test_log_output_redirect();
    test_log_stderr_default();
    test_log_macros();

    /* Reset to defaults for other test suites */
    eni_log_set_output(NULL);
    eni_log_set_level(ENI_LOG_INFO);

    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           tests_passed, tests_run, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
