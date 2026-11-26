// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Tests for data format modules

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <time.h>
#include "eni/data_format.h"
#include "eni/edf.h"
#include "eni/xdf.h"
#include "eni/bdf.h"
#include "eni/annotation.h"
#include "eni/recorder.h"
#include "eni/eni_format.h"
#ifndef _WIN32
#include <signal.h>
#include <unistd.h>
#endif

static int tests_run = 0, tests_passed = 0, tests_failed = 0;
#define TEST(name) do { tests_run++; printf("  TEST %-40s ", #name); } while(0)
#define PASS() do { tests_passed++; printf("[PASS]\n"); } while(0)
#define FAIL(msg) do { tests_failed++; printf("[FAIL] %s\n", msg); } while(0)

static void test_format_detect(void)
{
    TEST(data_format_detect);
    uint8_t edf_magic[256] = {0};
    edf_magic[0] = '0';
    assert(eni_data_format_detect(edf_magic, 256) == ENI_FORMAT_EDF);

    uint8_t bdf_magic[8] = {0xFF, 'B','I','O','S','E','M','I'};
    assert(eni_data_format_detect(bdf_magic, 8) == ENI_FORMAT_BDF);

    uint8_t xdf_magic[4] = {'X','D','F',':'};
    assert(eni_data_format_detect(xdf_magic, 4) == ENI_FORMAT_XDF);

    uint8_t eni_magic[4] = {'E','N','I','1'};
    assert(eni_data_format_detect(eni_magic, 4) == ENI_FORMAT_ENI);

    assert(eni_data_format_detect((const uint8_t*)"????", 4) == ENI_FORMAT_UNKNOWN);
    PASS();
}

static void test_format_names(void)
{
    TEST(data_format_names);
    assert(strcmp(eni_data_format_name(ENI_FORMAT_EDF), "EDF+") == 0);
    assert(strcmp(eni_data_format_name(ENI_FORMAT_BDF), "BDF+") == 0);
    assert(strcmp(eni_data_format_name(ENI_FORMAT_XDF), "XDF") == 0);
    assert(strcmp(eni_data_format_name(ENI_FORMAT_ENI), "ENI") == 0);
    PASS();
}

/* Reading a native file in calls smaller than a data chunk must return every
 * sample once, in order: the part of a chunk that did not fit in the buffer is
 * read by the next call, not skipped. */
static void test_native_read_samples_across_calls(void)
{
    TEST(native_read_samples_across_calls);
    const char *path = "/tmp/eni_test_native_read.eni";
    eni_data_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.num_channels = 1;
    strcpy(hdr.channels[0].label, "C3");
    hdr.channels[0].sample_rate = 256.0;

    double chunk0[10], chunk1[3];
    for (int i = 0; i < 10; i++) chunk0[i] = (double)(i + 1);
    for (int i = 0; i < 3; i++) chunk1[i] = (double)(i + 11);

    eni_native_file_t wr;
    assert(eni_native_create(&wr, path, &hdr, 0) == ENI_OK);
    assert(eni_native_write_samples(&wr, chunk0, 10) == ENI_OK);
    assert(eni_native_write_event(&wr, 0.5, "marker") == ENI_OK);
    assert(eni_native_write_samples(&wr, chunk1, 3) == ENI_OK);
    assert(eni_native_finalize(&wr) == ENI_OK);

    eni_native_file_t rd;
    assert(eni_native_open(&rd, path) == ENI_OK);
    double got[16] = {0};
    int total = 0, calls = 0, n = 0;
    do {
        double buf[4];
        assert(eni_native_read_samples(&rd, buf, 4, &n) == ENI_OK);
        for (int i = 0; i < n && total < 16; i++) got[total++] = buf[i];
        calls++;
    } while (n > 0 && calls < 20);
    eni_native_close(&rd);
    remove(path);

    bool ok = (total == 13);
    for (int i = 0; ok && i < 13; i++)
        if (fabs(got[i] - (double)(i + 1)) > 1e-6) ok = false;
    if (ok) {
        PASS();
    } else {
        char msg[160];
        snprintf(msg, sizeof(msg), "read %d of 13 samples in 4-sample calls: %g %g %g %g %g ...",
                 total, got[0], got[1], got[2], got[3], got[4]);
        FAIL(msg);
    }
}

static void test_edf_digital_physical_conversion(void)
{
    TEST(edf_digital_physical_conversion);
    eni_channel_info_t ch = {
        .physical_min = -3200.0, .physical_max = 3200.0,
        .digital_min = -32768, .digital_max = 32767,
    };
    double phys = eni_edf_digital_to_physical(&ch, 0);
    assert(fabs(phys) < 0.1); /* Should be near 0 */

    int16_t dig = eni_edf_physical_to_digital(&ch, 0.0);
    assert(abs(dig) < 2); /* Should be near 0 */

    /* Round-trip */
    double orig = 1000.0;
    int16_t d = eni_edf_physical_to_digital(&ch, orig);
    double back = eni_edf_digital_to_physical(&ch, d);
    assert(fabs(back - orig) < 1.0); /* Within 1 unit quantization */
    PASS();
}

