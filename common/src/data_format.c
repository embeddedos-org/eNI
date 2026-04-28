// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

#include "eni/data_format.h"
#include <string.h>
#include <stdio.h>

eni_data_format_t eni_data_format_detect(const uint8_t *data, size_t len)
{
    if (!data || len < 4) return ENI_FORMAT_UNKNOWN;

    /* EDF: first byte is 0x30 ('0') — "0       " version field */
    if (data[0] == '0' && len >= 256) return ENI_FORMAT_EDF;

    /* BDF: first byte is 0xFF, followed by "BIOSEMI" */
    if (data[0] == 0xFF && len >= 8 && memcmp(data + 1, "BIOSEMI", 7) == 0)
        return ENI_FORMAT_BDF;

    /* XDF: "XDF:" magic */
    if (len >= 4 && memcmp(data, "XDF:", 4) == 0) return ENI_FORMAT_XDF;

    /* ENI native: "ENI1" magic */
    if (len >= 4 && memcmp(data, "ENI1", 4) == 0) return ENI_FORMAT_ENI;

    return ENI_FORMAT_UNKNOWN;
}

eni_data_format_t eni_data_format_detect_file(const char *path)
{
    if (!path) return ENI_FORMAT_UNKNOWN;

    FILE *fp = fopen(path, "rb");
    if (!fp) return ENI_FORMAT_UNKNOWN;

    uint8_t header[256];
    size_t nread = fread(header, 1, sizeof(header), fp);
    fclose(fp);

    return eni_data_format_detect(header, nread);
}

const char *eni_data_format_name(eni_data_format_t fmt)
{
    switch (fmt) {
    case ENI_FORMAT_EDF: return "EDF+";
    case ENI_FORMAT_BDF: return "BDF+";
    case ENI_FORMAT_XDF: return "XDF";
    case ENI_FORMAT_ENI: return "ENI";
    default:             return "unknown";
    }
}

void eni_data_header_init(eni_data_header_t *hdr)
{
    if (hdr) memset(hdr, 0, sizeof(*hdr));
}
