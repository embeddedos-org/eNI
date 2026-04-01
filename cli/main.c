// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "eni/common.h"
#include "eni_min/service.h"
#include "eni_fw/service.h"
#include "eni_platform/platform.h"
#include "simulator.h"

#ifdef ENI_HAS_EEG
#include "eeg.h"
#endif
#ifdef ENI_HAS_STIMULATOR_SIM
#include "stimulator_sim.h"
#endif

static eni_status_t tool_cursor_move(const eni_tool_call_t *call, eni_tool_result_t *result)
{
    (void)call;
    const char *msg = "cursor moved";
    size_t len = strlen(msg);
    memcpy(result->data, msg, len);
    result->len = len;
    result->status = ENI_OK;
    return ENI_OK;
}

static eni_status_t tool_select(const eni_tool_call_t *call, eni_tool_result_t *result)
{
    (void)call;
    const char *msg = "selection made";
    size_t len = strlen(msg);
    memcpy(result->data, msg, len);
    result->len = len;
    result->status = ENI_OK;
    return ENI_OK;
}

static void print_usage(void)
{
    printf("ENI — Neural Interface Adapter v%s\n\n", ENI_VERSION_STRING);
    printf("Usage: eni <command> [options]\n\n");
    printf("Commands:\n");
    printf("  run-min       Run ENI-Min with simulator provider\n");
    printf("  run-fw        Run ENI-Framework with simulator provider\n");
    printf("  info          Show platform information\n");
    printf("  version       Show version\n");
    printf("  help          Show this help\n");
    printf("\nOptions:\n");
    printf("  --eeg              Use EEG provider instead of simulator\n");
    printf("  --decoder=TYPE     Decoder type: energy, nn (default: energy)\n");
    printf("  --model=PATH       Model path for nn decoder\n");
    printf("  --feedback         Enable feedback loop with simulated stimulator\n");
}

typedef struct {
    int use_eeg;
    int use_feedback;
    const char *decoder_type;
    const char *model_path;
} cli_opts_t;

static void parse_opts(int argc, char *argv[], cli_opts_t *opts)
{
    memset(opts, 0, sizeof(*opts));
    opts->decoder_type = "energy";
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--eeg") == 0) opts->use_eeg = 1;
        else if (strcmp(argv[i], "--feedback") == 0) opts->use_feedback = 1;
        else if (strncmp(argv[i], "--decoder=", 10) == 0) opts->decoder_type = argv[i] + 10;
        else if (strncmp(argv[i], "--model=", 8) == 0) opts->model_path = argv[i] + 8;
    }
}

static int cmd_info(void)
{
    eni_platform_init();
    eni_platform_info_t pi = eni_platform_info();

    printf("ENI v%s\n", ENI_VERSION_STRING);
    printf("Platform: %s (%s)\n", pi.os_name, pi.arch);
    printf("Real-time: %s\n", pi.realtime_capable ? "yes" : "no");
    printf("Hardware:  %s\n", pi.hardware_access  ? "yes" : "no");
#ifdef ENI_HAS_DSP
    printf("DSP:       enabled\n");
#else
    printf("DSP:       disabled\n");
#endif
#ifdef ENI_HAS_DECODER
    printf("Decoder:   enabled\n");
#else
    printf("Decoder:   disabled\n");
#endif
#ifdef ENI_HAS_STIMULATOR
    printf("Feedback:  enabled\n");
#else
    printf("Feedback:  disabled\n");
#endif
    return 0;
}