static void test_edf_write_read_roundtrip(void)
{
    TEST(edf_write_read_roundtrip);
    const char *path = "/tmp/eni_test.edf";
    eni_data_header_t hdr;
    eni_data_header_init(&hdr);
    hdr.num_channels = 2;
    hdr.record_duration = 1.0;
    strncpy(hdr.patient, "TestPatient", sizeof(hdr.patient) - 1);

    for (int i = 0; i < 2; i++) {
        snprintf(hdr.channels[i].label, ENI_DATA_MAX_LABEL, "Ch%d", i);
        hdr.channels[i].physical_min = -3200.0;
        hdr.channels[i].physical_max = 3200.0;
        hdr.channels[i].digital_min = -32768;
        hdr.channels[i].digital_max = 32767;
        hdr.channels[i].samples_per_record = 256;
        hdr.channels[i].sample_rate = 256.0;
    }

    /* Write */
    eni_edf_file_t edf;
    eni_status_t rc = eni_edf_create(&edf, path, &hdr);
    assert(rc == ENI_OK);

    double samples[512]; /* 2 channels * 256 samples */
    for (int i = 0; i < 512; i++) samples[i] = sin(2.0 * 3.14159 * i / 256.0) * 100.0;

    rc = eni_edf_write_record(&edf, samples, 512);
    assert(rc == ENI_OK);

    rc = eni_edf_finalize(&edf);
    assert(rc == ENI_OK);

    /* Read back */
    eni_edf_file_t edf2;
    rc = eni_edf_open(&edf2, path);
    assert(rc == ENI_OK);
    assert(edf2.header.num_channels == 2);
    assert(edf2.header.num_records == 1);

    double readback[512];
    rc = eni_edf_read_record(&edf2, 0, readback, 512);
    assert(rc == ENI_OK);

    /* Verify first few samples are close (within quantization error) */
    for (int i = 0; i < 10; i++) {
        assert(fabs(readback[i] - samples[i]) < 1.0);
    }

    eni_edf_close(&edf2);
    PASS();
}

/* Header fields that use every byte of their fixed width must read back
 * whole: dd.mm.yy and hh.mm.ss are always 8 characters, a 16-character label
 * fills its field, and "%.6g" writes -3276.75 as 8 characters. */
static void fill_full_width_header(eni_data_header_t *hdr)
{
    eni_data_header_init(hdr);
    hdr->num_channels = 1;
    hdr->record_duration = 1.0;
    strncpy(hdr->start_date, "13.11.26", sizeof(hdr->start_date) - 1);
    strncpy(hdr->start_time, "22.37.05", sizeof(hdr->start_time) - 1);
    strncpy(hdr->channels[0].label, "EEG Fpz-Cz left1", ENI_DATA_MAX_LABEL - 1);
    hdr->channels[0].physical_min = -3276.75;
    hdr->channels[0].physical_max = 3276.75;
    hdr->channels[0].digital_min = -32768;
    hdr->channels[0].digital_max = 32767;
    hdr->channels[0].samples_per_record = 4;
}

static bool full_width_header_ok(const eni_data_header_t *got)
{
    if (strcmp(got->start_date, "13.11.26") != 0 ||
        strcmp(got->start_time, "22.37.05") != 0 ||
        strcmp(got->channels[0].label, "EEG Fpz-Cz left1") != 0 ||
        got->channels[0].physical_min != -3276.75 ||
        got->channels[0].physical_max != 3276.75) {
        char msg[200];
        snprintf(msg, sizeof(msg), "read back date='%s' time='%s' label='%s' pmin=%g pmax=%g",
                 got->start_date, got->start_time, got->channels[0].label,
                 got->channels[0].physical_min, got->channels[0].physical_max);
        FAIL(msg);
        return false;
    }
    return true;
}

static void test_edf_header_full_width_fields(void)
{
    TEST(edf_header_full_width_fields);
    const char *path = "/tmp/eni_test_full_width.edf";
    eni_data_header_t hdr;
    fill_full_width_header(&hdr);

    eni_edf_file_t edf;
    assert(eni_edf_create(&edf, path, &hdr) == ENI_OK);
    double samples[4] = {0.0, 1.0, -1.0, 0.5};
    assert(eni_edf_write_record(&edf, samples, 4) == ENI_OK);
    assert(eni_edf_finalize(&edf) == ENI_OK);

    eni_edf_file_t rd;
    assert(eni_edf_open(&rd, path) == ENI_OK);
    bool ok = full_width_header_ok(&rd.header);
    eni_edf_close(&rd);
    remove(path);
    if (ok) PASS();
}

/* An EDF+ record's annotation signal starts with a time-keeping TAL
 * ("+0" 0x14 0x14 0x00); the record's annotations follow it, and one TAL
 * may carry several annotations that share its onset. */
static void test_edf_read_annotations_after_timekeeping_tal(void)
{
    TEST(edf_read_annotations_after_timekeeping_tal);
    const char *path = "/tmp/eni_test_annotations.edf";
    eni_data_header_t hdr;
    eni_data_header_init(&hdr);
    hdr.num_channels = 2;
    hdr.record_duration = 1.0;
    for (int i = 0; i < 2; i++) {
        hdr.channels[i].physical_min = -3200.0;
        hdr.channels[i].physical_max = 3200.0;
        hdr.channels[i].digital_min = -32768;
        hdr.channels[i].digital_max = 32767;
        hdr.channels[i].sample_rate = 4.0;
    }
    strncpy(hdr.channels[0].label, "EEG Fz", ENI_DATA_MAX_LABEL - 1);
    hdr.channels[0].samples_per_record = 4;
    strncpy(hdr.channels[1].label, "EDF Annotations", ENI_DATA_MAX_LABEL - 1);
    hdr.channels[1].samples_per_record = 32; /* 64 bytes of TALs */

    eni_edf_file_t edf;
    assert(eni_edf_create(&edf, path, &hdr) == ENI_OK);
    int64_t data_offset = edf.data_offset;
    double zeros[36] = {0};
    assert(eni_edf_write_record(&edf, zeros, 36) == ENI_OK);
    assert(eni_edf_finalize(&edf) == ENI_OK);

    /* Put TALs in the record's annotation signal (after 4 EEG samples). */
    static const char tals[] =
        "+0" "\x14" "\x14" "\0"
        "+1.5" "\x15" "2" "\x14" "Stim on" "\x14" "\0"
        "+3" "\x14" "Eyes closed" "\x14" "Blink" "\x14" "\0";
    char annot[64] = {0};
    memcpy(annot, tals, sizeof(tals) - 1);
    FILE *fp = fopen(path, "r+b");
    assert(fp);
    assert(fseek(fp, (long)(data_offset + 4 * 2), SEEK_SET) == 0);
    assert(fwrite(annot, 1, sizeof(annot), fp) == sizeof(annot));
    fclose(fp);

    eni_edf_file_t rd;
    assert(eni_edf_open(&rd, path) == ENI_OK);
    assert(eni_edf_read_annotations(&rd) == ENI_OK);
    bool ok = rd.annotation_count == 3 &&
              rd.annotations[0].onset == 1.5 && rd.annotations[0].duration == 2.0 &&
              strcmp(rd.annotations[0].text, "Stim on") == 0 &&
              rd.annotations[1].onset == 3.0 && rd.annotations[1].duration == 0.0 &&
              strcmp(rd.annotations[1].text, "Eyes closed") == 0 &&
              rd.annotations[2].onset == 3.0 &&
              strcmp(rd.annotations[2].text, "Blink") == 0;
    if (!ok) {
        char msg[200];
        snprintf(msg, sizeof(msg), "read %d annotation(s), want 3 (first: onset=%g text='%s')",
                 rd.annotation_count,
                 rd.annotation_count > 0 ? rd.annotations[0].onset : 0.0,
                 rd.annotation_count > 0 ? rd.annotations[0].text : "");
        FAIL(msg);
    }
    eni_edf_close(&rd);
    remove(path);
    if (ok) PASS();
}

