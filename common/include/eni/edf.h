// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// EDF+ (European Data Format) reader/writer per specification

#ifndef ENI_EDF_H
#define ENI_EDF_H

#include "eni/data_format.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ENI_EDF_HEADER_SIZE      256
#define ENI_EDF_SIGNAL_HDR_SIZE  256
#define ENI_EDF_MAX_ANNOTATIONS  1024

typedef struct {
    double onset;
    double duration;
    char   text[256];
} eni_edf_annotation_t;

typedef struct {
    eni_data_header_t     header;
    eni_edf_annotation_t  annotations[ENI_EDF_MAX_ANNOTATIONS];
    int                   annotation_count;
    /* Annotations present in the file that eni_edf_read_annotations() could
     * not store: text that does not fit eni_edf_annotation_t::text, or any
     * annotation past ENI_EDF_MAX_ANNOTATIONS. Zero means the read was
     * complete. See eni_edf_read_annotations(). */
    int                   dropped_count;
    FILE                 *fp;
    bool                  writing;
    int64_t               data_offset;
    int                   record_size; /* bytes per data record */
} eni_edf_file_t;

/* Reading */
eni_status_t eni_edf_open(eni_edf_file_t *edf, const char *path);
eni_status_t eni_edf_read_record(eni_edf_file_t *edf, int64_t record_idx,
                                  double *samples, int max_samples);
/* Parse every annotation in the file into edf->annotations.
 *
 * Sets edf->annotation_count to the number stored and edf->dropped_count to
 * the number found but not stored: an annotation whose text needs
 * sizeof(eni_edf_annotation_t::text) bytes or more, and every annotation
 * past ENI_EDF_MAX_ANNOTATIONS. The whole file is scanned either way, so
 * dropped_count is the true number lost, not the number seen before the
 * parser gave up.
 *
 * Returns ENI_OK when every annotation was stored, and ENI_ERR_OVERFLOW when
 * dropped_count > 0 — the stored annotations are still valid and complete up
 * to annotation_count, but the list is not the whole file. ENI_OK with a
 * short list therefore means a short file, not a truncated read. */
eni_status_t eni_edf_read_annotations(eni_edf_file_t *edf);
void         eni_edf_close(eni_edf_file_t *edf);

/* Writing */
eni_status_t eni_edf_create(eni_edf_file_t *edf, const char *path,
                             const eni_data_header_t *header);
eni_status_t eni_edf_write_record(eni_edf_file_t *edf, const double *samples,
                                   int num_samples);
eni_status_t eni_edf_write_annotation(eni_edf_file_t *edf, double onset,
                                       double duration, const char *text);
eni_status_t eni_edf_finalize(eni_edf_file_t *edf);

/* Utility */
double eni_edf_digital_to_physical(const eni_channel_info_t *ch, int16_t digital);
int16_t eni_edf_physical_to_digital(const eni_channel_info_t *ch, double physical);

#ifdef __cplusplus
}
#endif

#endif /* ENI_EDF_H */
