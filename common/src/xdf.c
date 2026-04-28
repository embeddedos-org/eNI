// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// XDF (Extensible Data Format) reader/writer

#include "eni/xdf.h"
#include "eni/log.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static uint32_t xdf_read_varlen(FILE *fp)
{
    uint8_t b;
    if (fread(&b, 1, 1, fp) != 1) return 0;

    if (b == 1) {
        uint8_t len;
        if (fread(&len, 1, 1, fp) != 1) return 0;
        return len;
    } else if (b == 4) {
        uint32_t len;
        if (fread(&len, 1, 4, fp) != 4) return 0;
        return len;
    } else if (b == 8) {
        /* 64-bit length — we truncate to 32-bit */
        uint32_t lo, hi;
        if (fread(&lo, 1, 4, fp) != 4) return 0;
        if (fread(&hi, 1, 4, fp) != 4) return 0;
        return lo;
    }
    return b; /* length byte itself is the length */
}

static void xdf_write_varlen(FILE *fp, uint32_t len)
{
    if (len < 256) {
        uint8_t tag = 1, l = (uint8_t)len;
        fwrite(&tag, 1, 1, fp);
        fwrite(&l, 1, 1, fp);
    } else {
        uint8_t tag = 4;
        fwrite(&tag, 1, 1, fp);
        fwrite(&len, 1, 4, fp);
    }
}

eni_status_t eni_xdf_open(eni_xdf_file_t *xdf, const char *path)
{
    if (!xdf || !path) return ENI_ERR_INVALID;
    memset(xdf, 0, sizeof(*xdf));

    xdf->fp = fopen(path, "rb");
    if (!xdf->fp) return ENI_ERR_IO;

    /* Verify magic: "XDF:" */
    char magic[4];
    if (fread(magic, 1, 4, xdf->fp) != 4 || memcmp(magic, "XDF:", 4) != 0) {
        fclose(xdf->fp);
        return ENI_ERR_CONFIG;
    }

    /* Read chunks */
    while (!feof(xdf->fp)) {
        uint32_t chunk_len = xdf_read_varlen(xdf->fp);
        if (chunk_len == 0) break;

        uint16_t chunk_tag;
        if (fread(&chunk_tag, 1, 2, xdf->fp) != 2) break;

        long content_len = (long)chunk_len - 2;
        if (content_len <= 0) continue;

        switch (chunk_tag) {
        case ENI_XDF_CHUNK_FILEHEADER: {
            size_t to_read = (size_t)content_len;
            if (to_read >= sizeof(xdf->file_header)) to_read = sizeof(xdf->file_header) - 1;
            fread(xdf->file_header, 1, to_read, xdf->fp);
            if ((long)to_read < content_len) fseek(xdf->fp, content_len - (long)to_read, SEEK_CUR);
            break;
        }
        case ENI_XDF_CHUNK_STREAMHEADER: {
            if (xdf->stream_count >= ENI_XDF_MAX_STREAMS) {
                fseek(xdf->fp, content_len, SEEK_CUR);
                break;
            }
            uint32_t stream_id;
            if (fread(&stream_id, 1, 4, xdf->fp) != 4) goto done;
            content_len -= 4;

            eni_xdf_stream_t *s = &xdf->streams[xdf->stream_count];
            memset(s, 0, sizeof(*s));
            s->stream_id = (int)stream_id;

            /* Read XML content (simplified parse) */
            char *xml = (char *)malloc((size_t)content_len + 1);
            if (!xml) { fseek(xdf->fp, content_len, SEEK_CUR); break; }
            fread(xml, 1, (size_t)content_len, xdf->fp);
            xml[content_len] = '\0';

            /* Extract simple fields from XML */
            char *p;
            if ((p = strstr(xml, "<name>")) != NULL) {
                p += 6;
                char *end = strstr(p, "</name>");
                if (end) { int n = (int)(end - p); if (n > 79) n = 79; memcpy(s->name, p, (size_t)n); }
            }
            if ((p = strstr(xml, "<type>")) != NULL) {
                p += 6;
                char *end = strstr(p, "</type>");
                if (end) { int n = (int)(end - p); if (n > 31) n = 31; memcpy(s->type, p, (size_t)n); }
            }
            if ((p = strstr(xml, "<channel_count>")) != NULL) {
                s->channel_count = atoi(p + 15);
            }
            if ((p = strstr(xml, "<nominal_srate>")) != NULL) {
                s->nominal_srate = atof(p + 15);
            }
            if ((p = strstr(xml, "<channel_format>")) != NULL) {
                p += 16;
                char *end = strstr(p, "</channel_format>");
                if (end) { int n = (int)(end - p); if (n > 15) n = 15; memcpy(s->channel_format, p, (size_t)n); }
            }
            free(xml);
            xdf->stream_count++;
            break;
        }
        case ENI_XDF_CHUNK_CLOCKOFFSET: {
            uint32_t stream_id;
            if (fread(&stream_id, 1, 4, xdf->fp) != 4) goto done;
            double offset;
            if (fread(&offset, 1, 8, xdf->fp) != 8) goto done;
            fseek(xdf->fp, content_len - 12, SEEK_CUR);

            for (int i = 0; i < xdf->stream_count; i++) {
                if (xdf->streams[i].stream_id == (int)stream_id &&
                    xdf->streams[i].clock_offset_count < 64) {
                    xdf->streams[i].clock_offsets[xdf->streams[i].clock_offset_count++] = offset;
                }
            }
            break;
        }
        default:
            fseek(xdf->fp, content_len, SEEK_CUR);
            break;
        }
    }

done:
    ENI_LOG_INFO("xdf", "opened %s: %d streams", path, xdf->stream_count);
    return ENI_OK;
}