/* sample_count is what callers size their buffers from — the Python binding
 * allocates sample_count * channel_count doubles up front — so it must count
 * only payload the file actually holds. A samples chunk whose declared
 * length runs past the end of the file must not inflate it. */
static void test_xdf_open_bounds_sample_count_to_file(void)
{
    TEST(xdf_open_bounds_sample_count_to_file);
    const char *path = "/tmp/eni_test_sample_count_trunc.xdf";
    eni_xdf_stream_t eeg;
    memset(&eeg, 0, sizeof(eeg));
    eeg.stream_id = 1; strcpy(eeg.name, "EEG"); strcpy(eeg.type, "EEG");
    eeg.channel_count = 1; eeg.nominal_srate = 256.0;

    const double eeg_a[4] = {1, 2, 3, 4};

    eni_xdf_file_t wr;
    assert(eni_xdf_create(&wr, path) == ENI_OK);
    assert(eni_xdf_add_stream(&wr, &eeg) == ENI_OK);
    assert(eni_xdf_write_samples(&wr, 1, eeg_a, 4) == ENI_OK);
    /* End of the samples payload, before finalize appends stream footers. */
    long after_samples = ftell(wr.fp);
    assert(after_samples > 24);
    assert(eni_xdf_finalize(&wr) == ENI_OK);

    /* Cut the file 20 bytes into the samples payload: the chunk header still
     * declares four values, but only the first survives whole on disk. */
    long truncated = after_samples - 20;

    FILE *fp = fopen(path, "rb");
    assert(fp);
    char *bytes = (char *)malloc((size_t)truncated);
    assert(bytes);
    assert(fread(bytes, 1, (size_t)truncated, fp) == (size_t)truncated);
    fclose(fp);

    fp = fopen(path, "wb");
    assert(fp);
    assert(fwrite(bytes, 1, (size_t)truncated, fp) == (size_t)truncated);
    fclose(fp);
    free(bytes);

    eni_xdf_file_t rd;
    assert(eni_xdf_open(&rd, path) == ENI_OK);
    int64_t count = -1;
    for (int i = 0; i < rd.stream_count; i++)
        if (rd.streams[i].stream_id == 1) count = rd.streams[i].sample_count;

    /* sample_count must describe what is really there: equal to the values
     * eni_xdf_read_samples() can actually return, not the four the truncated
     * chunk header still claims. */
    double buf[16] = {0};
    int n = 0;
    eni_status_t rs = eni_xdf_read_samples(&rd, 1, buf, 16, &n);
    eni_xdf_close(&rd);
    remove(path);

    bool ok = rs == ENI_OK && n >= 1 && n < 4 && count == (int64_t)n;
    if (!ok) {
        char msg[200];
        snprintf(msg, sizeof(msg),
                 "sample_count=%lld but only %d value(s) are readable "
                 "(chunk header still claims 4)",
                 (long long)count, n);
        FAIL(msg);
    }
    if (ok) PASS();
}

/* Build a one-record EDF+ whose annotation signal holds `tal_bytes` bytes of
 * caller-supplied TALs. */
static void build_tal_file(const char *path, const char *tals, int tal_bytes)
{
    eni_data_header_t hdr;
    eni_data_header_init(&hdr);
    hdr.num_channels = 2;
    hdr.record_duration = 1.0;
    for (int i = 0; i < 2; i++) {
        hdr.channels[i].physical_min = -3200.0;
        hdr.channels[i].physical_max = 3200.0;
        hdr.channels[i].digital_min = -32768;
        hdr.channels[i].digital_max = 32767;
        hdr.channels[i].sample_rate = 4.0;
    }
    strncpy(hdr.channels[0].label, "EEG Fz", ENI_DATA_MAX_LABEL - 1);
    hdr.channels[0].samples_per_record = 4;
    strncpy(hdr.channels[1].label, "EDF Annotations", ENI_DATA_MAX_LABEL - 1);
    hdr.channels[1].samples_per_record = tal_bytes / 2;

    eni_edf_file_t edf;
    assert(eni_edf_create(&edf, path, &hdr) == ENI_OK);
    int64_t data_offset = edf.data_offset;
    int record_samples = 4 + tal_bytes / 2;
    double *zeros = (double *)calloc((size_t)record_samples, sizeof(double));
    assert(zeros);
    assert(eni_edf_write_record(&edf, zeros, record_samples) == ENI_OK);
    assert(eni_edf_finalize(&edf) == ENI_OK);
    free(zeros);

    FILE *fp = fopen(path, "r+b");
    assert(fp);
    assert(fseek(fp, (long)(data_offset + 4 * 2), SEEK_SET) == 0);
    assert(fwrite(tals, 1, (size_t)tal_bytes, fp) == (size_t)tal_bytes);
    fclose(fp);
}

