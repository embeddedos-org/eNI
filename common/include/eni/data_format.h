// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Data format abstraction layer

#ifndef ENI_DATA_FORMAT_H
#define ENI_DATA_FORMAT_H

#include "eni/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ENI_DATA_MAX_CHANNELS 256
#define ENI_DATA_MAX_LABEL    80

typedef enum {
    ENI_FORMAT_UNKNOWN,
    ENI_FORMAT_EDF,
    ENI_FORMAT_BDF,
    ENI_FORMAT_XDF,
    ENI_FORMAT_ENI,
} eni_data_format_t;

typedef struct {
    char    label[ENI_DATA_MAX_LABEL];
    char    transducer[ENI_DATA_MAX_LABEL];
    char    physical_dim[8];
    double  physical_min;
    double  physical_max;
    int32_t digital_min;
    int32_t digital_max;
    double  sample_rate;
    int     samples_per_record;
} eni_channel_info_t;

typedef struct {
    eni_data_format_t format;
    char              patient[80];
    char              recording[80];
    char              start_date[16];
    char              start_time[16];
    int               num_channels;
    double            record_duration;
    int64_t           num_records;
    eni_channel_info_t channels[ENI_DATA_MAX_CHANNELS];
} eni_data_header_t;

/* Auto-detect format from file magic bytes */
eni_data_format_t eni_data_format_detect(const uint8_t *data, size_t len);
eni_data_format_t eni_data_format_detect_file(const char *path);
const char       *eni_data_format_name(eni_data_format_t fmt);

void eni_data_header_init(eni_data_header_t *hdr);

#ifdef __cplusplus
}
#endif

#endif /* ENI_DATA_FORMAT_H */