eni_status_t eni_xdf_read_samples(eni_xdf_file_t *xdf, int stream_id,
                                   double *samples, int max_samples,
                                   int *samples_read)
{
    if (!xdf || !xdf->fp || !samples || !samples_read) return ENI_ERR_INVALID;

    *samples_read = 0;
    rewind(xdf->fp);
    fseek(xdf->fp, 4, SEEK_SET); /* skip magic */

    while (!feof(xdf->fp) && *samples_read < max_samples) {
        uint32_t chunk_len = xdf_read_varlen(xdf->fp);
        if (chunk_len == 0) break;

        uint16_t chunk_tag;
        if (fread(&chunk_tag, 1, 2, xdf->fp) != 2) break;
        long content_len = (long)chunk_len - 2;

        if (chunk_tag == ENI_XDF_CHUNK_SAMPLES) {
            uint32_t sid;
            if (fread(&sid, 1, 4, xdf->fp) != 4) break;
            content_len -= 4;

            if ((int)sid == stream_id) {
                int n = content_len / 8; /* assume double64 */
                for (int i = 0; i < n && *samples_read < max_samples; i++) {
                    double val;
                    if (fread(&val, 1, 8, xdf->fp) != 8) break;
                    samples[(*samples_read)++] = val;
                    content_len -= 8;
                }
            }
            if (content_len > 0) fseek(xdf->fp, content_len, SEEK_CUR);
        } else {
            fseek(xdf->fp, content_len, SEEK_CUR);
        }
    }
    return ENI_OK;
}

void eni_xdf_close(eni_xdf_file_t *xdf)
{
    if (xdf && xdf->fp) { fclose(xdf->fp); xdf->fp = NULL; }
}

eni_status_t eni_xdf_create(eni_xdf_file_t *xdf, const char *path)
{
    if (!xdf || !path) return ENI_ERR_INVALID;
    memset(xdf, 0, sizeof(*xdf));

    xdf->fp = fopen(path, "wb");
    if (!xdf->fp) return ENI_ERR_IO;
    xdf->writing = true;

    fwrite("XDF:", 1, 4, xdf->fp);

    /* File header chunk */
    const char *fh = "<?xml version=\"1.0\"?><info><version>1.0</version></info>";
    uint32_t fh_len = (uint32_t)strlen(fh) + 2;
    xdf_write_varlen(xdf->fp, fh_len);
    uint16_t tag = ENI_XDF_CHUNK_FILEHEADER;
    fwrite(&tag, 1, 2, xdf->fp);
    fwrite(fh, 1, strlen(fh), xdf->fp);

    return ENI_OK;
}

