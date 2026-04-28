// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// XDF (Extensible Data Format) reader/writer

#ifndef ENI_XDF_H
#define ENI_XDF_H

#include "eni/data_format.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ENI_XDF_MAX_STREAMS 32

typedef enum {
    ENI_XDF_CHUNK_FILEHEADER = 1,
    ENI_XDF_CHUNK_STREAMHEADER = 2,
    ENI_XDF_CHUNK_SAMPLES = 3,
    ENI_XDF_CHUNK_CLOCKOFFSET = 4,
    ENI_XDF_CHUNK_STREAMFOOTER = 6,
} eni_xdf_chunk_type_t;

typedef struct {
    int    stream_id;
    char   name[80];
    char   type[32];
    int    channel_count;
    double nominal_srate;
    char   channel_format[16]; /* "float32", "double64", "int16", etc. */
    double clock_offsets[64];
    int    clock_offset_count;
    int64_t sample_count;
} eni_xdf_stream_t;

typedef struct {
    eni_xdf_stream_t streams[ENI_XDF_MAX_STREAMS];
    int              stream_count;
    FILE            *fp;
    bool             writing;
    char             file_header[1024];
} eni_xdf_file_t;

/* Reading */
eni_status_t eni_xdf_open(eni_xdf_file_t *xdf, const char *path);
eni_status_t eni_xdf_read_samples(eni_xdf_file_t *xdf, int stream_id,
                                   double *samples, int max_samples,
                                   int *samples_read);
void         eni_xdf_close(eni_xdf_file_t *xdf);

/* Writing */
eni_status_t eni_xdf_create(eni_xdf_file_t *xdf, const char *path);
eni_status_t eni_xdf_add_stream(eni_xdf_file_t *xdf, const eni_xdf_stream_t *stream);
eni_status_t eni_xdf_write_samples(eni_xdf_file_t *xdf, int stream_id,
                                    const double *samples, int num_samples);
eni_status_t eni_xdf_write_clock_offset(eni_xdf_file_t *xdf, int stream_id, double offset);
eni_status_t eni_xdf_finalize(eni_xdf_file_t *xdf);

#ifdef __cplusplus
}
#endif

#endif /* ENI_XDF_H */