static int cmd_run_min(int ticks, const cli_opts_t *opts)
{
    eni_config_t cfg;
    eni_config_load_defaults(&cfg, ENI_VARIANT_MIN);

    const eni_provider_ops_t *prov_ops = &eni_provider_simulator_ops;
    const char *prov_name = "simulator";

#ifdef ENI_HAS_EEG
    if (opts->use_eeg) {
        eni_eeg_config_t eeg_cfg = {0};
        eeg_cfg.channels = 21;
        eeg_cfg.sample_rate = 256;
        if (eni_eeg_init(&eeg_cfg) != 0) {
            fprintf(stderr, "EEG init failed\n");
            return 1;
        }
        eni_eeg_connect(NULL);
        prov_ops = eni_eeg_get_provider();
        prov_name = "eeg";
    }
#else
    (void)opts;
#endif

    eni_min_service_t svc;
    eni_status_t st = eni_min_service_init(&svc, &cfg, prov_ops);
    if (st != ENI_OK) {
        fprintf(stderr, "init failed: %s\n", eni_status_str(st));
        return 1;
    }

    /* Register mappings */
    eni_min_mapper_add(&svc.mapper, "move_left",    "ui.cursor.move");
    eni_min_mapper_add(&svc.mapper, "move_right",   "ui.cursor.move");
    eni_min_mapper_add(&svc.mapper, "select",       "ui.select");
    eni_min_mapper_add(&svc.mapper, "scroll_up",    "ui.cursor.move");
    eni_min_mapper_add(&svc.mapper, "scroll_down",  "ui.cursor.move");
    eni_min_mapper_add(&svc.mapper, "activate",     "ui.select");
    eni_min_mapper_add(&svc.mapper, "deactivate",   "ui.select");
    eni_min_mapper_add(&svc.mapper, "idle",         "ui.cursor.move");
    eni_min_mapper_add(&svc.mapper, "attention",    "ui.cursor.move");
    eni_min_mapper_add(&svc.mapper, "motor_intent", "ui.cursor.move");
    eni_min_mapper_add(&svc.mapper, "motor_execute","ui.select");

    /* Register tools */
    eni_tool_entry_t cursor_tool = { .name = "ui.cursor.move", .description = "Move cursor", .exec = tool_cursor_move };
    eni_tool_entry_t select_tool = { .name = "ui.select", .description = "Select item", .exec = tool_select };
    eni_min_tool_bridge_register(&svc.tool_bridge, &cursor_tool);
    eni_min_tool_bridge_register(&svc.tool_bridge, &select_tool);

    st = eni_min_service_start(&svc);
    if (st != ENI_OK) {
        fprintf(stderr, "start failed: %s\n", eni_status_str(st));
        return 1;
    }

    printf("ENI-Min running (%s, %d ticks)...\n", prov_name, ticks);
    for (int i = 0; i < ticks; i++) {
        eni_min_service_tick(&svc);
    }

    eni_min_service_stats(&svc);
    eni_min_service_shutdown(&svc);
    return 0;
}

static int cmd_run_fw(int ticks, const cli_opts_t *opts)
{
    (void)opts;
    eni_config_t cfg;
    eni_config_load_defaults(&cfg, ENI_VARIANT_FRAMEWORK);

    eni_fw_service_t svc;
    eni_status_t st = eni_fw_service_init(&svc, &cfg);
    if (st != ENI_OK) {
        fprintf(stderr, "init failed: %s\n", eni_status_str(st));
        return 1;
    }

    st = eni_fw_service_add_provider(&svc, &eni_provider_simulator_ops, "simulator");
    if (st != ENI_OK) {
        fprintf(stderr, "add provider failed: %s\n", eni_status_str(st));
        return 1;
    }

    st = eni_fw_service_start(&svc);
    if (st != ENI_OK) {
        fprintf(stderr, "start failed: %s\n", eni_status_str(st));
        return 1;
    }

    printf("ENI-Framework running (simulator, %d ticks)...\n", ticks);
    for (int i = 0; i < ticks; i++) {
        eni_fw_service_tick(&svc);
    }

    eni_fw_service_stats(&svc);
    eni_fw_service_shutdown(&svc);
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        print_usage();
        return 0;
    }

    eni_log_set_level(ENI_LOG_INFO);

    const char *cmd = argv[1];

    if (strcmp(cmd, "version") == 0) {
        printf("ENI v%s\n", ENI_VERSION_STRING);
        return 0;
    }

    if (strcmp(cmd, "help") == 0) {
        print_usage();
        return 0;
    }

    if (strcmp(cmd, "info") == 0) {
        return cmd_info();
    }

    cli_opts_t opts;
    parse_opts(argc, argv, &opts);

    int ticks = 30;
    for (int i = 2; i < argc; i++) {
        if (argv[i][0] != '-') {
            int t = atoi(argv[i]);
            if (t > 0) { ticks = t; break; }
        }
    }

    if (strcmp(cmd, "run-min") == 0) {
        return cmd_run_min(ticks, &opts);
    }

    if (strcmp(cmd, "run-fw") == 0) {
        return cmd_run_fw(ticks, &opts);
    }

    fprintf(stderr, "Unknown command: %s\n", cmd);
    print_usage();
    return 1;
}