/* Build that file, then read the annotations back. */
static eni_status_t read_back_tals(const char *path, const char *tals, int tal_bytes,
                                   eni_edf_file_t *rd)
{
    build_tal_file(path, tals, tal_bytes);
    assert(eni_edf_open(rd, path) == ENI_OK);
    return eni_edf_read_annotations(rd);
}

/* An annotation whose text does not fit eni_edf_annotation_t::text used to be
 * skipped with no trace at all, and the call still returned ENI_OK. The loss
 * must now be visible: counted in dropped_count and reported as
 * ENI_ERR_OVERFLOW, while the annotations that do fit are still returned. */
static void test_edf_read_annotations_reports_oversized_text(void)
{
    TEST(edf_read_annotations_reports_oversized_text);
    const char *path = "/tmp/eni_test_annot_long.edf";

    /* Time-keeping TAL, then a 300-character annotation (longer than the
     * 256-byte text buffer), then one that fits. */
    char tals[1024];
    memset(tals, 0, sizeof(tals));
    int n = 0;
    memcpy(tals + n, "+0" "\x14" "\x14", 4); n += 4;
    tals[n++] = '\0';
    memcpy(tals + n, "+1" "\x14", 3);        n += 3;
    memset(tals + n, 'x', 300);              n += 300;
    tals[n++] = '\x14';
    tals[n++] = '\0';
    memcpy(tals + n, "+2" "\x14" "Short" "\x14", 9); n += 9;
    tals[n++] = '\0';
    assert(n <= (int)sizeof(tals));

    eni_edf_file_t rd;
    eni_status_t rc = read_back_tals(path, tals, (int)sizeof(tals), &rd);

    bool ok = rc == ENI_ERR_OVERFLOW &&
              rd.dropped_count == 1 &&
              rd.annotation_count == 1 &&
              strcmp(rd.annotations[0].text, "Short") == 0;
    if (!ok) {
        char msg[200];
        snprintf(msg, sizeof(msg),
                 "rc=%d (want %d), annotation_count=%d (want 1), dropped_count=%d (want 1)",
                 (int)rc, (int)ENI_ERR_OVERFLOW, rd.annotation_count, rd.dropped_count);
        FAIL(msg);
    }
    eni_edf_close(&rd);
    remove(path);
    if (ok) PASS();
}

/* Past ENI_EDF_MAX_ANNOTATIONS the parser used to stop and return ENI_OK, so a
 * truncated list was indistinguishable from a short file. It must now keep
 * scanning, count every annotation it cannot store, and report
 * ENI_ERR_OVERFLOW with the exact number lost. */
static void test_edf_read_annotations_reports_table_full(void)
{
    TEST(edf_read_annotations_reports_table_full);
    const char *path = "/tmp/eni_test_annot_cap.edf";

    /* Time-keeping TAL (5 bytes incl. its NUL), then `count` TALs of
     * "+1" 0x14 "a" 0x14 0x00 — 6 bytes each, one annotation each. */
    const int extra = 5;
    const int count = ENI_EDF_MAX_ANNOTATIONS + extra;
    int tal_bytes = 5 + count * 6;
    if (tal_bytes % 2) tal_bytes++;              /* samples are 2 bytes */
    char *tals = (char *)calloc((size_t)tal_bytes, 1);
    assert(tals);
    int n = 0;
    memcpy(tals + n, "+0" "\x14" "\x14", 4); n += 4;
    tals[n++] = '\0';
    for (int i = 0; i < count; i++) {
        memcpy(tals + n, "+1" "\x14" "a" "\x14", 5); n += 5;
        tals[n++] = '\0';
    }
    assert(n <= tal_bytes);

    eni_edf_file_t rd;
    eni_status_t rc = read_back_tals(path, tals, tal_bytes, &rd);
    free(tals);

    bool ok = rc == ENI_ERR_OVERFLOW &&
              rd.annotation_count == ENI_EDF_MAX_ANNOTATIONS &&
              rd.dropped_count == extra;
    if (!ok) {
        char msg[200];
        snprintf(msg, sizeof(msg),
                 "rc=%d (want %d), annotation_count=%d (want %d), dropped_count=%d (want %d)",
                 (int)rc, (int)ENI_ERR_OVERFLOW, rd.annotation_count,
                 ENI_EDF_MAX_ANNOTATIONS, rd.dropped_count, extra);
        FAIL(msg);
    }
    eni_edf_close(&rd);
    remove(path);
    if (ok) PASS();
}

/* The complete-read case must stay ENI_OK with dropped_count == 0, so callers
 * can trust the distinction. */
static void test_edf_read_annotations_complete_read_reports_no_loss(void)
{
    TEST(edf_read_annotations_complete_read_reports_no_loss);
    const char *path = "/tmp/eni_test_annot_ok.edf";
    static const char tals[64] =
        "+0" "\x14" "\x14" "\0"
        "+1.5" "\x15" "2" "\x14" "Stim on" "\x14" "\0";

    eni_edf_file_t rd;
    eni_status_t rc = read_back_tals(path, tals, (int)sizeof(tals), &rd);

    bool ok = rc == ENI_OK && rd.annotation_count == 1 && rd.dropped_count == 0;
    if (!ok) {
        char msg[200];
        snprintf(msg, sizeof(msg), "rc=%d, annotation_count=%d, dropped_count=%d",
                 (int)rc, rd.annotation_count, rd.dropped_count);
        FAIL(msg);
    }
    eni_edf_close(&rd);
    remove(path);
    if (ok) PASS();
}

