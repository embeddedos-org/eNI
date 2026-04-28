// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// EDF+ reader/writer per European Data Format specification

#include "eni/edf.h"
#include "eni/log.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static void read_fixed(FILE *fp, char *buf, int len)
{
    size_t n = fread(buf, 1, (size_t)len, fp);
    /* Null-terminate within bounds: write at most at buf[len-1] */
    int end = (int)(n < (size_t)len ? n : (size_t)(len - 1));
    buf[end] = '\0';
    /* Trim trailing spaces */
    for (int i = end - 1; i >= 0 && buf[i] == ' '; i--)
        buf[i] = '\0';
}

static void write_fixed(FILE *fp, const char *str, int len)
{
    int slen = (int)strlen(str);
    if (slen > len) slen = len;
    fwrite(str, 1, (size_t)slen, fp);
    for (int i = slen; i < len; i++) fputc(' ', fp);
}

static void write_fixed_int(FILE *fp, int val, int len)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", val);
    write_fixed(fp, buf, len);
}

static void write_fixed_double(FILE *fp, double val, int len)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%.6g", val);
    write_fixed(fp, buf, len);
}

double eni_edf_digital_to_physical(const eni_channel_info_t *ch, int16_t digital)
{
    if (!ch) return 0.0;
    double gain = (ch->physical_max - ch->physical_min) /
                  (double)(ch->digital_max - ch->digital_min);
    return ch->physical_min + gain * (digital - ch->digital_min);
}

int16_t eni_edf_physical_to_digital(const eni_channel_info_t *ch, double physical)
{
    if (!ch) return 0;
    double gain = (double)(ch->digital_max - ch->digital_min) /
                  (ch->physical_max - ch->physical_min);
    double d = ch->digital_min + gain * (physical - ch->physical_min);
    if (d < -32768.0) d = -32768.0;
    if (d > 32767.0) d = 32767.0;
    return (int16_t)round(d);
}

eni_status_t eni_edf_open(eni_edf_file_t *edf, const char *path)
{
    if (!edf || !path) return ENI_ERR_INVALID;
    memset(edf, 0, sizeof(*edf));

    edf->fp = fopen(path, "rb");
    if (!edf->fp) return ENI_ERR_IO;

    edf->writing = false;
    edf->header.format = ENI_FORMAT_EDF;

    /* Read main header (256 bytes) */
    char buf[81];
    read_fixed(edf->fp, buf, 8);  /* version */
    read_fixed(edf->fp, edf->header.patient, 80);
    read_fixed(edf->fp, edf->header.recording, 80);
    read_fixed(edf->fp, edf->header.start_date, 8);
    read_fixed(edf->fp, edf->header.start_time, 8);

    read_fixed(edf->fp, buf, 8);  /* header size in bytes */
    int header_bytes = atoi(buf);

    read_fixed(edf->fp, buf, 44); /* reserved / EDF+C or EDF+D */

    read_fixed(edf->fp, buf, 8);  /* number of data records */
    edf->header.num_records = atoll(buf);

    read_fixed(edf->fp, buf, 8);  /* duration of data record */
    edf->header.record_duration = atof(buf);

    read_fixed(edf->fp, buf, 4);  /* number of signals */
    edf->header.num_channels = atoi(buf);

    if (edf->header.num_channels <= 0 || edf->header.num_channels > ENI_DATA_MAX_CHANNELS) {
        fclose(edf->fp);
        return ENI_ERR_CONFIG;
    }

    int ns = edf->header.num_channels;

    /* Read signal headers */
    for (int i = 0; i < ns; i++)
        read_fixed(edf->fp, edf->header.channels[i].label, 16);
    for (int i = 0; i < ns; i++)
        read_fixed(edf->fp, edf->header.channels[i].transducer, 80);
    for (int i = 0; i < ns; i++)
        read_fixed(edf->fp, edf->header.channels[i].physical_dim, 8);
    for (int i = 0; i < ns; i++) {
        read_fixed(edf->fp, buf, 8);
        edf->header.channels[i].physical_min = atof(buf);
    }
    for (int i = 0; i < ns; i++) {
        read_fixed(edf->fp, buf, 8);
        edf->header.channels[i].physical_max = atof(buf);
    }
    for (int i = 0; i < ns; i++) {
        read_fixed(edf->fp, buf, 8);
        edf->header.channels[i].digital_min = atoi(buf);
    }
    for (int i = 0; i < ns; i++) {
        read_fixed(edf->fp, buf, 8);
        edf->header.channels[i].digital_max = atoi(buf);
    }
    for (int i = 0; i < ns; i++)
        read_fixed(edf->fp, buf, 80); /* prefiltering — skip */
    for (int i = 0; i < ns; i++) {
        read_fixed(edf->fp, buf, 8);
        edf->header.channels[i].samples_per_record = atoi(buf);
        if (edf->header.record_duration > 0.0)
            edf->header.channels[i].sample_rate =
                edf->header.channels[i].samples_per_record / edf->header.record_duration;
    }
    for (int i = 0; i < ns; i++)
        read_fixed(edf->fp, buf, 32); /* reserved per signal — skip */

    /* Calculate record size and data offset */
    edf->record_size = 0;
    for (int i = 0; i < ns; i++)
        edf->record_size += edf->header.channels[i].samples_per_record * 2; /* 16-bit */

    edf->data_offset = header_bytes;

    ENI_LOG_INFO("edf", "opened %s: %d channels, %lld records, %.1fs/record",
                 path, ns, (long long)edf->header.num_records, edf->header.record_duration);
    return ENI_OK;
}

