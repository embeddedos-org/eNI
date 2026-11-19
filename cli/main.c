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

#if defined(ENI_HAS_EEG) && defined(ENI_HAS_DSP) && defined(ENI_HAS_DECODER)
#define ENI_CLI_HAS_EEG_PIPELINE 1
#endif
#if defined(ENI_HAS_STIMULATOR) && defined(ENI_HAS_STIMULATOR_SIM)
#include "eni_fw/feedback.h"
#define ENI_CLI_HAS_FEEDBACK 1
#endif

#define CLI_DEFAULT_TICKS 30
/* The min signal processor's default epoch is 256 samples and EEG yields one sample per tick. */
#define CLI_EEG_DEFAULT_TICKS 1024
#define CLI_EEG_SAMPLE_RATE   256
#define CLI_EXIT_USAGE        2

/* ── Simulated effector ─────────────────────────────────────────────── */

#define EFFECTOR_GRID 100
#define EFFECTOR_STEP 10

typedef enum {
    EFF_LEFT,
    EFF_RIGHT,
    EFF_UP,
    EFF_DOWN,
    EFF_SELECT,
    EFF_ACTIVATE,
    EFF_DEACTIVATE,
    EFF_HOLD,
} effector_op_t;

static const char *const k_effector_op_names[] = {
    "cursor.left", "cursor.right", "cursor.up", "cursor.down",
    "select", "activate", "deactivate", "hold",
};

typedef struct {
    int      x;
    int      y;
    int      selections;
    bool     active;
    unsigned actions;
} effector_t;

static effector_t g_effector;

static void effector_reset(void)
{
    memset(&g_effector, 0, sizeof(g_effector));
    g_effector.x = EFFECTOR_GRID / 2;
    g_effector.y = EFFECTOR_GRID / 2;
}

static int effector_clamp(int v)
{
    if (v < 0) return 0;
    if (v > EFFECTOR_GRID) return EFFECTOR_GRID;
    return v;
}

static eni_status_t effector_apply(effector_op_t op, const eni_tool_call_t *call,
                                   eni_tool_result_t *result)
{
    effector_t *e = &g_effector;
    switch (op) {
    case EFF_LEFT:       e->x = effector_clamp(e->x - EFFECTOR_STEP); break;
    case EFF_RIGHT:      e->x = effector_clamp(e->x + EFFECTOR_STEP); break;
    case EFF_UP:         e->y = effector_clamp(e->y - EFFECTOR_STEP); break;
    case EFF_DOWN:       e->y = effector_clamp(e->y + EFFECTOR_STEP); break;
    case EFF_SELECT:     e->selections++;                             break;
    case EFF_ACTIVATE:   e->active = true;                            break;
    case EFF_DEACTIVATE: e->active = false;                           break;
    case EFF_HOLD:                                                    break;
    }
    e->actions++;

    int n = snprintf(result->data, sizeof(result->data),
                     "[effector] #%u %s via %s -> cursor=(%d,%d) selections=%d active=%s",
                     e->actions, k_effector_op_names[op], call->tool,
                     e->x, e->y, e->selections, e->active ? "yes" : "no");
    size_t len = n > 0 ? (size_t)n : 0;
    if (len >= sizeof(result->data)) len = sizeof(result->data) - 1;
    result->len = len;
    result->status = ENI_OK;
    printf("%s\n", result->data);
    return ENI_OK;
}

static void effector_print_summary(void)
{
    printf("[effector] final cursor=(%d,%d) selections=%d active=%s actions=%u\n",
           g_effector.x, g_effector.y, g_effector.selections,
           g_effector.active ? "yes" : "no", g_effector.actions);
}

static eni_status_t tool_cursor_left(const eni_tool_call_t *c, eni_tool_result_t *r)  { return effector_apply(EFF_LEFT, c, r); }
static eni_status_t tool_cursor_right(const eni_tool_call_t *c, eni_tool_result_t *r) { return effector_apply(EFF_RIGHT, c, r); }
static eni_status_t tool_cursor_up(const eni_tool_call_t *c, eni_tool_result_t *r)    { return effector_apply(EFF_UP, c, r); }
static eni_status_t tool_cursor_down(const eni_tool_call_t *c, eni_tool_result_t *r)  { return effector_apply(EFF_DOWN, c, r); }
static eni_status_t tool_select(const eni_tool_call_t *c, eni_tool_result_t *r)       { return effector_apply(EFF_SELECT, c, r); }
static eni_status_t tool_activate(const eni_tool_call_t *c, eni_tool_result_t *r)     { return effector_apply(EFF_ACTIVATE, c, r); }
static eni_status_t tool_deactivate(const eni_tool_call_t *c, eni_tool_result_t *r)   { return effector_apply(EFF_DEACTIVATE, c, r); }
static eni_status_t tool_hold(const eni_tool_call_t *c, eni_tool_result_t *r)         { return effector_apply(EFF_HOLD, c, r); }

