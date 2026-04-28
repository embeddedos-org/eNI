// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Tests for data format modules

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include "eni/data_format.h"
#include "eni/edf.h"
#include "eni/annotation.h"

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

    assert(eni_data_format_detect((uint8_t*)"????", 4) == ENI_FORMAT_UNKNOWN);
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

int main(void)
{
    printf("=== Data Format Tests ===\n");
    test_format_detect();
    test_format_names();
    test_edf_digital_physical_conversion();
    test_edf_write_read_roundtrip();
    test_annotation_system();
    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return tests_failed > 0 ? 1 : 0;
}