/* The parser no longer stops once the annotation table is full, so the scan
 * length is bounded by the file rather than by the table. An EDF header can
 * claim any record count, and seeking past the end of a regular file
 * succeeds, so a one-record file claiming 99,999,999 records must not send
 * the parser round a malloc/fseek/fread cycle 99,999,999 times. */
static void test_edf_read_annotations_bounds_scan_to_file(void)
{
    TEST(edf_read_annotations_bounds_scan_to_file);
    const char *path = "/tmp/eni_test_annot_record_claim.edf";
    static const char tals[64] =
        "+0" "\x14" "\x14" "\0"
        "+1.5" "\x15" "2" "\x14" "Stim on" "\x14" "\0";

    build_tal_file(path, tals, (int)sizeof(tals));

    /* Claim 99,999,999 records in a file that holds exactly one. Offset 236
     * is the EDF header's "number of data records" field. */
    FILE *fp = fopen(path, "r+b");
    assert(fp);
    assert(fseek(fp, 236, SEEK_SET) == 0);
    assert(fwrite("99999999", 1, 8, fp) == 8);
    fclose(fp);

    eni_edf_file_t rd;
    assert(eni_edf_open(&rd, path) == ENI_OK);

    clock_t started = clock();
    eni_status_t rc = eni_edf_read_annotations(&rd);
    double elapsed = (double)(clock() - started) / (double)CLOCKS_PER_SEC;

    /* Generous bound: the clamped scan is one record and finishes in well
     * under a millisecond, while the unbounded scan takes minutes. */
    bool ok = rc == ENI_OK && rd.annotation_count == 1 &&
              rd.dropped_count == 0 && elapsed < 5.0;
    if (!ok) {
        char msg[200];
        snprintf(msg, sizeof(msg),
                 "rc=%d, annotation_count=%d, dropped_count=%d, elapsed=%.3fs",
                 (int)rc, rd.annotation_count, rd.dropped_count, elapsed);
        FAIL(msg);
    }
    eni_edf_close(&rd);
    remove(path);
    if (ok) PASS();
}

static void test_bdf_header_full_width_fields(void)
{
    TEST(bdf_header_full_width_fields);
    const char *path = "/tmp/eni_test_full_width.bdf";
    eni_data_header_t hdr;
    fill_full_width_header(&hdr);
    hdr.channels[0].digital_min = -8388608;
    hdr.channels[0].digital_max = 8388607;

    eni_bdf_file_t bdf;
    assert(eni_bdf_create(&bdf, path, &hdr) == ENI_OK);
    double samples[4] = {0.0, 1.0, -1.0, 0.5};
    assert(eni_bdf_write_record(&bdf, samples, 4) == ENI_OK);
    assert(eni_bdf_finalize(&bdf) == ENI_OK);

    eni_bdf_file_t rd;
    assert(eni_bdf_open(&rd, path) == ENI_OK);
    bool ok = full_width_header_ok(&rd.header);
    eni_bdf_close(&rd);
    remove(path);
    if (ok) PASS();
}

static void test_annotation_system(void)
{
    TEST(annotation_system);
    eni_annotation_list_t list;
    eni_annotation_list_init(&list);

    assert(eni_annotation_add_marker(&list, 1.0, "event1") == ENI_OK);
    assert(eni_annotation_add_marker(&list, 3.0, "event2") == ENI_OK);
    assert(eni_annotation_add_epoch(&list, 2.0, 1.0, "epoch1") == ENI_OK);
    assert(list.count == 3);

    eni_annotation_sort_by_onset(&list);
    assert(list.entries[0].onset == 1.0);
    assert(list.entries[1].onset == 2.0);

    const eni_annotation_t *results[10];
    int found = eni_annotation_find_in_range(&list, 0.5, 2.5, results, 10);
    assert(found >= 2);

    assert(eni_annotation_add_tag(&list, 1, "tag1") == ENI_OK);
    assert(list.entries[0].tag_count == 1);

    assert(eni_annotation_remove(&list, 1) == ENI_OK);
    assert(list.count == 2);

    PASS();
}

/* eni_xdf_open() must report how many samples each stream holds, so a reader
 * can size its buffer as sample_count * channel_count (read_xdf() in the
 * Python binding does exactly that). */