eni_status_t eni_edf_read_record(eni_edf_file_t *edf, int64_t record_idx,
                                  double *samples, int max_samples)
{
    if (!edf || !edf->fp || edf->writing || !samples) return ENI_ERR_INVALID;
    if (record_idx < 0 || record_idx >= edf->header.num_records) return ENI_ERR_INVALID;

    int64_t offset = edf->data_offset + record_idx * edf->record_size;
    if (fseek(edf->fp, (long)offset, SEEK_SET) != 0) return ENI_ERR_IO;

    int sample_idx = 0;
    for (int ch = 0; ch < edf->header.num_channels; ch++) {
        int spr = edf->header.channels[ch].samples_per_record;
        for (int s = 0; s < spr && sample_idx < max_samples; s++) {
            uint8_t lo, hi;
            if (fread(&lo, 1, 1, edf->fp) != 1) return ENI_ERR_IO;
            if (fread(&hi, 1, 1, edf->fp) != 1) return ENI_ERR_IO;
            int16_t digital = (int16_t)((hi << 8) | lo);
            samples[sample_idx++] = eni_edf_digital_to_physical(&edf->header.channels[ch], digital);
        }
    }
    return ENI_OK;
}

eni_status_t eni_edf_read_annotations(eni_edf_file_t *edf)
{
    /* Find EDF Annotations channel */
    if (!edf || !edf->fp) return ENI_ERR_INVALID;

    int annot_ch = -1;
    for (int i = 0; i < edf->header.num_channels; i++) {
        if (strstr(edf->header.channels[i].label, "Annotations") != NULL) {
            annot_ch = i;
            break;
        }
    }
    if (annot_ch < 0) return ENI_OK; /* No annotations channel */

    edf->annotation_count = 0;
    /* Parse TAL (Time-stamped Annotation Lists) from annotation channel */
    /* Each TAL: +onset\x15duration\x14annotation\x14\x00 */
    for (int64_t r = 0; r < edf->header.num_records && edf->annotation_count < ENI_EDF_MAX_ANNOTATIONS; r++) {
        int64_t offset = edf->data_offset + r * edf->record_size;
        /* Skip to annotation channel */
        for (int c = 0; c < annot_ch; c++)
            offset += edf->header.channels[c].samples_per_record * 2;

        if (fseek(edf->fp, (long)offset, SEEK_SET) != 0) continue;

        int annot_bytes = edf->header.channels[annot_ch].samples_per_record * 2;
        char *buf = (char *)malloc((size_t)annot_bytes + 1);
        if (!buf) return ENI_ERR_NOMEM;

        size_t nread = fread(buf, 1, (size_t)annot_bytes, edf->fp);
        buf[nread] = '\0';

        /* Simple TAL parse */
        int pos = 0;
        while (pos < (int)nread && edf->annotation_count < ENI_EDF_MAX_ANNOTATIONS) {
            if (buf[pos] != '+' && buf[pos] != '-') { pos++; continue; }

            eni_edf_annotation_t *a = &edf->annotations[edf->annotation_count];
            a->onset = strtod(&buf[pos], NULL);

            /* Find duration separator (0x15) */
            int dur_start = pos;
            while (dur_start < (int)nread && buf[dur_start] != '\x15' && buf[dur_start] != '\x14')
                dur_start++;

            a->duration = 0.0;
            if (dur_start < (int)nread && buf[dur_start] == '\x15') {
                a->duration = strtod(&buf[dur_start + 1], NULL);
                dur_start++;
                while (dur_start < (int)nread && buf[dur_start] != '\x14') dur_start++;
            }

            /* Find text (after 0x14) */
            int text_start = dur_start;
            if (text_start < (int)nread && buf[text_start] == '\x14') text_start++;

            int text_end = text_start;
            while (text_end < (int)nread && buf[text_end] != '\x14' && buf[text_end] != '\0')
                text_end++;

            int text_len = text_end - text_start;
            if (text_len > 0 && text_len < (int)sizeof(a->text)) {
                memcpy(a->text, &buf[text_start], (size_t)text_len);
                a->text[text_len] = '\0';
                edf->annotation_count++;
            }

            pos = text_end + 1;
            if (pos < (int)nread && buf[pos] == '\0') break; /* End of TAL */
        }
        free(buf);
    }
    return ENI_OK;
}