typedef struct {
    const char      *intent;
    const char      *tool;
    const char      *description;
    eni_tool_exec_fn exec;
} intent_binding_t;

static const intent_binding_t k_bindings[] = {
    /* simulator provider intents */
    {"move_left",     "ui.cursor.left",  "Move cursor left",         tool_cursor_left},
    {"move_right",    "ui.cursor.right", "Move cursor right",        tool_cursor_right},
    {"scroll_up",     "ui.cursor.up",    "Move cursor up",           tool_cursor_up},
    {"scroll_down",   "ui.cursor.down",  "Move cursor down",         tool_cursor_down},
    {"select",        "ui.select",       "Select item under cursor", tool_select},
    {"activate",      "ui.activate",     "Activate effector",        tool_activate},
    {"deactivate",    "ui.deactivate",   "Deactivate effector",      tool_deactivate},
    /* energy / nn decoder class labels */
    {"motor_intent",  "ui.cursor.right", "Move cursor right",        tool_cursor_right},
    {"motor_execute", "ui.select",       "Select item under cursor", tool_select},
    {"attention",     "ui.activate",     "Activate effector",        tool_activate},
    {"idle",          "ui.hold",         "Hold position",            tool_hold},
};
static const size_t k_binding_count = sizeof(k_bindings) / sizeof(k_bindings[0]);

static eni_tool_entry_t make_tool_entry(const char *name, const intent_binding_t *b)
{
    eni_tool_entry_t e;
    memset(&e, 0, sizeof(e));
    snprintf(e.name, sizeof(e.name), "%s", name);
    e.description = b->description;
    e.exec = b->exec;
    return e;
}

static eni_status_t register_min_bindings(eni_min_service_t *svc)
{
    for (size_t i = 0; i < k_binding_count; i++) {
        const intent_binding_t *b = &k_bindings[i];
        eni_status_t st = eni_min_mapper_add(&svc->mapper, b->intent, b->tool);
        if (st != ENI_OK) return st;
        if (!eni_tool_find(&svc->tool_bridge.registry, b->tool)) {
            eni_tool_entry_t e = make_tool_entry(b->tool, b);
            st = eni_min_tool_bridge_register(&svc->tool_bridge, &e);
            if (st != ENI_OK) return st;
        }
    }
    return ENI_OK;
}

/* The framework service dispatches with the intent name as the tool name. */
static eni_status_t register_fw_bindings(eni_fw_service_t *svc)
{
    for (size_t i = 0; i < k_binding_count; i++) {
        eni_tool_entry_t e = make_tool_entry(k_bindings[i].intent, &k_bindings[i]);
        eni_status_t st = eni_fw_orchestrator_register_tool(&svc->orchestrator, &e);
        if (st != ENI_OK) return st;
    }
    return ENI_OK;
}

/* ── Feedback (simulated stimulator) ────────────────────────────────── */

#ifdef ENI_CLI_HAS_FEEDBACK
static const eni_stim_params_t k_feedback_pulse = {
    .type         = ENI_STIM_HAPTIC,
    .channel      = 0,
    .amplitude    = 0.5f,
    .duration_ms  = 100,
    .frequency_hz = 50.0f,
    .pattern      = 0,
};

static eni_fw_feedback_t g_fw_feedback;
static eni_stimulator_t  g_fw_stimulator;

static eni_status_t feedback_connector_send(eni_fw_connector_t *conn, const eni_event_t *ev)
{
    (void)conn;
    if (ev->type != ENI_EVENT_INTENT) return ENI_OK;
    uint64_t now_ms = ev->timestamp.sec * 1000u + ev->timestamp.nsec / 1000000u;
    eni_event_t fb_ev;
    eni_status_t st = eni_fw_feedback_evaluate(&g_fw_feedback, ev, &fb_ev, now_ms);
    /* No matching rule, or blocked by stimulation safety limits: both are expected outcomes. */
    if (st == ENI_ERR_NOT_FOUND || st == ENI_ERR_PERMISSION) return ENI_OK;
    return st;
}