static void test_xdf_open_sets_sample_count(void)
{
    TEST(xdf_open_sets_sample_count);
    const char *path = "/tmp/eni_test_sample_count.xdf";
    eni_xdf_stream_t eeg, mrk;
    memset(&eeg, 0, sizeof(eeg));
    memset(&mrk, 0, sizeof(mrk));
    eeg.stream_id = 1; strcpy(eeg.name, "EEG"); strcpy(eeg.type, "EEG");
    eeg.channel_count = 2; eeg.nominal_srate = 256.0;
    mrk.stream_id = 2; strcpy(mrk.name, "Markers"); strcpy(mrk.type, "Markers");
    mrk.channel_count = 1; mrk.nominal_srate = 0.0;

    const double eeg_a[6] = {1, 2, 3, 4, 5, 6};
    const double eeg_b[4] = {7, 8, 9, 10};
    const double mrk_a[3] = {100, 200, 300};

    eni_xdf_file_t wr;
    assert(eni_xdf_create(&wr, path) == ENI_OK);
    assert(eni_xdf_add_stream(&wr, &eeg) == ENI_OK);
    assert(eni_xdf_add_stream(&wr, &mrk) == ENI_OK);
    assert(eni_xdf_write_samples(&wr, 1, eeg_a, 6) == ENI_OK);
    assert(eni_xdf_write_samples(&wr, 2, mrk_a, 3) == ENI_OK);
    assert(eni_xdf_write_clock_offset(&wr, 1, 0.25) == ENI_OK);
    assert(eni_xdf_write_samples(&wr, 1, eeg_b, 4) == ENI_OK);
    assert(eni_xdf_finalize(&wr) == ENI_OK);

    eni_xdf_file_t rd;
    assert(eni_xdf_open(&rd, path) == ENI_OK);
    int64_t eeg_count = -1, mrk_count = -1;
    int eeg_channels = 0;
    for (int i = 0; i < rd.stream_count; i++) {
        if (rd.streams[i].stream_id == 1) {
            eeg_count = rd.streams[i].sample_count;
            eeg_channels = rd.streams[i].channel_count;
        }
        if (rd.streams[i].stream_id == 2) mrk_count = rd.streams[i].sample_count;
    }
    /* A buffer sized from sample_count must hold every value of the stream. */
    double buf[16] = {0};
    int n = 0;
    int cap = (int)(eeg_count * eeg_channels);
    if (cap < 1) cap = 1;
    if (cap > 16) cap = 16;
    eni_status_t rs = eni_xdf_read_samples(&rd, 1, buf, cap, &n);
    eni_xdf_close(&rd);
    remove(path);

    if (eeg_count == 5 && mrk_count == 3 && rs == ENI_OK && n == 10 &&
        fabs(buf[0] - 1.0) < 1e-9 && fabs(buf[9] - 10.0) < 1e-9) {
        PASS();
    } else {
        char msg[160];
        snprintf(msg, sizeof(msg), "sample_count EEG=%lld Markers=%lld (want 5, 3); read %d of 10 EEG values",
                 (long long)eeg_count, (long long)mrk_count, n);
        FAIL(msg);
    }
}

#ifndef _WIN32
static void recorder_stop_timeout(int sig)
{
    (void)sig;
    static const char msg[] = "[FAIL] eni_recorder_stop() did not return within 5 s\n";
    (void)!write(1, msg, sizeof(msg) - 1);
    _exit(1);
}
#endif

/* Stopping with fewer samples buffered than one record holds must finish:
 * whole records are written, the incomplete tail is dropped. */
static void test_recorder_stop_with_partial_record(void)
{
    TEST(recorder_stop_with_partial_record);
    const char *path = "/tmp/eni_test_recorder.edf";
    eni_data_header_t hdr;
    eni_data_header_init(&hdr);
    hdr.num_channels = 1;
    hdr.record_duration = 1.0;
    snprintf(hdr.channels[0].label, ENI_DATA_MAX_LABEL, "Ch0");
    hdr.channels[0].physical_min = -100.0;
    hdr.channels[0].physical_max = 100.0;
    hdr.channels[0].digital_min = -32768;
    hdr.channels[0].digital_max = 32767;
    hdr.channels[0].samples_per_record = 4;
    hdr.channels[0].sample_rate = 4.0;

    static eni_recorder_t rec;
    eni_status_t rc = eni_recorder_init(&rec, ENI_FORMAT_EDF, &hdr);
    assert(rc == ENI_OK);
    rc = eni_recorder_start(&rec, path);
    assert(rc == ENI_OK);

    double samples[10]; /* two whole records plus 2 samples */
    for (int i = 0; i < 10; i++) samples[i] = (double)i;
    rc = eni_recorder_push_samples(&rec, samples, 10);
    assert(rc == ENI_OK);

    fflush(stdout);
#ifndef _WIN32
    signal(SIGALRM, recorder_stop_timeout);
    alarm(5);
#endif
    rc = eni_recorder_stop(&rec);
#ifndef _WIN32
    alarm(0);
#endif
    assert(rc == ENI_OK);
    assert(rec.state == ENI_RECORDER_STOPPED);
    if (rec.records_written != 2 || rec.samples_written != 8) {
        char msg[96];
        snprintf(msg, sizeof(msg), "records_written=%llu samples_written=%llu, expected 2 and 8",
                 (unsigned long long)rec.records_written,
                 (unsigned long long)rec.samples_written);
        FAIL(msg);
        eni_recorder_destroy(&rec);
        return;
    }
    eni_recorder_destroy(&rec);

    eni_edf_file_t edf;
    rc = eni_edf_open(&edf, path);
    assert(rc == ENI_OK);
    assert(edf.header.num_records == 2);
    double back[4];
    rc = eni_edf_read_record(&edf, 1, back, 4);
    assert(rc == ENI_OK);
    assert(fabs(back[3] - 7.0) < 0.01);
    eni_edf_close(&edf);
    remove(path);
    PASS();
}

/* Two channels of 2 samples per record: a record is 4 samples. A call with
 * fewer must be refused, not written as a short record that shifts every
 * record after it. */
static void short_record_header(eni_data_header_t *hdr)
{
    eni_data_header_init(hdr);
    hdr->num_channels = 2;
    hdr->record_duration = 1.0;
    for (int i = 0; i < 2; i++) {
        snprintf(hdr->channels[i].label, ENI_DATA_MAX_LABEL, "Ch%d", i);
        hdr->channels[i].physical_min = -100.0;
        hdr->channels[i].physical_max = 100.0;
        hdr->channels[i].samples_per_record = 2;
        hdr->channels[i].sample_rate = 2.0;
    }
}

