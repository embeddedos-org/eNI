// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// A failed open must not leave a closed FILE* behind in the handle.
//
// eni_native_close() and eni_xdf_close() free any non-NULL fp, so a caller
// that closes defensively after a failed open -- the ordinary C idiom --
// would fclose() the same FILE* twice. AddressSanitizer reports that as a
// double-free; without a sanitizer it is silent heap corruption.
//
// The trigger is a wrong 4-byte magic, which is the first check each opener
// performs, so any file of the wrong format reaches it.
//
// These assert the handle directly rather than relying on a sanitizer, so the
// test fails deterministically in an ordinary Debug build.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "eni/eni_format.h"
#include "eni/xdf.h"

static int tests_run = 0, tests_passed = 0, tests_failed = 0;
#define TEST(name) do { tests_run++; printf("  TEST %-40s ", #name); } while(0)
#define PASS() do { tests_passed++; printf("[PASS]\n"); } while(0)
#define FAIL(msg) do { tests_failed++; printf("[FAIL] %s\n", msg); } while(0)

/* Write a file whose first four bytes are deliberately not the magic the
 * opener expects. Everything after the magic is irrelevant: the opener
 * rejects the file before reading any of it. */
static bool write_bad_magic(const char *path)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) return false;
    bool ok = fwrite("ZZZZ................", 1, 20, fp) == 20;
    if (fclose(fp) != 0) return false;
    return ok;
}

static void test_native_failed_open_clears_handle(void)
{
    TEST(native_failed_open_clears_handle);

    const char *path = "/tmp/eni_test_bad_magic.eni";
    if (!write_bad_magic(path)) { FAIL("could not write the fixture file"); return; }

    eni_native_file_t nf;
    eni_status_t st = eni_native_open(&nf, path);
    if (st != ENI_ERR_CONFIG) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "native: bad magic accepted: open returned %d", (int)st);
        remove(path);
        FAIL(msg);
        return;
    }
    if (nf.fp != NULL) {
        remove(path);
        FAIL("native: failed open left a closed FILE* in the handle; "
             "eni_native_close() would fclose() it a second time");
        return;
    }
    eni_native_close(&nf); /* must be a no-op, not a second fclose */
    remove(path);
    PASS();
}

static void test_xdf_failed_open_clears_handle(void)
{
    TEST(xdf_failed_open_clears_handle);

    const char *path = "/tmp/eni_test_bad_magic.xdf";
    if (!write_bad_magic(path)) { FAIL("could not write the fixture file"); return; }

    eni_xdf_file_t xdf;
    eni_status_t st = eni_xdf_open(&xdf, path);
    if (st != ENI_ERR_CONFIG) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "xdf: bad magic accepted: open returned %d", (int)st);
        remove(path);
        FAIL(msg);
        return;
    }
    if (xdf.fp != NULL) {
        remove(path);
        FAIL("xdf: failed open left a closed FILE* in the handle; "
             "eni_xdf_close() would fclose() it a second time");
        return;
    }
    eni_xdf_close(&xdf); /* must be a no-op, not a second fclose */
    remove(path);
    PASS();
}

/* A missing file is the other way open() fails. It returns before fopen()
 * succeeds, so fp must be NULL here too -- and close() must still be safe. */
static void test_missing_file_clears_handle(void)
{
    TEST(missing_file_clears_handle);

    const char *missing = "/tmp/eni_does_not_exist_8c1f2a.dat";
    remove(missing);

    eni_native_file_t nf;
    if (eni_native_open(&nf, missing) != ENI_ERR_IO) { FAIL("native: expected ENI_ERR_IO"); return; }
    if (nf.fp != NULL) { FAIL("native: missing file left a non-NULL fp"); return; }
    eni_native_close(&nf);

    eni_xdf_file_t xdf;
    if (eni_xdf_open(&xdf, missing) != ENI_ERR_IO) { FAIL("xdf: expected ENI_ERR_IO"); return; }
    if (xdf.fp != NULL) { FAIL("xdf: missing file left a non-NULL fp"); return; }
    eni_xdf_close(&xdf);

    PASS();
}

int main(void)
{
    printf("=== Format Open Handle Tests ===\n");
    test_native_failed_open_clears_handle();
    test_xdf_failed_open_clears_handle();
    test_missing_file_clears_handle();
    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return tests_failed > 0 ? 1 : 0;
}