void eni_edf_close(eni_edf_file_t *edf)
{
    if (!edf) return;
    if (edf->fp) { fclose(edf->fp); edf->fp = NULL; }
}

eni_status_t eni_edf_create(eni_edf_file_t *edf, const char *path,
                             const eni_data_header_t *header)
{
    if (!edf || !path || !header) return ENI_ERR_INVALID;
    if (header->num_channels <= 0 || header->num_channels > ENI_DATA_MAX_CHANNELS)
        return ENI_ERR_INVALID;

    memset(edf, 0, sizeof(*edf));
    edf->header = *header;
    edf->header.format = ENI_FORMAT_EDF;
    edf->writing = true;

    edf->fp = fopen(path, "wb");
    if (!edf->fp) return ENI_ERR_IO;

    int ns = header->num_channels;

    /* Write main header */
    write_fixed(edf->fp, "0", 8);        /* version */
    write_fixed(edf->fp, header->patient, 80);
    write_fixed(edf->fp, header->recording, 80);
    write_fixed(edf->fp, header->start_date[0] ? header->start_date : "01.01.26", 8);
    write_fixed(edf->fp, header->start_time[0] ? header->start_time : "00.00.00", 8);

    int header_size = 256 + ns * 256;
    write_fixed_int(edf->fp, header_size, 8);
    write_fixed(edf->fp, "EDF+C", 44);   /* reserved — continuous */
    write_fixed(edf->fp, "-1", 8);        /* num records — updated on finalize */
    write_fixed_double(edf->fp, header->record_duration > 0.0 ? header->record_duration : 1.0, 8);
    write_fixed_int(edf->fp, ns, 4);

    /* Signal headers */
    for (int i = 0; i < ns; i++)
        write_fixed(edf->fp, header->channels[i].label, 16);
    for (int i = 0; i < ns; i++)
        write_fixed(edf->fp, header->channels[i].transducer, 80);
    for (int i = 0; i < ns; i++)
        write_fixed(edf->fp, header->channels[i].physical_dim, 8);
    for (int i = 0; i < ns; i++)
        write_fixed_double(edf->fp, header->channels[i].physical_min, 8);
    for (int i = 0; i < ns; i++)
        write_fixed_double(edf->fp, header->channels[i].physical_max, 8);
    for (int i = 0; i < ns; i++)
        write_fixed_int(edf->fp, header->channels[i].digital_min, 8);
    for (int i = 0; i < ns; i++)
        write_fixed_int(edf->fp, header->channels[i].digital_max, 8);
    for (int i = 0; i < ns; i++)
        write_fixed(edf->fp, "", 80); /* prefiltering */
    for (int i = 0; i < ns; i++)
        write_fixed_int(edf->fp, header->channels[i].samples_per_record, 8);
    for (int i = 0; i < ns; i++)
        write_fixed(edf->fp, "", 32); /* reserved per signal */

    edf->data_offset = header_size;
    edf->record_size = 0;
    for (int i = 0; i < ns; i++)
        edf->record_size += header->channels[i].samples_per_record * 2;

    return ENI_OK;
}

eni_status_t eni_edf_write_record(eni_edf_file_t *edf, const double *samples,
                                   int num_samples)
{
    if (!edf || !edf->fp || !edf->writing || !samples) return ENI_ERR_INVALID;

    int sample_idx = 0;
    for (int ch = 0; ch < edf->header.num_channels; ch++) {
        int spr = edf->header.channels[ch].samples_per_record;
        for (int s = 0; s < spr && sample_idx < num_samples; s++) {
            int16_t digital = eni_edf_physical_to_digital(&edf->header.channels[ch], samples[sample_idx++]);
            uint8_t lo = (uint8_t)(digital & 0xFF);
            uint8_t hi = (uint8_t)((digital >> 8) & 0xFF);
            fwrite(&lo, 1, 1, edf->fp);
            fwrite(&hi, 1, 1, edf->fp);
        }
    }
    edf->header.num_records++;
    return ENI_OK;
}

eni_status_t eni_edf_write_annotation(eni_edf_file_t *edf, double onset,
                                       double duration, const char *text)
{
    if (!edf || edf->annotation_count >= ENI_EDF_MAX_ANNOTATIONS) return ENI_ERR_OVERFLOW;

    eni_edf_annotation_t *a = &edf->annotations[edf->annotation_count++];
    a->onset = onset;
    a->duration = duration;
    if (text) strncpy(a->text, text, sizeof(a->text) - 1);
    return ENI_OK;
}

eni_status_t eni_edf_finalize(eni_edf_file_t *edf)
{
    if (!edf || !edf->fp || !edf->writing) return ENI_ERR_INVALID;

    /* Update number of records in header */
    fseek(edf->fp, 236, SEEK_SET);
    write_fixed_int(edf->fp, (int)edf->header.num_records, 8);

    fclose(edf->fp);
    edf->fp = NULL;
    edf->writing = false;
    return ENI_OK;
}