static void test_edf_bdf_write_short_record_refused(void)
{
    TEST(edf_bdf_write_short_record_refused);
    const double rec0[4] = {1.0, 2.0, 3.0, 4.0};
    const double rec1[4] = {5.0, 6.0, 7.0, 8.0};
    const double short_rec[3] = {9.0, 9.0, 9.0};
    char msg[320] = "";

    const char *edf_path = "/tmp/eni_test_short_record.edf";
    eni_data_header_t hdr;
    short_record_header(&hdr);
    for (int i = 0; i < 2; i++) { hdr.channels[i].digital_min = -32768; hdr.channels[i].digital_max = 32767; }
    eni_edf_file_t edf;
    assert(eni_edf_create(&edf, edf_path, &hdr) == ENI_OK);
    assert(eni_edf_write_record(&edf, rec0, 4) == ENI_OK);
    eni_status_t edf_short = eni_edf_write_record(&edf, short_rec, 3);
    assert(eni_edf_write_record(&edf, rec1, 4) == ENI_OK);
    assert(eni_edf_finalize(&edf) == ENI_OK);
    eni_edf_file_t erd;
    assert(eni_edf_open(&erd, edf_path) == ENI_OK);
    double eback[4] = {0};
    int64_t edf_records = erd.header.num_records;
    eni_status_t edf_read = eni_edf_read_record(&erd, 1, eback, 4);
    eni_edf_close(&erd);
    remove(edf_path);
    if (edf_short != ENI_ERR_INVALID || edf_records != 2 || edf_read != ENI_OK ||
        fabs(eback[0] - 5.0) > 0.01 || fabs(eback[3] - 8.0) > 0.01)
        snprintf(msg, sizeof(msg), "EDF: short write rc=%d, %lld records, record 1 = %g..%g (want rc=%d, 2, 5..8)",
                 (int)edf_short, (long long)edf_records, eback[0], eback[3], (int)ENI_ERR_INVALID);

    const char *bdf_path = "/tmp/eni_test_short_record.bdf";
    short_record_header(&hdr);
    for (int i = 0; i < 2; i++) { hdr.channels[i].digital_min = -8388608; hdr.channels[i].digital_max = 8388607; }
    eni_bdf_file_t bdf;
    assert(eni_bdf_create(&bdf, bdf_path, &hdr) == ENI_OK);
    assert(eni_bdf_write_record(&bdf, rec0, 4) == ENI_OK);
    eni_status_t bdf_short = eni_bdf_write_record(&bdf, short_rec, 3);
    assert(eni_bdf_write_record(&bdf, rec1, 4) == ENI_OK);
    assert(eni_bdf_finalize(&bdf) == ENI_OK);
    eni_bdf_file_t brd;
    assert(eni_bdf_open(&brd, bdf_path) == ENI_OK);
    double bback[4] = {0};
    int64_t bdf_records = brd.header.num_records;
    eni_status_t bdf_read = eni_bdf_read_record(&brd, 1, bback, 4);
    eni_bdf_close(&brd);
    remove(bdf_path);
    if (bdf_short != ENI_ERR_INVALID || bdf_records != 2 || bdf_read != ENI_OK ||
        fabs(bback[0] - 5.0) > 0.01 || fabs(bback[3] - 8.0) > 0.01) {
        size_t used = strlen(msg);
        snprintf(msg + used, sizeof(msg) - used, "%sBDF: short write rc=%d, %lld records, record 1 = %g..%g",
                 used ? "; " : "", (int)bdf_short, (long long)bdf_records, bback[0], bback[3]);
    }

    if (msg[0]) FAIL(msg);
    else PASS();
}

/* The mirror case: an oversized call used to write one record, silently drop
 * the tail and still advance num_records, so the caller lost data and was
 * told it had succeeded. It must now be refused like a short one, leaving the
 * file byte-identical. */
static void test_edf_bdf_write_long_record_refused(void)
{
    TEST(edf_bdf_write_long_record_refused);
    const double rec0[4] = {1.0, 2.0, 3.0, 4.0};
    const double rec1[4] = {5.0, 6.0, 7.0, 8.0};
    const double long_rec[6] = {9.0, 9.0, 9.0, 9.0, 9.0, 9.0}; /* 1.5 records */
    char msg[320] = "";

    const char *edf_path = "/tmp/eni_test_long_record.edf";
    eni_data_header_t hdr;
    short_record_header(&hdr);
    for (int i = 0; i < 2; i++) { hdr.channels[i].digital_min = -32768; hdr.channels[i].digital_max = 32767; }
    eni_edf_file_t edf;
    assert(eni_edf_create(&edf, edf_path, &hdr) == ENI_OK);
    assert(eni_edf_write_record(&edf, rec0, 4) == ENI_OK);
    long edf_before = ftell(edf.fp);
    eni_status_t edf_long = eni_edf_write_record(&edf, long_rec, 6);
    long edf_after = ftell(edf.fp);
    assert(eni_edf_write_record(&edf, rec1, 4) == ENI_OK);
    assert(eni_edf_finalize(&edf) == ENI_OK);

    eni_edf_file_t erd;
    assert(eni_edf_open(&erd, edf_path) == ENI_OK);
    double eback[4] = {0};
    int64_t edf_records = erd.header.num_records;
    eni_status_t edf_read = eni_edf_read_record(&erd, 1, eback, 4);
    eni_edf_close(&erd);
    remove(edf_path);
    if (edf_long != ENI_ERR_INVALID || edf_before != edf_after || edf_records != 2 ||
        edf_read != ENI_OK || fabs(eback[0] - 5.0) > 0.01 || fabs(eback[3] - 8.0) > 0.01)
        snprintf(msg, sizeof(msg),
                 "EDF: long write rc=%d, wrote %ld bytes, %lld records, record 1 = %g..%g "
                 "(want rc=%d, 0 bytes, 2, 5..8)",
                 (int)edf_long, edf_after - edf_before, (long long)edf_records,
                 eback[0], eback[3], (int)ENI_ERR_INVALID);

    const char *bdf_path = "/tmp/eni_test_long_record.bdf";
    short_record_header(&hdr);
    for (int i = 0; i < 2; i++) { hdr.channels[i].digital_min = -8388608; hdr.channels[i].digital_max = 8388607; }
    eni_bdf_file_t bdf;
    assert(eni_bdf_create(&bdf, bdf_path, &hdr) == ENI_OK);
    assert(eni_bdf_write_record(&bdf, rec0, 4) == ENI_OK);
    long bdf_before = ftell(bdf.fp);
    eni_status_t bdf_long = eni_bdf_write_record(&bdf, long_rec, 6);
    long bdf_after = ftell(bdf.fp);
    assert(eni_bdf_write_record(&bdf, rec1, 4) == ENI_OK);
    assert(eni_bdf_finalize(&bdf) == ENI_OK);

    eni_bdf_file_t brd;
    assert(eni_bdf_open(&brd, bdf_path) == ENI_OK);
    double bback[4] = {0};
    int64_t bdf_records = brd.header.num_records;
    eni_status_t bdf_read = eni_bdf_read_record(&brd, 1, bback, 4);
    eni_bdf_close(&brd);
    remove(bdf_path);
    if (bdf_long != ENI_ERR_INVALID || bdf_before != bdf_after || bdf_records != 2 ||
        bdf_read != ENI_OK || fabs(bback[0] - 5.0) > 0.01 || fabs(bback[3] - 8.0) > 0.01) {
        size_t used = strlen(msg);
        snprintf(msg + used, sizeof(msg) - used,
                 "%sBDF: long write rc=%d, wrote %ld bytes, %lld records, record 1 = %g..%g",
                 used ? "; " : "", (int)bdf_long, bdf_after - bdf_before,
                 (long long)bdf_records, bback[0], bback[3]);
    }

    if (msg[0]) FAIL(msg);
    else PASS();
}