static const eni_fw_connector_ops_t k_feedback_connector_ops = {
    .name = "stimulator",
    .send = feedback_connector_send,
};

static eni_status_t fw_feedback_setup(eni_fw_service_t *svc)
{
    eni_status_t st = eni_fw_feedback_init(&g_fw_feedback, &eni_stimulator_sim_ops, 1.0f, 1000);
    if (st != ENI_OK) return st;

    memset(&g_fw_stimulator, 0, sizeof(g_fw_stimulator));
    snprintf(g_fw_stimulator.name, sizeof(g_fw_stimulator.name), "%s", eni_stimulator_sim_ops.name);
    g_fw_stimulator.ops = &eni_stimulator_sim_ops;
    st = eni_stimulator_sim_ops.init(&g_fw_stimulator, NULL);
    if (st != ENI_OK) return st;

    st = eni_fw_feedback_add_output(&g_fw_feedback, &g_fw_stimulator);
    if (st != ENI_OK) return st;
    st = eni_fw_feedback_add_rule(&g_fw_feedback, "select", 0.80f, &k_feedback_pulse,
                                  ENI_FW_ADAPT_CONFIDENCE, 0.0f);
    if (st != ENI_OK) return st;
    return eni_fw_connector_manager_add(&svc->connectors, &k_feedback_connector_ops,
                                        "stimulator", NULL);
}

static eni_status_t min_feedback_setup(eni_min_service_t *svc)
{
    eni_status_t st = eni_min_feedback_init(&svc->feedback, &eni_stimulator_sim_ops, 1.0f, 1000);
    if (st != ENI_OK) return st;
    st = eni_min_feedback_add_rule(&svc->feedback, "select", 0.80f, &k_feedback_pulse);
    if (st != ENI_OK) return st;
    return eni_min_feedback_add_rule(&svc->feedback, "motor_execute", 0.80f, &k_feedback_pulse);
}
#endif

/* ── CLI ────────────────────────────────────────────────────────────── */

static void print_usage(void)
{
    printf("ENI — Neural Interface Adapter v%s\n\n", ENI_VERSION_STRING);
    printf("Usage: eni <command> [TICKS] [options]\n\n");
    printf("Commands:\n");
    printf("  run-min       Run ENI-Min (simulator provider; default %d ticks, %d with --eeg)\n",
           CLI_DEFAULT_TICKS, CLI_EEG_DEFAULT_TICKS);
    printf("  run-fw        Run ENI-Framework with simulator provider (default %d ticks)\n",
           CLI_DEFAULT_TICKS);
    printf("  info          Show platform information\n");
    printf("  version       Show version\n");
    printf("  help          Show this help\n");
    printf("\nOptions:\n");
    printf("  --eeg              run-min only: simulated EEG -> DSP -> decoder -> intents\n");
    printf("  --decoder=TYPE     run-min --eeg only: energy, nn (default: energy)\n");
    printf("  --model=PATH       Not supported: the nn decoder has no model-file loader\n");
    printf("  --feedback         Drive the simulated stimulator from 'select' intents\n");
}

typedef struct {
    int         use_eeg;
    int         use_feedback;
    const char *decoder_type;
    const char *model_path;
    int         ticks;
} cli_opts_t;

static int parse_opts(int argc, char *argv[], cli_opts_t *opts)
{
    memset(opts, 0, sizeof(*opts));
    for (int i = 2; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "--eeg") == 0) {
            opts->use_eeg = 1;
        } else if (strcmp(a, "--feedback") == 0) {
            opts->use_feedback = 1;
        } else if (strncmp(a, "--decoder=", 10) == 0) {
            opts->decoder_type = a + 10;
        } else if (strncmp(a, "--model=", 8) == 0) {
            opts->model_path = a + 8;
        } else if (a[0] == '-') {
            fprintf(stderr, "error: unknown option: %s\n", a);
            return -1;
        } else {
            char *end = NULL;
            long t = strtol(a, &end, 10);
            if (opts->ticks != 0 || *end != '\0' || t <= 0 || t > 100000000L) {
                fprintf(stderr, "error: invalid tick count: %s\n", a);
                return -1;
            }
            opts->ticks = (int)t;
        }
    }
    return 0;
}

static int reject_flag(const char *cmd, const char *flag, const char *why)
{
    fprintf(stderr, "error: %s does not support %s: %s\n", cmd, flag, why);
    return CLI_EXIT_USAGE;
}

