// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eni/config.h"
#include "eni/json.h"
#include "eni/log.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

eni_status_t eni_config_init(eni_config_t *cfg)
{
    if (!cfg) return ENI_ERR_INVALID;
    memset(cfg, 0, sizeof(*cfg));
    cfg->variant = ENI_VARIANT_MIN;
    cfg->mode    = ENI_MODE_INTENT;
    cfg->filter.min_confidence = 0.80f;
    cfg->filter.debounce_ms    = 100;
    return ENI_OK;
}

eni_status_t eni_config_load_defaults(eni_config_t *cfg, eni_variant_t variant)
{
    if (!cfg) return ENI_ERR_INVALID;

    eni_config_init(cfg);
    cfg->variant = variant;

    if (variant == ENI_VARIANT_MIN) {
        cfg->mode = ENI_MODE_INTENT;
        cfg->providers[0].name      = "simulator";
        cfg->providers[0].transport = ENI_TRANSPORT_UNIX_SOCKET;
        cfg->provider_count         = 1;
        cfg->filter.min_confidence  = 0.80f;
        cfg->filter.debounce_ms     = 100;
    } else {
        cfg->mode = ENI_MODE_FEATURES_INTENT;
        cfg->providers[0].name      = "generic-decoder";
        cfg->providers[0].transport = ENI_TRANSPORT_GRPC;
        cfg->provider_count         = 1;
        cfg->routing.max_latency_ms = 20;
        cfg->routing.local_only     = true;
        cfg->observability.metrics  = true;
        cfg->observability.audit    = true;
        cfg->observability.trace    = false;
    }

    return ENI_OK;
}

static void trim_whitespace(char *s)
{
    char *start = s;
    while (*start == ' ' || *start == '\t') start++;
    if (start != s) memmove(s, start, strlen(start) + 1);
    if (*s == '\0') return;
    char *end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r'))
        end--;
    *(end + 1) = '\0';
}

static void config_apply_kv(eni_config_t *cfg, const char *key, const char *value)
{
    if (strcmp(key, "variant") == 0) {
        if (strcmp(value, "min") == 0)
            cfg->variant = ENI_VARIANT_MIN;
        else if (strcmp(value, "framework") == 0)
            cfg->variant = ENI_VARIANT_FRAMEWORK;
    } else if (strcmp(key, "mode") == 0) {
        if (strcmp(value, "intent") == 0)
            cfg->mode = ENI_MODE_INTENT;
        else if (strcmp(value, "features") == 0)
            cfg->mode = ENI_MODE_FEATURES;
        else if (strcmp(value, "raw") == 0)
            cfg->mode = ENI_MODE_RAW;
        else if (strcmp(value, "features_intent") == 0)
            cfg->mode = ENI_MODE_FEATURES_INTENT;
    } else if (strcmp(key, "confidence_threshold") == 0) {
        cfg->filter.min_confidence = (float)strtod(value, NULL);
    } else if (strcmp(key, "debounce_ms") == 0) {
        cfg->filter.debounce_ms = (uint32_t)strtol(value, NULL, 10);
    } else if (strcmp(key, "max_providers") == 0) {
        int n = (int)strtol(value, NULL, 10);
        cfg->provider_count = (n > 0 && n <= ENI_CONFIG_MAX_PROVIDERS) ? n : 0;
    } else if (strcmp(key, "default_deny") == 0) {
        cfg->policy.require_confirmation = (strcmp(value, "true") == 0);
    } else if (strcmp(key, "epoch_size") == 0) {
        cfg->dsp.epoch_size = (uint32_t)strtol(value, NULL, 10);
    } else if (strcmp(key, "sample_rate") == 0) {
        cfg->dsp.sample_rate = (uint32_t)strtol(value, NULL, 10);
    } else if (strcmp(key, "artifact_threshold") == 0) {
        cfg->dsp.artifact_threshold = (float)strtod(value, NULL);
    } else if (strcmp(key, "model_path") == 0) {
        strncpy(cfg->decoder.model_path, value, sizeof(cfg->decoder.model_path) - 1);
    } else if (strcmp(key, "num_classes") == 0) {
        cfg->decoder.num_classes = (int)strtol(value, NULL, 10);
    }
}

static bool has_json_extension(const char *path)
{
    size_t len = strlen(path);
    return (len >= 5 && strcmp(path + len - 5, ".json") == 0);
}

