// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// eNI native binary format — optimized for real-time streaming

#ifndef ENI_FORMAT_H
#define ENI_FORMAT_H

#include "eni/data_format.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ENI format: "ENI1" magic, little-endian, chunk-based
 * Header: magic(4) + version(2) + flags(2) + num_channels(4) + sample_rate(4) + reserved(16) = 32 bytes
 * Data chunks: tag(2) + length(4) + payload
 */

#define ENI_FMT_MAGIC       "ENI1"
#define ENI_FMT_VERSION     1
#define ENI_FMT_FLAG_COMPRESSED 0x0001

#define ENI_FMT_CHUNK_HEADER   0x0001
#define ENI_FMT_CHUNK_CHANNEL  0x0002
#define ENI_FMT_CHUNK_DATA     0x0003
#define ENI_FMT_CHUNK_EVENT    0x0004
#define ENI_FMT_CHUNK_META     0x0005

typedef struct {
    eni_data_header_t header;
    FILE             *fp;
    bool              writing;
    uint16_t          flags;
} eni_native_file_t;

eni_status_t eni_native_open(eni_native_file_t *nf, const char *path);
eni_status_t eni_native_read_samples(eni_native_file_t *nf, double *samples,
                                      int max_samples, int *samples_read);
void         eni_native_close(eni_native_file_t *nf);

eni_status_t eni_native_create(eni_native_file_t *nf, const char *path,
                                const eni_data_header_t *header, uint16_t flags);
eni_status_t eni_native_write_samples(eni_native_file_t *nf, const double *samples,
                                       int num_samples);
eni_status_t eni_native_write_event(eni_native_file_t *nf, double timestamp,
                                     const char *text);
eni_status_t eni_native_finalize(eni_native_file_t *nf);

#ifdef __cplusplus
}
#endif

#endif /* ENI_FORMAT_H */
