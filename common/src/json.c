// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Minimal recursive descent JSON parser — no external dependencies

#include "eni/json.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

static void skip_ws(eni_json_parser_t *p)
{
    while (p->pos < p->len && isspace((unsigned char)p->src[p->pos]))
        p->pos++;
}

static char peek(eni_json_parser_t *p)
{
    skip_ws(p);
    return (p->pos < p->len) ? p->src[p->pos] : '\0';
}

static char advance(eni_json_parser_t *p)
{
    return (p->pos < p->len) ? p->src[p->pos++] : '\0';
}

static bool expect(eni_json_parser_t *p, char c)
{
    skip_ws(p);
    if (p->pos < p->len && p->src[p->pos] == c) {
        p->pos++;
        return true;
    }
    return false;
}

static eni_status_t parse_value(eni_json_parser_t *p, eni_json_value_t *val);

static eni_status_t parse_string(eni_json_parser_t *p, char *out, int max_len)
{
    if (!expect(p, '"')) {
        snprintf(p->error, sizeof(p->error), "expected '\"' at pos %d", p->pos);
        return ENI_ERR_CONFIG;
    }

    int i = 0;
    while (p->pos < p->len && p->src[p->pos] != '"') {
        char c = p->src[p->pos++];
        if (c == '\\' && p->pos < p->len) {
            char esc = p->src[p->pos++];
            switch (esc) {
            case '"':  c = '"'; break;
            case '\\': c = '\\'; break;
            case '/':  c = '/'; break;
            case 'b':  c = '\b'; break;
            case 'f':  c = '\f'; break;
            case 'n':  c = '\n'; break;
            case 'r':  c = '\r'; break;
            case 't':  c = '\t'; break;
            default:   c = esc; break;
            }
        }
        if (i < max_len - 1) out[i++] = c;
    }
    out[i] = '\0';

    if (!expect(p, '"')) {
        snprintf(p->error, sizeof(p->error), "unterminated string at pos %d", p->pos);
        return ENI_ERR_CONFIG;
    }
    return ENI_OK;
}

static eni_status_t parse_number(eni_json_parser_t *p, double *out)
{
    skip_ws(p);
    char buf[64];
    int i = 0;
    while (p->pos < p->len && i < 63) {
        char c = p->src[p->pos];
        if (c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E' || isdigit((unsigned char)c)) {
            buf[i++] = c;
            p->pos++;
        } else {
            break;
        }
    }
    buf[i] = '\0';
    if (i == 0) {
        snprintf(p->error, sizeof(p->error), "expected number at pos %d", p->pos);
        return ENI_ERR_CONFIG;
    }
    *out = strtod(buf, NULL);
    return ENI_OK;
}

static eni_status_t parse_object(eni_json_parser_t *p, eni_json_value_t *val)
{
    if (!expect(p, '{')) return ENI_ERR_CONFIG;

    val->type = ENI_JSON_OBJECT;
    val->data.container.children = NULL;
    val->data.container.count = 0;

    if (peek(p) == '}') { advance(p); return ENI_OK; }

    /* Pre-allocate children array */
    eni_json_value_t *children = (eni_json_value_t *)calloc(ENI_JSON_MAX_CHILDREN, sizeof(eni_json_value_t));
    if (!children) return ENI_ERR_NOMEM;
    val->data.container.children = children;

    int count = 0;
    do {
        if (count >= ENI_JSON_MAX_CHILDREN) { return ENI_ERR_OVERFLOW; }

        eni_json_value_t *child = &children[count];
        memset(child, 0, sizeof(*child));

        eni_status_t rc = parse_string(p, child->key, ENI_JSON_MAX_STRING);
        if (rc != ENI_OK) { val->data.container.count = count; return rc; }

        if (!expect(p, ':')) {
            snprintf(p->error, sizeof(p->error), "expected ':' at pos %d", p->pos);
            val->data.container.count = count;
            return ENI_ERR_CONFIG;
        }

        rc = parse_value(p, child);
        if (rc != ENI_OK) { val->data.container.count = count; return rc; }

        count++;
    } while (expect(p, ','));

    val->data.container.count = count;

    if (!expect(p, '}')) {
        snprintf(p->error, sizeof(p->error), "expected '}' at pos %d", p->pos);
        return ENI_ERR_CONFIG;
    }
    return ENI_OK;
}

