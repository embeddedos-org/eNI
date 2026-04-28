// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// BDF+ reader/writer — 24-bit BioSemi variant

#include "eni/bdf.h"
#include "eni/log.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static void bdf_read_fixed(FILE *fp, char *buf, int len)
{
    size_t n = fread(buf, 1, (size_t)len, fp);
    int end = (int)(n < (size_t)len ? n : (size_t)(len - 1));
    buf[end] = '\0';
    for (int i = end - 1; i >= 0 && buf[i] == ' '; i--) buf[i] = '\0';
}

static void bdf_write_fixed(FILE *fp, const char *str, int len)
{
    int slen = (int)strlen(str);
    if (slen > len) slen = len;
    fwrite(str, 1, (size_t)slen, fp);
    for (int i = slen; i < len; i++) fputc(' ', fp);
}

static void bdf_write_fixed_int(FILE *fp, int val, int len)
{
    char buf[32]; snprintf(buf, sizeof(buf), "%d", val);
    bdf_write_fixed(fp, buf, len);
}

static void bdf_write_fixed_double(FILE *fp, double val, int len)
{
    char buf[32]; snprintf(buf, sizeof(buf), "%.6g", val);
    bdf_write_fixed(fp, buf, len);
}

double eni_bdf_digital_to_physical(const eni_channel_info_t *ch, int32_t digital)
{
    if (!ch) return 0.0;
    double gain = (ch->physical_max - ch->physical_min) /
                  (double)(ch->digital_max - ch->digital_min);
    return ch->physical_min + gain * (digital - ch->digital_min);
}

int32_t eni_bdf_physical_to_digital(const eni_channel_info_t *ch, double physical)
{
    if (!ch) return 0;
    double gain = (double)(ch->digital_max - ch->digital_min) /
                  (ch->physical_max - ch->physical_min);
    double d = ch->digital_min + gain * (physical - ch->physical_min);
    if (d < -8388608.0) d = -8388608.0;
    if (d > 8388607.0) d = 8388607.0;
    return (int32_t)round(d);
}

eni_status_t eni_bdf_open(eni_bdf_file_t *bdf, const char *path)
{
    if (!bdf || !path) return ENI_ERR_INVALID;
    memset(bdf, 0, sizeof(*bdf));

    bdf->fp = fopen(path, "rb");
    if (!bdf->fp) return ENI_ERR_IO;

    bdf->writing = false;
    bdf->header.format = ENI_FORMAT_BDF;

    char buf[81];
    /* BDF: 0xFF + "BIOSEMI" (8 bytes) */
    fread(buf, 1, 8, bdf->fp);
    bdf_read_fixed(bdf->fp, bdf->header.patient, 80);
    bdf_read_fixed(bdf->fp, bdf->header.recording, 80);
    bdf_read_fixed(bdf->fp, bdf->header.start_date, 8);
    bdf_read_fixed(bdf->fp, bdf->header.start_time, 8);

    bdf_read_fixed(bdf->fp, buf, 8);
    int header_bytes = atoi(buf);

    bdf_read_fixed(bdf->fp, buf, 44); /* reserved */
    bdf_read_fixed(bdf->fp, buf, 8);
    bdf->header.num_records = atoll(buf);

    bdf_read_fixed(bdf->fp, buf, 8);
    bdf->header.record_duration = atof(buf);

    bdf_read_fixed(bdf->fp, buf, 4);
    bdf->header.num_channels = atoi(buf);

    if (bdf->header.num_channels <= 0 || bdf->header.num_channels > ENI_DATA_MAX_CHANNELS) {
        fclose(bdf->fp); return ENI_ERR_CONFIG;
    }

    int ns = bdf->header.num_channels;

    for (int i = 0; i < ns; i++) bdf_read_fixed(bdf->fp, bdf->header.channels[i].label, 16);
    for (int i = 0; i < ns; i++) bdf_read_fixed(bdf->fp, bdf->header.channels[i].transducer, 80);
    for (int i = 0; i < ns; i++) bdf_read_fixed(bdf->fp, bdf->header.channels[i].physical_dim, 8);
    for (int i = 0; i < ns; i++) { bdf_read_fixed(bdf->fp, buf, 8); bdf->header.channels[i].physical_min = atof(buf); }
    for (int i = 0; i < ns; i++) { bdf_read_fixed(bdf->fp, buf, 8); bdf->header.channels[i].physical_max = atof(buf); }
    for (int i = 0; i < ns; i++) { bdf_read_fixed(bdf->fp, buf, 8); bdf->header.channels[i].digital_min = atoi(buf); }
    for (int i = 0; i < ns; i++) { bdf_read_fixed(bdf->fp, buf, 8); bdf->header.channels[i].digital_max = atoi(buf); }
    for (int i = 0; i < ns; i++) bdf_read_fixed(bdf->fp, buf, 80);
    for (int i = 0; i < ns; i++) {
        bdf_read_fixed(bdf->fp, buf, 8);
        bdf->header.channels[i].samples_per_record = atoi(buf);
        if (bdf->header.record_duration > 0.0)
            bdf->header.channels[i].sample_rate =
                bdf->header.channels[i].samples_per_record / bdf->header.record_duration;
    }
    for (int i = 0; i < ns; i++) bdf_read_fixed(bdf->fp, buf, 32);

    bdf->record_size = 0;
    for (int i = 0; i < ns; i++)
        bdf->record_size += bdf->header.channels[i].samples_per_record * 3; /* 24-bit */

    bdf->data_offset = header_bytes;
    return ENI_OK;
}