eni_status_t eni_xdf_add_stream(eni_xdf_file_t *xdf, const eni_xdf_stream_t *stream)
{
    if (!xdf || !xdf->writing || !stream) return ENI_ERR_INVALID;
    if (xdf->stream_count >= ENI_XDF_MAX_STREAMS) return ENI_ERR_OVERFLOW;

    xdf->streams[xdf->stream_count++] = *stream;

    /* Write stream header chunk */
    char xml[512];
    int xml_len = snprintf(xml, sizeof(xml),
        "<?xml version=\"1.0\"?><info>"
        "<name>%s</name><type>%s</type>"
        "<channel_count>%d</channel_count>"
        "<nominal_srate>%.1f</nominal_srate>"
        "<channel_format>%s</channel_format>"
        "</info>",
        stream->name, stream->type,
        stream->channel_count, stream->nominal_srate,
        stream->channel_format[0] ? stream->channel_format : "double64");

    uint32_t chunk_len = (uint32_t)xml_len + 6;
    xdf_write_varlen(xdf->fp, chunk_len);
    uint16_t tag = ENI_XDF_CHUNK_STREAMHEADER;
    fwrite(&tag, 1, 2, xdf->fp);
    uint32_t sid = (uint32_t)stream->stream_id;
    fwrite(&sid, 1, 4, xdf->fp);
    fwrite(xml, 1, (size_t)xml_len, xdf->fp);

    return ENI_OK;
}

eni_status_t eni_xdf_write_samples(eni_xdf_file_t *xdf, int stream_id,
                                    const double *samples, int num_samples)
{
    if (!xdf || !xdf->writing || !xdf->fp || !samples) return ENI_ERR_INVALID;

    uint32_t chunk_len = (uint32_t)(num_samples * 8 + 6);
    xdf_write_varlen(xdf->fp, chunk_len);
    uint16_t tag = ENI_XDF_CHUNK_SAMPLES;
    fwrite(&tag, 1, 2, xdf->fp);
    uint32_t sid = (uint32_t)stream_id;
    fwrite(&sid, 1, 4, xdf->fp);
    fwrite(samples, 8, (size_t)num_samples, xdf->fp);

    return ENI_OK;
}

eni_status_t eni_xdf_write_clock_offset(eni_xdf_file_t *xdf, int stream_id, double offset)
{
    if (!xdf || !xdf->writing || !xdf->fp) return ENI_ERR_INVALID;

    uint32_t chunk_len = 14; /* 2 tag + 4 sid + 8 offset */
    xdf_write_varlen(xdf->fp, chunk_len);
    uint16_t tag = ENI_XDF_CHUNK_CLOCKOFFSET;
    fwrite(&tag, 1, 2, xdf->fp);
    uint32_t sid = (uint32_t)stream_id;
    fwrite(&sid, 1, 4, xdf->fp);
    fwrite(&offset, 1, 8, xdf->fp);

    return ENI_OK;
}

eni_status_t eni_xdf_finalize(eni_xdf_file_t *xdf)
{
    if (!xdf || !xdf->fp || !xdf->writing) return ENI_ERR_INVALID;

    /* Write stream footer chunks */
    for (int i = 0; i < xdf->stream_count; i++) {
        const char *xml = "<?xml version=\"1.0\"?><info></info>";
        uint32_t chunk_len = (uint32_t)strlen(xml) + 6;
        xdf_write_varlen(xdf->fp, chunk_len);
        uint16_t tag = ENI_XDF_CHUNK_STREAMFOOTER;
        fwrite(&tag, 1, 2, xdf->fp);
        uint32_t sid = (uint32_t)xdf->streams[i].stream_id;
        fwrite(&sid, 1, 4, xdf->fp);
        fwrite(xml, 1, strlen(xml), xdf->fp);
    }

    fclose(xdf->fp);
    xdf->fp = NULL;
    xdf->writing = false;
    return ENI_OK;
}