static const char *const k_no_model_loader =
    "the nn decoder has no model-file loader (eni_decoder_nn_ops ignores model_path)";

#ifdef ENI_CLI_HAS_EEG_PIPELINE
static const eni_decoder_ops_t *lookup_decoder(const char *name)
{
    if (strcmp(name, "energy") == 0) return &eni_decoder_energy_ops;
    if (strcmp(name, "nn") == 0)     return &eni_decoder_nn_ops;
    return NULL;
}
#endif

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

static int cmd_run_min(const cli_opts_t *opts)
{
    if (opts->model_path)
        return reject_flag("run-min", "--model", k_no_model_loader);
    if (opts->decoder_type && !opts->use_eeg)
        return reject_flag("run-min", "--decoder",
                           "decoders consume raw signals and the simulator already emits intents (add --eeg)");
#ifndef ENI_CLI_HAS_FEEDBACK
    if (opts->use_feedback)
        return reject_flag("run-min", "--feedback",
                           "built without ENI_BUILD_STIMULATOR / ENI_PROVIDER_STIMULATOR_SIM");
#endif
#ifndef ENI_CLI_HAS_EEG_PIPELINE
    if (opts->use_eeg)
        return reject_flag("run-min", "--eeg",
                           "built without ENI_PROVIDER_EEG / ENI_BUILD_DSP / ENI_BUILD_DECODER");
#else
    const char *decoder_name = opts->decoder_type ? opts->decoder_type : "energy";
    const eni_decoder_ops_t *dec_ops = NULL;
    if (opts->use_eeg) {
        dec_ops = lookup_decoder(decoder_name);
        if (!dec_ops) {
            fprintf(stderr, "error: unknown decoder '%s' (expected energy or nn)\n", decoder_name);
            return CLI_EXIT_USAGE;
        }
    }
#endif

    eni_config_t cfg;
    eni_config_load_defaults(&cfg, ENI_VARIANT_MIN);

    const eni_provider_ops_t *prov_ops = &eni_provider_simulator_ops;
    const char *prov_name = "simulator";
    int ticks = opts->ticks > 0 ? opts->ticks : CLI_DEFAULT_TICKS;

#ifdef ENI_CLI_HAS_EEG_PIPELINE
    if (opts->use_eeg) {
        eni_eeg_config_t eeg_cfg;
        memset(&eeg_cfg, 0, sizeof(eeg_cfg));
        eeg_cfg.channels = 21;
        eeg_cfg.sample_rate = CLI_EEG_SAMPLE_RATE;
        if (eni_eeg_init(&eeg_cfg) != 0 || eni_eeg_connect("sim:EEG-001") != 0) {
            fprintf(stderr, "EEG init failed\n");
            return 1;
        }
        prov_ops = eni_eeg_get_provider();
        prov_name = "eeg";
        cfg.providers[0].name = prov_name;
        if (opts->ticks == 0) ticks = CLI_EEG_DEFAULT_TICKS;
    }
#endif

    eni_min_service_t svc;
    eni_status_t st = eni_min_service_init(&svc, &cfg, prov_ops);
    if (st != ENI_OK) {
        fprintf(stderr, "init failed: %s\n", eni_status_str(st));
        return 1;
    }

    effector_reset();
    st = register_min_bindings(&svc);
    if (st != ENI_OK) {
        fprintf(stderr, "tool registration failed: %s\n", eni_status_str(st));
        eni_min_service_shutdown(&svc);
        return 1;
    }

#ifdef ENI_CLI_HAS_EEG_PIPELINE
    if (opts->use_eeg) {
        st = eni_min_signal_processor_init(&svc.signal_processor, cfg.dsp.epoch_size,
                                           CLI_EEG_SAMPLE_RATE, cfg.dsp.artifact_threshold);
        if (st == ENI_OK) st = eni_min_decoder_init(&svc.decoder, dec_ops, &cfg.decoder);
        if (st != ENI_OK) {
            fprintf(stderr, "decoder pipeline init failed: %s\n", eni_status_str(st));
            eni_min_service_shutdown(&svc);
            return 1;
        }
    }
#endif
#ifdef ENI_CLI_HAS_FEEDBACK
    if (opts->use_feedback) {
        st = min_feedback_setup(&svc);
        if (st != ENI_OK) {
            fprintf(stderr, "feedback init failed: %s\n", eni_status_str(st));
            eni_min_service_shutdown(&svc);
            return 1;
        }
    }
#endif

    st = eni_min_service_start(&svc);
    if (st != ENI_OK) {
        fprintf(stderr, "start failed: %s\n", eni_status_str(st));
        eni_min_service_shutdown(&svc);
        return 1;
    }

    printf("ENI-Min running (%s, %d ticks)...\n", prov_name, ticks);
    for (int i = 0; i < ticks; i++) {
        eni_min_service_tick(&svc);
    }

    eni_min_service_stats(&svc);
    effector_print_summary();
#ifdef ENI_CLI_HAS_EEG_PIPELINE
    if (opts->use_eeg) {
        printf("[decoder] type=%s decoded=%llu\n", decoder_name,
               (unsigned long long)svc.events_decoded);
    }
#endif
#ifdef ENI_CLI_HAS_FEEDBACK
    if (opts->use_feedback) {
        printf("[feedback] stimulations=%llu\n", (unsigned long long)svc.events_stimulated);
    }
#endif

    eni_min_service_shutdown(&svc);
#ifdef ENI_CLI_HAS_EEG_PIPELINE
    if (opts->use_eeg) eni_min_decoder_shutdown(&svc.decoder);
#endif
#ifdef ENI_CLI_HAS_FEEDBACK
    if (opts->use_feedback) eni_min_feedback_shutdown(&svc.feedback);
#endif
    return 0;
}