static eni_status_t parse_array(eni_json_parser_t *p, eni_json_value_t *val)
{
    if (!expect(p, '[')) return ENI_ERR_CONFIG;

    val->type = ENI_JSON_ARRAY;
    val->data.container.children = NULL;
    val->data.container.count = 0;

    if (peek(p) == ']') { advance(p); return ENI_OK; }

    eni_json_value_t *children = (eni_json_value_t *)calloc(ENI_JSON_MAX_CHILDREN, sizeof(eni_json_value_t));
    if (!children) return ENI_ERR_NOMEM;
    val->data.container.children = children;

    int count = 0;
    do {
        if (count >= ENI_JSON_MAX_CHILDREN) { val->data.container.count = count; return ENI_ERR_OVERFLOW; }

        eni_json_value_t *child = &children[count];
        memset(child, 0, sizeof(*child));

        eni_status_t rc = parse_value(p, child);
        if (rc != ENI_OK) { val->data.container.count = count; return rc; }

        count++;
    } while (expect(p, ','));

    val->data.container.count = count;

    if (!expect(p, ']')) {
        snprintf(p->error, sizeof(p->error), "expected ']' at pos %d", p->pos);
        return ENI_ERR_CONFIG;
    }
    return ENI_OK;
}

static eni_status_t parse_value(eni_json_parser_t *p, eni_json_value_t *val)
{
    if (p->depth >= ENI_JSON_MAX_DEPTH) {
        snprintf(p->error, sizeof(p->error), "max nesting depth exceeded");
        return ENI_ERR_OVERFLOW;
    }
    p->depth++;

    skip_ws(p);
    char c = peek(p);

    eni_status_t rc;
    switch (c) {
    case '"':
        val->type = ENI_JSON_STRING;
        rc = parse_string(p, val->data.string, ENI_JSON_MAX_STRING);
        break;
    case '{':
        rc = parse_object(p, val);
        break;
    case '[':
        rc = parse_array(p, val);
        break;
    case 't':
        if (p->pos + 4 <= p->len && strncmp(&p->src[p->pos], "true", 4) == 0) {
            p->pos += 4;
            val->type = ENI_JSON_BOOL;
            val->data.boolean = true;
            rc = ENI_OK;
        } else { rc = ENI_ERR_CONFIG; }
        break;
    case 'f':
        if (p->pos + 5 <= p->len && strncmp(&p->src[p->pos], "false", 5) == 0) {
            p->pos += 5;
            val->type = ENI_JSON_BOOL;
            val->data.boolean = false;
            rc = ENI_OK;
        } else { rc = ENI_ERR_CONFIG; }
        break;
    case 'n':
        if (p->pos + 4 <= p->len && strncmp(&p->src[p->pos], "null", 4) == 0) {
            p->pos += 4;
            val->type = ENI_JSON_NULL;
            rc = ENI_OK;
        } else { rc = ENI_ERR_CONFIG; }
        break;
    default:
        if (c == '-' || isdigit((unsigned char)c)) {
            val->type = ENI_JSON_NUMBER;
            rc = parse_number(p, &val->data.number);
        } else {
            snprintf(p->error, sizeof(p->error), "unexpected char '%c' at pos %d", c, p->pos);
            rc = ENI_ERR_CONFIG;
        }
        break;
    }

    p->depth--;
    return rc;
}

eni_status_t eni_json_parse(const char *json_str, eni_json_value_t *root)
{
    if (!json_str || !root) return ENI_ERR_INVALID;

    eni_json_parser_t parser = {
        .src   = json_str,
        .pos   = 0,
        .len   = (int)strlen(json_str),
        .depth = 0,
    };
    memset(parser.error, 0, sizeof(parser.error));
    memset(root, 0, sizeof(*root));

    return parse_value(&parser, root);
}

void eni_json_free(eni_json_value_t *val)
{
    if (!val) return;
    if (val->type == ENI_JSON_OBJECT || val->type == ENI_JSON_ARRAY) {
        if (val->data.container.children) {
            for (int i = 0; i < val->data.container.count; i++) {
                eni_json_free(&val->data.container.children[i]);
            }
            free(val->data.container.children);
            val->data.container.children = NULL;
        }
    }
}

const eni_json_value_t *eni_json_get(const eni_json_value_t *obj, const char *key)
{
    if (!obj || obj->type != ENI_JSON_OBJECT || !key) return NULL;
    for (int i = 0; i < obj->data.container.count; i++) {
        if (strcmp(obj->data.container.children[i].key, key) == 0)
            return &obj->data.container.children[i];
    }
    return NULL;
}

const char *eni_json_get_string(const eni_json_value_t *obj, const char *key, const char *def)
{
    const eni_json_value_t *v = eni_json_get(obj, key);
    return (v && v->type == ENI_JSON_STRING) ? v->data.string : def;
}

double eni_json_get_number(const eni_json_value_t *obj, const char *key, double def)
{
    const eni_json_value_t *v = eni_json_get(obj, key);
    return (v && v->type == ENI_JSON_NUMBER) ? v->data.number : def;
}

bool eni_json_get_bool(const eni_json_value_t *obj, const char *key, bool def)
{
    const eni_json_value_t *v = eni_json_get(obj, key);
    return (v && v->type == ENI_JSON_BOOL) ? v->data.boolean : def;
}
