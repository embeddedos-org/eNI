// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

#include "eni/eni_format.h"
#include "eni/log.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

eni_status_t eni_native_open(eni_native_file_t *nf, const char *path)
{
    if (!nf || !path) return ENI_ERR_INVALID;
    memset(nf, 0, sizeof(*nf));

    nf->fp = fopen(path, "rb");
    if (!nf->fp) return ENI_ERR_IO;

    nf->header.format = ENI_FORMAT_ENI;

    /* Read header */
    char magic[4];
    if (fread(magic, 1, 4, nf->fp) != 4 || memcmp(magic, ENI_FMT_MAGIC, 4) != 0) {
        fclose(nf->fp); return ENI_ERR_CONFIG;
    }

    uint16_t version;
    fread(&version, 2, 1, nf->fp);
    fread(&nf->flags, 2, 1, nf->fp);

    uint32_t num_channels, sample_rate;
    fread(&num_channels, 4, 1, nf->fp);
    fread(&sample_rate, 4, 1, nf->fp);

    nf->header.num_channels = (int)num_channels;
    /* Skip reserved */
    fseek(nf->fp, 16, SEEK_CUR);

    /* Read channel info chunks */
    for (int ch = 0; ch < nf->header.num_channels; ch++) {
        uint16_t tag;
        uint32_t len;
        if (fread(&tag, 2, 1, nf->fp) != 1) break;
        if (fread(&len, 4, 1, nf->fp) != 1) break;

        if (tag == ENI_FMT_CHUNK_CHANNEL && len >= 80) {
            fread(nf->header.channels[ch].label, 1, 80, nf->fp);
            nf->header.channels[ch].label[79] = '\0';
            nf->header.channels[ch].sample_rate = (double)sample_rate;
            if (len > 80) fseek(nf->fp, (long)(len - 80), SEEK_CUR);
        } else {
            fseek(nf->fp, (long)len, SEEK_CUR);
        }
    }

    return ENI_OK;
}

eni_status_t eni_native_read_samples(eni_native_file_t *nf, double *samples,
                                      int max_samples, int *samples_read)
{
    if (!nf || !nf->fp || !samples || !samples_read) return ENI_ERR_INVALID;
    *samples_read = 0;

    while (!feof(nf->fp) && *samples_read < max_samples) {
        uint16_t tag;
        uint32_t len;
        if (fread(&tag, 2, 1, nf->fp) != 1) break;
        if (fread(&len, 4, 1, nf->fp) != 1) break;

        if (tag == ENI_FMT_CHUNK_DATA) {
            int n = (int)(len / sizeof(float));
            for (int i = 0; i < n && *samples_read < max_samples; i++) {
                float val;
                if (fread(&val, sizeof(float), 1, nf->fp) != 1) break;
                samples[(*samples_read)++] = (double)val;
                len -= sizeof(float);
            }
            if (len > 0) fseek(nf->fp, (long)len, SEEK_CUR);
        } else {
            fseek(nf->fp, (long)len, SEEK_CUR);
        }
    }
    return ENI_OK;
}

void eni_native_close(eni_native_file_t *nf)
{
    if (nf && nf->fp) { fclose(nf->fp); nf->fp = NULL; }
}

eni_status_t eni_native_create(eni_native_file_t *nf, const char *path,
                                const eni_data_header_t *header, uint16_t flags)
{
    if (!nf || !path || !header) return ENI_ERR_INVALID;
    memset(nf, 0, sizeof(*nf));
    nf->header = *header;
    nf->header.format = ENI_FORMAT_ENI;
    nf->flags = flags;
    nf->writing = true;

    nf->fp = fopen(path, "wb");
    if (!nf->fp) return ENI_ERR_IO;

    /* Write header */
    fwrite(ENI_FMT_MAGIC, 1, 4, nf->fp);
    uint16_t version = ENI_FMT_VERSION;
    fwrite(&version, 2, 1, nf->fp);
    fwrite(&flags, 2, 1, nf->fp);
    uint32_t nc = (uint32_t)header->num_channels;
    fwrite(&nc, 4, 1, nf->fp);
    uint32_t sr = (header->num_channels > 0) ? (uint32_t)header->channels[0].sample_rate : 256;
    fwrite(&sr, 4, 1, nf->fp);
    uint8_t reserved[16] = {0};
    fwrite(reserved, 1, 16, nf->fp);

    /* Write channel info */
    for (int ch = 0; ch < header->num_channels; ch++) {
        uint16_t tag = ENI_FMT_CHUNK_CHANNEL;
        uint32_t len = 80;
        fwrite(&tag, 2, 1, nf->fp);
        fwrite(&len, 4, 1, nf->fp);
        char label[80];
        memset(label, 0, 80);
        strncpy(label, header->channels[ch].label, 79);
        fwrite(label, 1, 80, nf->fp);
    }

    return ENI_OK;
}

eni_status_t eni_native_write_samples(eni_native_file_t *nf, const double *samples,
                                       int num_samples)
{
    if (!nf || !nf->fp || !nf->writing || !samples) return ENI_ERR_INVALID;

    uint16_t tag = ENI_FMT_CHUNK_DATA;
    uint32_t len = (uint32_t)(num_samples * sizeof(float));
    fwrite(&tag, 2, 1, nf->fp);
    fwrite(&len, 4, 1, nf->fp);

    for (int i = 0; i < num_samples; i++) {
        float val = (float)samples[i];
        fwrite(&val, sizeof(float), 1, nf->fp);
    }
    return ENI_OK;
}

eni_status_t eni_native_write_event(eni_native_file_t *nf, double timestamp,
                                     const char *text)
{
    if (!nf || !nf->fp || !nf->writing || !text) return ENI_ERR_INVALID;

    int text_len = (int)strlen(text);
    uint16_t tag = ENI_FMT_CHUNK_EVENT;
    uint32_t len = 8 + (uint32_t)text_len;
    fwrite(&tag, 2, 1, nf->fp);
    fwrite(&len, 4, 1, nf->fp);
    fwrite(&timestamp, 8, 1, nf->fp);
    fwrite(text, 1, (size_t)text_len, nf->fp);

    return ENI_OK;
}

eni_status_t eni_native_finalize(eni_native_file_t *nf)
{
    if (!nf || !nf->fp || !nf->writing) return ENI_ERR_INVALID;
    fclose(nf->fp);
    nf->fp = NULL;
    nf->writing = false;
    return ENI_OK;
}