static eni_status_t config_load_json(eni_config_t *cfg, const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        ENI_LOG_WARN("config", "cannot open JSON config: %s", path);
        return ENI_ERR_IO;
    }

    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (fsize <= 0 || fsize > 1024 * 1024) {
        fclose(fp);
        return ENI_ERR_IO;
    }

    char *buf = (char *)malloc((size_t)fsize + 1);
    if (!buf) { fclose(fp); return ENI_ERR_NOMEM; }

    size_t nread = fread(buf, 1, (size_t)fsize, fp);
    buf[nread] = '\0';
    fclose(fp);

    eni_json_value_t root;
    eni_status_t rc = eni_json_parse(buf, &root);
    free(buf);

    if (rc != ENI_OK) {
        eni_json_free(&root); /* Clean up any partial allocations */
        ENI_LOG_WARN("config", "JSON parse error in %s", path);
        return rc;
    }

    if (root.type != ENI_JSON_OBJECT) {
        eni_json_free(&root);
        return ENI_ERR_CONFIG;
    }

    /* Apply known keys */
    const char *s;
    s = eni_json_get_string(&root, "variant", NULL);
    if (s) config_apply_kv(cfg, "variant", s);

    s = eni_json_get_string(&root, "mode", NULL);
    if (s) config_apply_kv(cfg, "mode", s);

    double d;
    d = eni_json_get_number(&root, "confidence_threshold", -1.0);
    if (d >= 0.0) cfg->filter.min_confidence = (float)d;

    d = eni_json_get_number(&root, "debounce_ms", -1.0);
    if (d >= 0.0) cfg->filter.debounce_ms = (uint32_t)d;

    cfg->policy.require_confirmation = eni_json_get_bool(&root, "default_deny", cfg->policy.require_confirmation);
    cfg->observability.metrics = eni_json_get_bool(&root, "metrics", cfg->observability.metrics);
    cfg->observability.audit   = eni_json_get_bool(&root, "audit", cfg->observability.audit);
    cfg->observability.trace   = eni_json_get_bool(&root, "trace", cfg->observability.trace);

    /* DSP config */
    const eni_json_value_t *dsp = eni_json_get(&root, "dsp");
    if (dsp && dsp->type == ENI_JSON_OBJECT) {
        d = eni_json_get_number(dsp, "epoch_size", 0.0);
        if (d > 0.0) cfg->dsp.epoch_size = (uint32_t)d;
        d = eni_json_get_number(dsp, "sample_rate", 0.0);
        if (d > 0.0) cfg->dsp.sample_rate = (uint32_t)d;
        d = eni_json_get_number(dsp, "artifact_threshold", 0.0);
        if (d > 0.0) cfg->dsp.artifact_threshold = (float)d;
    }

    /* Decoder config */
    const eni_json_value_t *dec = eni_json_get(&root, "decoder");
    if (dec && dec->type == ENI_JSON_OBJECT) {
        s = eni_json_get_string(dec, "model_path", NULL);
        if (s) strncpy(cfg->decoder.model_path, s, sizeof(cfg->decoder.model_path) - 1);
        d = eni_json_get_number(dec, "num_classes", 0.0);
        if (d > 0.0) cfg->decoder.num_classes = (int)d;
        d = eni_json_get_number(dec, "confidence_threshold", 0.0);
        if (d > 0.0) cfg->decoder.confidence_threshold = (float)d;
    }

    eni_json_free(&root);
    ENI_LOG_INFO("config", "loaded JSON config from %s", path);
    return ENI_OK;
}

static eni_status_t config_load_kv(eni_config_t *cfg, const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp) {
        ENI_LOG_WARN("config", "cannot open config file: %s", path);
        return ENI_ERR_IO;
    }

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == ';' || *p == '\n' || *p == '\r' || *p == '\0')
            continue;

        char *eq = strchr(p, '=');
        if (!eq) continue;

        *eq = '\0';
        char *key = p;
        char *value = eq + 1;

        trim_whitespace(key);
        trim_whitespace(value);

        if (*key == '\0') continue;

        config_apply_kv(cfg, key, value);
    }

    if (fclose(fp) != 0) { ENI_LOG_WARN("config", "failed to close config file: %s", path); }
    ENI_LOG_INFO("config", "loaded config from %s", path);
    return ENI_OK;
}

eni_status_t eni_config_load_file(eni_config_t *cfg, const char *path)
{
    if (!cfg || !path) return ENI_ERR_INVALID;

    eni_config_init(cfg);

    if (has_json_extension(path)) {
        return config_load_json(cfg, path);
    }
    return config_load_kv(cfg, path);
}

void eni_config_dump(const eni_config_t *cfg)
{
    if (!cfg) return;

    const char *var_str = cfg->variant == ENI_VARIANT_MIN ? "min" : "framework";
    const char *mode_str;
    switch (cfg->mode) {
    case ENI_MODE_INTENT:          mode_str = "intent";          break;
    case ENI_MODE_FEATURES:        mode_str = "features";        break;
    case ENI_MODE_RAW:             mode_str = "raw";             break;
    case ENI_MODE_FEATURES_INTENT: mode_str = "features+intent"; break;
    default:                       mode_str = "unknown";         break;
    }

    printf("[config] variant=%s mode=%s providers=%d\n",
           var_str, mode_str, cfg->provider_count);

    for (int i = 0; i < cfg->provider_count; i++) {
        printf("  provider[%d]: %s\n", i,
               cfg->providers[i].name ? cfg->providers[i].name : "(null)");
    }

    printf("  filter: confidence=%.2f debounce=%u ms\n",
           cfg->filter.min_confidence, cfg->filter.debounce_ms);
    printf("  observability: metrics=%d audit=%d trace=%d\n",
           cfg->observability.metrics, cfg->observability.audit,
           cfg->observability.trace);
    printf("  dsp: epoch=%u rate=%u artifact=%.2f\n",
           cfg->dsp.epoch_size, cfg->dsp.sample_rate, cfg->dsp.artifact_threshold);
    printf("  decoder: classes=%d model=%s\n",
           cfg->decoder.num_classes, cfg->decoder.model_path);
}
