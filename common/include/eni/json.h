// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Minimal recursive descent JSON parser — no external dependencies

#ifndef ENI_JSON_H
#define ENI_JSON_H

#include "eni/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ENI_JSON_MAX_DEPTH    32
#define ENI_JSON_MAX_STRING   256
#define ENI_JSON_MAX_CHILDREN 64

typedef enum {
    ENI_JSON_NULL,
    ENI_JSON_BOOL,
    ENI_JSON_NUMBER,
    ENI_JSON_STRING,
    ENI_JSON_ARRAY,
    ENI_JSON_OBJECT,
} eni_json_type_t;

typedef struct eni_json_value_s eni_json_value_t;

struct eni_json_value_s {
    eni_json_type_t type;
    char            key[ENI_JSON_MAX_STRING];

    union {
        bool   boolean;
        double number;
        char   string[ENI_JSON_MAX_STRING];
        struct {
            eni_json_value_t *children;
            int               count;
        } container;
    } data;
};

typedef struct {
    const char *src;
    int         pos;
    int         len;
    int         depth;
    char        error[128];
} eni_json_parser_t;

/* Parse a JSON string into a value tree. Caller must call eni_json_free() when done. */
eni_status_t eni_json_parse(const char *json_str, eni_json_value_t *root);

/* Free allocated children (recursive) */
void eni_json_free(eni_json_value_t *val);

/* Lookup helpers for objects */
const eni_json_value_t *eni_json_get(const eni_json_value_t *obj, const char *key);
const char             *eni_json_get_string(const eni_json_value_t *obj, const char *key, const char *def);
double                  eni_json_get_number(const eni_json_value_t *obj, const char *key, double def);
bool                    eni_json_get_bool(const eni_json_value_t *obj, const char *key, bool def);

#ifdef __cplusplus
}
#endif

#endif /* ENI_JSON_H */