static int cmd_run_fw(const cli_opts_t *opts)
{
    if (opts->use_eeg)
        return reject_flag("run-fw", "--eeg",
                           "the framework service has no DSP/decoder stage (use run-min --eeg)");
    if (opts->decoder_type)
        return reject_flag("run-fw", "--decoder",
                           "the framework service has no decoder stage (use run-min --eeg --decoder=TYPE)");
    if (opts->model_path)
        return reject_flag("run-fw", "--model", k_no_model_loader);
#ifndef ENI_CLI_HAS_FEEDBACK
    if (opts->use_feedback)
        return reject_flag("run-fw", "--feedback",
                           "built without ENI_BUILD_STIMULATOR / ENI_PROVIDER_STIMULATOR_SIM");
#endif

    int ticks = opts->ticks > 0 ? opts->ticks : CLI_DEFAULT_TICKS;
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
        eni_fw_service_shutdown(&svc);
        return 1;
    }

    effector_reset();
    st = register_fw_bindings(&svc);
    if (st != ENI_OK) {
        fprintf(stderr, "tool registration failed: %s\n", eni_status_str(st));
        eni_fw_service_shutdown(&svc);
        return 1;
    }

#ifdef ENI_CLI_HAS_FEEDBACK
    if (opts->use_feedback) {
        st = fw_feedback_setup(&svc);
        if (st != ENI_OK) {
            fprintf(stderr, "feedback init failed: %s\n", eni_status_str(st));
            eni_fw_service_shutdown(&svc);
            return 1;
        }
    }
#endif

    st = eni_fw_service_start(&svc);
    if (st != ENI_OK) {
        fprintf(stderr, "start failed: %s\n", eni_status_str(st));
        eni_fw_service_shutdown(&svc);
        return 1;
    }

    printf("ENI-Framework running (simulator, %d ticks)...\n", ticks);
    for (int i = 0; i < ticks; i++) {
        eni_fw_service_tick(&svc);
    }

    eni_fw_service_stats(&svc);
    effector_print_summary();
#ifdef ENI_CLI_HAS_FEEDBACK
    if (opts->use_feedback) {
        printf("[feedback] evaluations=%llu delivered=%llu blocked=%llu\n",
               (unsigned long long)g_fw_feedback.evaluations,
               (unsigned long long)g_fw_feedback.stimulations_delivered,
               (unsigned long long)g_fw_feedback.stimulations_blocked);
    }
#endif

    eni_fw_service_shutdown(&svc);
#ifdef ENI_CLI_HAS_FEEDBACK
    if (opts->use_feedback) {
        eni_fw_feedback_shutdown(&g_fw_feedback);
        eni_stimulator_sim_ops.shutdown(&g_fw_stimulator);
    }
#endif
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

    if (strcmp(cmd, "run-min") != 0 && strcmp(cmd, "run-fw") != 0) {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        print_usage();
        return 1;
    }

    cli_opts_t opts;
    if (parse_opts(argc, argv, &opts) != 0) return CLI_EXIT_USAGE;

    if (strcmp(cmd, "run-min") == 0) {
        return cmd_run_min(&opts);
    }
    return cmd_run_fw(&opts);
}