/* record_samples is stored once instead of being recomputed per write, so it
 * must be right on both the create and the open path, and must agree with
 * record_size. */
static void test_edf_bdf_record_samples_stored(void)
{
    TEST(edf_bdf_record_samples_stored);
    char msg[320] = "";

    eni_data_header_t hdr;
    eni_data_header_init(&hdr);
    hdr.num_channels = 3;
    hdr.record_duration = 1.0;
    const int spr[3] = {8, 16, 4};   /* 28 samples per record */
    for (int i = 0; i < 3; i++) {
        snprintf(hdr.channels[i].label, ENI_DATA_MAX_LABEL, "Ch%d", i);
        hdr.channels[i].physical_min = -100.0;
        hdr.channels[i].physical_max = 100.0;
        hdr.channels[i].digital_min = -32768;
        hdr.channels[i].digital_max = 32767;
        hdr.channels[i].samples_per_record = spr[i];
        hdr.channels[i].sample_rate = (double)spr[i];
    }
    const int want = 28;

    const char *edf_path = "/tmp/eni_test_record_samples.edf";
    eni_edf_file_t edf;
    assert(eni_edf_create(&edf, edf_path, &hdr) == ENI_OK);
    int edf_create = edf.record_samples;
    int edf_size = edf.record_size;
    double samples[28] = {0};
    assert(eni_edf_write_record(&edf, samples, want) == ENI_OK);
    assert(eni_edf_finalize(&edf) == ENI_OK);
    eni_edf_file_t erd;
    assert(eni_edf_open(&erd, edf_path) == ENI_OK);
    int edf_open = erd.record_samples;
    eni_edf_close(&erd);
    remove(edf_path);
    if (edf_create != want || edf_open != want || edf_size != want * 2)
        snprintf(msg, sizeof(msg),
                 "EDF: create=%d open=%d record_size=%d (want %d, %d, %d)",
                 edf_create, edf_open, edf_size, want, want, want * 2);

    const char *bdf_path = "/tmp/eni_test_record_samples.bdf";
    for (int i = 0; i < 3; i++) { hdr.channels[i].digital_min = -8388608; hdr.channels[i].digital_max = 8388607; }
    eni_bdf_file_t bdf;
    assert(eni_bdf_create(&bdf, bdf_path, &hdr) == ENI_OK);
    int bdf_create = bdf.record_samples;
    int bdf_size = bdf.record_size;
    assert(eni_bdf_write_record(&bdf, samples, want) == ENI_OK);
    assert(eni_bdf_finalize(&bdf) == ENI_OK);
    eni_bdf_file_t brd;
    assert(eni_bdf_open(&brd, bdf_path) == ENI_OK);
    int bdf_open = brd.record_samples;
    eni_bdf_close(&brd);
    remove(bdf_path);
    if (bdf_create != want || bdf_open != want || bdf_size != want * 3) {
        size_t used = strlen(msg);
        snprintf(msg + used, sizeof(msg) - used,
                 "%sBDF: create=%d open=%d record_size=%d (want %d, %d, %d)",
                 used ? "; " : "", bdf_create, bdf_open, bdf_size, want, want, want * 3);
    }

    if (msg[0]) FAIL(msg);
    else PASS();
}

int main(void)
{
    printf("=== Data Format Tests ===\n");
    test_format_detect();
    test_format_names();
    test_native_read_samples_across_calls();
    test_edf_digital_physical_conversion();
    test_edf_write_read_roundtrip();
    test_edf_header_full_width_fields();
    test_edf_read_annotations_after_timekeeping_tal();
    test_edf_read_annotations_reports_oversized_text();
    test_edf_read_annotations_reports_table_full();
    test_edf_read_annotations_complete_read_reports_no_loss();
    test_edf_read_annotations_bounds_scan_to_file();
    test_bdf_header_full_width_fields();
    test_annotation_system();
    test_xdf_open_sets_sample_count();
    test_xdf_open_bounds_sample_count_to_file();
    test_recorder_stop_with_partial_record();
    test_edf_bdf_write_short_record_refused();
    test_edf_bdf_write_long_record_refused();
    test_edf_bdf_record_samples_stored();
    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return tests_failed > 0 ? 1 : 0;
}
