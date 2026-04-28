// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

#include "eni/annotation.h"
#include <string.h>
#include <stdlib.h>

void eni_annotation_list_init(eni_annotation_list_t *list)
{
    if (!list) return;
    memset(list, 0, sizeof(*list));
    list->next_id = 1;
}

eni_status_t eni_annotation_add_marker(eni_annotation_list_t *list,
                                        double onset, const char *text)
{
    if (!list || !text) return ENI_ERR_INVALID;
    if (list->count >= ENI_ANNOT_MAX) return ENI_ERR_OVERFLOW;

    eni_annotation_t *a = &list->entries[list->count];
    memset(a, 0, sizeof(*a));
    a->type = ENI_ANNOT_MARKER;
    a->onset = onset;
    a->duration = 0.0;
    a->id = list->next_id++;
    strncpy(a->text, text, ENI_ANNOT_MAX_TEXT - 1);
    list->count++;
    return ENI_OK;
}

eni_status_t eni_annotation_add_epoch(eni_annotation_list_t *list,
                                       double onset, double duration, const char *text)
{
    if (!list || !text || duration < 0.0) return ENI_ERR_INVALID;
    if (list->count >= ENI_ANNOT_MAX) return ENI_ERR_OVERFLOW;

    eni_annotation_t *a = &list->entries[list->count];
    memset(a, 0, sizeof(*a));
    a->type = ENI_ANNOT_EPOCH;
    a->onset = onset;
    a->duration = duration;
    a->id = list->next_id++;
    strncpy(a->text, text, ENI_ANNOT_MAX_TEXT - 1);
    list->count++;
    return ENI_OK;
}

eni_status_t eni_annotation_add_tag(eni_annotation_list_t *list, uint32_t id, const char *tag)
{
    if (!list || !tag) return ENI_ERR_INVALID;

    for (int i = 0; i < list->count; i++) {
        if (list->entries[i].id == id) {
            eni_annotation_t *a = &list->entries[i];
            if (a->tag_count >= ENI_ANNOT_MAX_TAGS) return ENI_ERR_OVERFLOW;
            strncpy(a->tags[a->tag_count], tag, 31);
            a->tag_count++;
            return ENI_OK;
        }
    }
    return ENI_ERR_NOT_FOUND;
}

eni_status_t eni_annotation_remove(eni_annotation_list_t *list, uint32_t id)
{
    if (!list) return ENI_ERR_INVALID;
    for (int i = 0; i < list->count; i++) {
        if (list->entries[i].id == id) {
            for (int j = i; j < list->count - 1; j++)
                list->entries[j] = list->entries[j + 1];
            list->count--;
            return ENI_OK;
        }
    }
    return ENI_ERR_NOT_FOUND;
}

static int annot_cmp_onset(const void *a, const void *b)
{
    const eni_annotation_t *aa = (const eni_annotation_t *)a;
    const eni_annotation_t *bb = (const eni_annotation_t *)b;
    if (aa->onset < bb->onset) return -1;
    if (aa->onset > bb->onset) return 1;
    return 0;
}

void eni_annotation_sort_by_onset(eni_annotation_list_t *list)
{
    if (!list || list->count <= 1) return;
    qsort(list->entries, (size_t)list->count, sizeof(eni_annotation_t), annot_cmp_onset);
}

int eni_annotation_find_in_range(const eni_annotation_list_t *list,
                                  double start, double end,
                                  const eni_annotation_t **results, int max_results)
{
    if (!list || !results) return 0;
    int found = 0;
    for (int i = 0; i < list->count && found < max_results; i++) {
        double onset = list->entries[i].onset;
        double offset = onset + list->entries[i].duration;
        if (onset <= end && offset >= start) {
            results[found++] = &list->entries[i];
        }
    }
    return found;
}

eni_status_t eni_annotation_merge(eni_annotation_list_t *dst,
                                   const eni_annotation_list_t *src)
{
    if (!dst || !src) return ENI_ERR_INVALID;
    for (int i = 0; i < src->count; i++) {
        if (dst->count >= ENI_ANNOT_MAX) return ENI_ERR_OVERFLOW;
        dst->entries[dst->count] = src->entries[i];
        dst->entries[dst->count].id = dst->next_id++;
        dst->count++;
    }
    return ENI_OK;
}
