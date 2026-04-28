// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// BDF+ (BioSemi Data Format) reader/writer — 24-bit variant of EDF+

#ifndef ENI_BDF_H
#define ENI_BDF_H

#include "eni/data_format.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    eni_data_header_t header;
    FILE             *fp;
    bool              writing;
    int64_t           data_offset;
    int               record_size;
} eni_bdf_file_t;

eni_status_t eni_bdf_open(eni_bdf_file_t *bdf, const char *path);
eni_status_t eni_bdf_read_record(eni_bdf_file_t *bdf, int64_t record_idx,
                                  double *samples, int max_samples);
void         eni_bdf_close(eni_bdf_file_t *bdf);

eni_status_t eni_bdf_create(eni_bdf_file_t *bdf, const char *path,
                             const eni_data_header_t *header);
eni_status_t eni_bdf_write_record(eni_bdf_file_t *bdf, const double *samples,
                                   int num_samples);
eni_status_t eni_bdf_finalize(eni_bdf_file_t *bdf);

double  eni_bdf_digital_to_physical(const eni_channel_info_t *ch, int32_t digital);
int32_t eni_bdf_physical_to_digital(const eni_channel_info_t *ch, double physical);

#ifdef __cplusplus
}
#endif

#endif /* ENI_BDF_H */
