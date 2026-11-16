// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Tests for the data player

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "eni/data_format.h"
#include "eni/edf.h"
#include "eni/player.h"

static int tests_run = 0, tests_passed = 0, tests_failed = 0;
#define TEST(name) do { tests_run++; printf("  TEST %-40s ", #name); } while(0)
#define PASS() do { tests_passed++; printf("[PASS]\n"); } while(0)
#define FAIL(msg) do { tests_failed++; printf("[FAIL] %s\n", msg); } while(0)

#define NUM_RECORDS 3

/* Relative path: ctest runs this in the build tree. */
static const char *k_path = "eni_test_player.edf";

static bool write_test_edf(void)
{
    eni_data_header_t hdr;
    eni_data_header_init(&hdr);
    hdr.num_channels = 1;
    hdr.record_duration = 0.01;
    snprintf(hdr.channels[0].label, ENI_DATA_MAX_LABEL, "Ch0");
    hdr.channels[0].physical_min = -100.0;
    hdr.channels[0].physical_max = 100.0;
    hdr.channels[0].digital_min = -32768;
    hdr.channels[0].digital_max = 32767;
    hdr.channels[0].samples_per_record = 4;
    hdr.channels[0].sample_rate = 400.0;

    eni_edf_file_t edf;
    if (eni_edf_create(&edf, k_path, &hdr) != ENI_OK) return false;
    double samples[4] = {0.0, 1.0, -1.0, 0.5};
    for (int r = 0; r < NUM_RECORDS; r++)
        if (eni_edf_write_record(&edf, samples, 4) != ENI_OK) return false;
    return eni_edf_finalize(&edf) == ENI_OK;
}

static volatile int g_records_seen;

static void count_records(const double *samples, int count, void *user_data)
{
    (void)samples; (void)count; (void)user_data;
    g_records_seen++;
}

/* Wait (up to ~2 s) for the playback thread to deliver every record. */
static void wait_for_records(int want)
{
    for (int i = 0; i < 200 && g_records_seen < want; i++)
        eni_platform_sleep_ms(10);
}

static void test_play_after_pause_before_play(void)
{
    TEST(player_play_after_pause_before_play);
    if (!write_test_edf()) { FAIL("could not write the test EDF"); return; }

    eni_player_t player;
    if (eni_player_open(&player, k_path) != ENI_OK) { FAIL("open failed"); remove(k_path); return; }
    g_records_seen = 0;
    eni_player_set_callback(&player, count_records, NULL);

    /* Pausing a player that is not playing must not stop play() working. */
    eni_player_pause(&player);
    eni_status_t rc = eni_player_play(&player);
    wait_for_records(NUM_RECORDS);

    if (rc != ENI_OK || g_records_seen != NUM_RECORDS) {
        char msg[120];
        snprintf(msg, sizeof(msg), "play() returned %d and delivered %d of %d records",
                 (int)rc, g_records_seen, NUM_RECORDS);
        /* Playback never started: skip stop()/close(). */
        FAIL(msg);
        remove(k_path);
        return;
    }

    eni_player_stop(&player);
    eni_player_close(&player);
    remove(k_path);
    PASS();
}

static void test_play_to_end(void)
{
    TEST(player_play_to_end);
    if (!write_test_edf()) { FAIL("could not write the test EDF"); return; }

    eni_player_t player;
    if (eni_player_open(&player, k_path) != ENI_OK) { FAIL("open failed"); remove(k_path); return; }
    g_records_seen = 0;
    eni_player_set_callback(&player, count_records, NULL);

    eni_status_t rc = eni_player_play(&player);
    wait_for_records(NUM_RECORDS);
    eni_player_stop(&player);
    eni_player_close(&player);
    remove(k_path);

    if (rc != ENI_OK || g_records_seen != NUM_RECORDS) { FAIL("records missing"); return; }
    PASS();
}

int main(void)
{
    printf("=== Player Tests ===\n");
    test_play_to_end();
    test_play_after_pause_before_play();
    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return tests_failed > 0 ? 1 : 0;
}
