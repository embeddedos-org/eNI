// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Annotation system — markers, epochs, tags

#ifndef ENI_ANNOTATION_H
#define ENI_ANNOTATION_H

#include "eni/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ENI_ANNOT_MAX_TEXT  256
#define ENI_ANNOT_MAX_TAGS 8
#define ENI_ANNOT_MAX      4096

typedef enum {
    ENI_ANNOT_MARKER,
    ENI_ANNOT_EPOCH,
} eni_annot_type_t;

typedef struct {
    eni_annot_type_t type;
    double           onset;
    double           duration;    /* 0 for markers */
    char             text[ENI_ANNOT_MAX_TEXT];
    char             tags[ENI_ANNOT_MAX_TAGS][32];
    int              tag_count;
    uint32_t         id;
} eni_annotation_t;

typedef struct {
    eni_annotation_t entries[ENI_ANNOT_MAX];
    int              count;
    uint32_t         next_id;
} eni_annotation_list_t;

void         eni_annotation_list_init(eni_annotation_list_t *list);
eni_status_t eni_annotation_add_marker(eni_annotation_list_t *list,
                                        double onset, const char *text);
eni_status_t eni_annotation_add_epoch(eni_annotation_list_t *list,
                                       double onset, double duration, const char *text);
eni_status_t eni_annotation_add_tag(eni_annotation_list_t *list, uint32_t id, const char *tag);
eni_status_t eni_annotation_remove(eni_annotation_list_t *list, uint32_t id);

void eni_annotation_sort_by_onset(eni_annotation_list_t *list);
int  eni_annotation_find_in_range(const eni_annotation_list_t *list,
                                   double start, double end,
                                   const eni_annotation_t **results, int max_results);
eni_status_t eni_annotation_merge(eni_annotation_list_t *dst,
                                   const eni_annotation_list_t *src);

#ifdef __cplusplus
}
#endif

#endif /* ENI_ANNOTATION_H */