eni_status_t eni_bdf_read_record(eni_bdf_file_t *bdf, int64_t record_idx,
                                  double *samples, int max_samples)
{
    if (!bdf || !bdf->fp || bdf->writing || !samples) return ENI_ERR_INVALID;
    if (record_idx < 0 || record_idx >= bdf->header.num_records) return ENI_ERR_INVALID;

    int64_t offset = bdf->data_offset + record_idx * bdf->record_size;
    if (fseek(bdf->fp, (long)offset, SEEK_SET) != 0) return ENI_ERR_IO;

    int sample_idx = 0;
    for (int ch = 0; ch < bdf->header.num_channels; ch++) {
        int spr = bdf->header.channels[ch].samples_per_record;
        for (int s = 0; s < spr && sample_idx < max_samples; s++) {
            uint8_t b[3];
            if (fread(b, 1, 3, bdf->fp) != 3) return ENI_ERR_IO;
            int32_t digital = (int32_t)(b[0] | (b[1] << 8) | (b[2] << 16));
            if (digital & 0x800000) digital |= 0xFF000000; /* sign extend */
            samples[sample_idx++] = eni_bdf_digital_to_physical(&bdf->header.channels[ch], digital);
        }
    }
    return ENI_OK;
}

void eni_bdf_close(eni_bdf_file_t *bdf)
{
    if (bdf && bdf->fp) { fclose(bdf->fp); bdf->fp = NULL; }
}

eni_status_t eni_bdf_create(eni_bdf_file_t *bdf, const char *path,
                             const eni_data_header_t *header)
{
    if (!bdf || !path || !header) return ENI_ERR_INVALID;
    memset(bdf, 0, sizeof(*bdf));
    bdf->header = *header;
    bdf->header.format = ENI_FORMAT_BDF;
    bdf->writing = true;

    bdf->fp = fopen(path, "wb");
    if (!bdf->fp) return ENI_ERR_IO;

    int ns = header->num_channels;

    /* BDF magic: 0xFF + "BIOSEMI" */
    fputc(0xFF, bdf->fp);
    bdf_write_fixed(bdf->fp, "BIOSEMI", 7);
    bdf_write_fixed(bdf->fp, header->patient, 80);
    bdf_write_fixed(bdf->fp, header->recording, 80);
    bdf_write_fixed(bdf->fp, header->start_date[0] ? header->start_date : "01.01.26", 8);
    bdf_write_fixed(bdf->fp, header->start_time[0] ? header->start_time : "00.00.00", 8);

    int header_size = 256 + ns * 256;
    bdf_write_fixed_int(bdf->fp, header_size, 8);
    bdf_write_fixed(bdf->fp, "BDF+C", 44);
    bdf_write_fixed(bdf->fp, "-1", 8);
    bdf_write_fixed_double(bdf->fp, header->record_duration > 0.0 ? header->record_duration : 1.0, 8);
    bdf_write_fixed_int(bdf->fp, ns, 4);

    for (int i = 0; i < ns; i++) bdf_write_fixed(bdf->fp, header->channels[i].label, 16);
    for (int i = 0; i < ns; i++) bdf_write_fixed(bdf->fp, header->channels[i].transducer, 80);
    for (int i = 0; i < ns; i++) bdf_write_fixed(bdf->fp, header->channels[i].physical_dim, 8);
    for (int i = 0; i < ns; i++) bdf_write_fixed_double(bdf->fp, header->channels[i].physical_min, 8);
    for (int i = 0; i < ns; i++) bdf_write_fixed_double(bdf->fp, header->channels[i].physical_max, 8);
    for (int i = 0; i < ns; i++) bdf_write_fixed_int(bdf->fp, header->channels[i].digital_min, 8);
    for (int i = 0; i < ns; i++) bdf_write_fixed_int(bdf->fp, header->channels[i].digital_max, 8);
    for (int i = 0; i < ns; i++) bdf_write_fixed(bdf->fp, "", 80);
    for (int i = 0; i < ns; i++) bdf_write_fixed_int(bdf->fp, header->channels[i].samples_per_record, 8);
    for (int i = 0; i < ns; i++) bdf_write_fixed(bdf->fp, "", 32);

    bdf->data_offset = header_size;
    bdf->record_size = 0;
    for (int i = 0; i < ns; i++) bdf->record_size += header->channels[i].samples_per_record * 3;

    return ENI_OK;
}

eni_status_t eni_bdf_write_record(eni_bdf_file_t *bdf, const double *samples, int num_samples)
{
    if (!bdf || !bdf->fp || !bdf->writing || !samples) return ENI_ERR_INVALID;

    int sample_idx = 0;
    for (int ch = 0; ch < bdf->header.num_channels; ch++) {
        int spr = bdf->header.channels[ch].samples_per_record;
        for (int s = 0; s < spr && sample_idx < num_samples; s++) {
            int32_t digital = eni_bdf_physical_to_digital(&bdf->header.channels[ch], samples[sample_idx++]);
            uint8_t b[3] = {
                (uint8_t)(digital & 0xFF),
                (uint8_t)((digital >> 8) & 0xFF),
                (uint8_t)((digital >> 16) & 0xFF)
            };
            fwrite(b, 1, 3, bdf->fp);
        }
    }
    bdf->header.num_records++;
    return ENI_OK;
}

eni_status_t eni_bdf_finalize(eni_bdf_file_t *bdf)
{
    if (!bdf || !bdf->fp || !bdf->writing) return ENI_ERR_INVALID;
    fseek(bdf->fp, 236, SEEK_SET);
    bdf_write_fixed_int(bdf->fp, (int)bdf->header.num_records, 8);
    fclose(bdf->fp);
    bdf->fp = NULL;
    bdf->writing = false;
    return ENI_OK;
}
