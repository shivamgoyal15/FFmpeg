/*
 * DICOM demuxer
 *
 * Copyright (c) 2019 Shivam Goyal
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "libavutil/avstring.h"
#include "libavutil/dict.h"
#include "libavutil/opt.h"
#include "libavutil/bprint.h"
#include "avformat.h"
#include "internal.h"
#include "dicom.h"


typedef struct DICOMContext {
    const AVClass *class; ///< Class for private options.
    int big_endian;

    uint64_t nb_frames;
    uint16_t bits_allocated;
    uint16_t bits_stored;
    uint16_t high_bit;
    uint16_t rows;
    uint16_t columns;
    uint32_t nb_DEs;
    TransferSyntax transfer_syntax;
    int window;
    int level;
    int metadata;
} DICOMContext;


static int dicom_probe(const AVProbeData *p)
{
    const uint8_t *d = p->buf;

    if (d[DICOM_PREAMBLE_SIZE] == 'D' &&
        d[DICOM_PREAMBLE_SIZE + 1] == 'I' &&
        d[DICOM_PREAMBLE_SIZE + 2] == 'C' &&
        d[DICOM_PREAMBLE_SIZE + 3] == 'M') {
        return AVPROBE_SCORE_MAX;
    }
    return 0;
}

static int set_imagegroup_data(AVFormatContext *s, AVStream* st, DataElement *de)
{
    DICOMContext *dicom = s->priv_data;
    void *bytes = de->bytes;
    int len = de->VL;
    char *cbytes;

    if (de->GroupNumber != IMAGE_GR_NB)
        return 0;

    switch (de->ElementNumber) {
        case 0x0010:
            dicom->rows = *(uint16_t *)bytes;
            st->codecpar->height = dicom->rows;
            break;
        case 0x0011:
            dicom->columns = *(uint16_t *)bytes;
            st->codecpar->width = dicom->columns;
            break;
        case 0x0100:
            dicom->bits_allocated = *(uint16_t *)bytes;
            st->codecpar->bits_per_raw_sample = dicom->bits_allocated;
            break;
        case 0x0101:
            dicom->bits_stored = *(uint16_t *)bytes;
            st->codecpar->bits_per_coded_sample = dicom->bits_stored;
            break;
        case 0x0102:
            dicom->high_bit = *(uint16_t *)bytes;
            break;
        case 0x0008:
            dicom->nb_frames = *(uint32_t *)bytes;
            break;
        case 0x1050:
            if (dicom->level == -1) {
                cbytes = av_malloc(len + 1);
                memcpy(cbytes, bytes, len);
                cbytes[len] = 0;
                st->codecpar->level = atoi(cbytes);
                dicom->level = st->codecpar->level;
                av_free(cbytes);
            }
            break;
        case 0x1051:
            if (dicom->window == -1) {
                cbytes = av_malloc(len + 1);
                memcpy(cbytes, bytes, len);
                cbytes[len] = 0;
                st->codecpar->profile = atoi(cbytes);
                dicom->level = st->codecpar->profile;
                av_free(cbytes);
            }
            break;
    }
    return 0;
}



static int read_data_element_metainfo(AVFormatContext *s, DataElement *de)
{
    ValueRepresentation vr;
    int ret;
    int bytes_read = 0;

    ret = avio_rl16(s->pb);
    if (ret < 0)
        return ret;
    de->GroupNumber = ret;

    de->ElementNumber =  avio_rl16(s->pb);

    vr = avio_rb16(s->pb); // Stored in Big Endian in dicom.h
    de->VR = vr;
    bytes_read += 6;
    switch (vr) {
        case OB:
        case OD:
        case OF:
        case OL:
        case OV:
        case OW:
        case SQ:
        case SV:
        case UC:
        case UR:
        case UT:
        case UN:
        case UV:
            avio_skip(s->pb, 2); // Padding always 0x0000
            de->VL = avio_rl32(s->pb);
            bytes_read += 6;
            if (de->VL % 2)
                av_log(s, AV_LOG_WARNING,"Data Element Value length:%d can't be odd\n", de->VL);
            break;
        default:
            de->VL = avio_rl16(s->pb);
            bytes_read += 2;
            if (de->VL % 2)
                av_log(s, AV_LOG_WARNING,"Data Element Value length:%d can't be odd\n", de->VL);
            break;
    }
    return bytes_read;
}

static int read_data_element_valuefield(AVFormatContext *s, DataElement *de)
{
    int ret;
    uint32_t len = de->VL;

    de->bytes = av_malloc(len);
    ret = avio_read(s->pb, de->bytes, len);
    return ret;
}

static int print_data_element_metadata(AVFormatContext *s, DataElement *de) {
    av_log(s, AV_LOG_INFO,"(%04x, %04x)\tVR=%c%c\tVL=%d\t",de->GroupNumber, de->ElementNumber, de->VR>>8, de->VR - ((de->VR>>8)<<8), de->VL);
    return 0;
}

static int print_data_element_value(AVFormatContext *s, DataElement *de) {
    ValueRepresentation vr = de->VR;
    uint32_t len = de->VL;
    void *data = de->bytes;
    char *cdata;

    switch (vr) {
        case AT:
            if (len < 4) {
                printf("[Invalid]\n");
                return 0;
            }
            cdata = data;
            av_log(s, AV_LOG_INFO, "%04d %04d", cdata[1] << 8 + cdata[0], cdata[4] << 8 + cdata[3]);
            break;
        case FL:
            av_log(s, AV_LOG_INFO, "%f\n", ((float *)data)[0]);
            break;
        default:
            cdata = av_malloc(len + 1);
            memcpy(cdata, data, len);
            cdata[len] = 0;
            av_log(s, AV_LOG_INFO, "%s\n", cdata);
            av_free(cdata);
    }
    return 0;
}

static char *get_key_str(DataElement *de) {
    char *key;
    if (!de->GroupNumber || !de->ElementNumber)
        return 0;
    key = (char *) av_malloc(10);
    sprintf(key, "%04x,%04x", de->GroupNumber, de->ElementNumber);
    key[9] = 0;
    return key;
}

// detects transfer syntax
static TransferSyntax get_transfer_sytax (const char *ts) {
    if (!strcmp(ts, "1.2.840.10008.1.2"))
        return IMPLICIT_VR;
    else if (!strcmp(ts, "1.2.840.10008.1.2.1"))
        return EXPLICIT_VR;
    else
        return UNSUPPORTED_TS;
}

static int dicom_read_header(AVFormatContext *s)
{
    AVIOContext  *pb = s->pb;
    AVDictionary **m = &s->metadata;
    DICOMContext *dicom = s->priv_data;
    DataElement *de;
    char *key;
    char *value;
    uint32_t header_size;
    uint32_t bytes_read = 0;
    int ret;

    ret = avio_skip(pb, DICOM_PREAMBLE_SIZE + DICOM_PREFIX_SIZE);
    if (ret < 0)
        return ret;
    de = av_malloc(sizeof(DataElement));
    ret = read_data_element_metainfo(s, de);
    ret = read_data_element_valuefield(s, de);
    if (ret < 0)
        return ret;
    if (de->GroupNumber != 0x02 || de->ElementNumber != 0x00) {
        av_log(s, AV_LOG_WARNING,"First data element is not \'File MetaInfo Group Length\'");
        header_size = 200; // Fallback to default length
    } else {
        header_size = *(uint32_t *)de->bytes;
    }
    while(bytes_read < header_size) {
        de = av_malloc(sizeof(DataElement));
        ret = read_data_element_metainfo(s, de);
        if (ret < 0)
            return ret;
        bytes_read += ret;
        key = get_key_str(de);
        ret = read_data_element_valuefield(s, de);
        if (ret < 0)
            return ret;
        bytes_read += ret;
        value = (char *) de->bytes;
        value = av_malloc(de->VL + 1);
        memcpy(value, de->bytes, de->VL);
        value[de->VL] = 0;
        if (de->GroupNumber == TS_GR_NB && de->ElementNumber == TS_EL_NB) {
            dicom->transfer_syntax = get_transfer_sytax(value);
        }
        av_dict_set(m, key, value, AV_DICT_DONT_STRDUP_KEY | AV_DICT_DONT_STRDUP_VAL);
        av_free(de);
    }
    s->ctx_flags |= AVFMTCTX_NOHEADER;
    s->start_time = 0;
    return 0;
}

static int dicom_read_packet(AVFormatContext *s, AVPacket *pkt)
{
    DICOMContext *dicom = s->priv_data;
    int metadata = dicom->metadata;
    AVStream  *st;
    DataElement *de;
    int ret;

    if (s->nb_streams < 1) {
        st = avformat_new_stream(s, NULL);
        if (!st)
            return AVERROR(ENOMEM);
        st->codecpar->codec_id = AV_CODEC_ID_DICOM;
        st->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
        if (st->codecpar->profile != -1) {
            st->codecpar->profile = dicom->window;
        }
        if (st->codecpar->level != -1) {
            st->codecpar->level = dicom->level;
        }
    } else
        st = s->streams[0];

    for (;;) {
        ret = avio_feof(s->pb);
        if (ret)
            return AVERROR_EOF;
        
        de = av_malloc(sizeof(DataElement));
        ret = read_data_element_metainfo(s,de);
        if (ret < 0)
            return ret;

        if (de->GroupNumber == IMAGE_GR_NB) {
            ret = read_data_element_valuefield(s, de);
            if (ret < 0)
                return ret;
            set_imagegroup_data(s, st, de);
        } else if (de->GroupNumber == PIXEL_GR_NB && de->ElementNumber == PIXELDATA_EL_NB) {
            if (av_new_packet(pkt, de->VL) < 0)
                return AVERROR(ENOMEM);
            pkt->pos = avio_tell(s->pb);
            pkt->stream_index = 0;
            pkt->size = de->VL;
            ret = avio_read(s->pb, pkt->data, de->VL);
            if (ret < 0)
                av_packet_unref(pkt);
            return ret;
        }
        else {
            ret = read_data_element_valuefield(s, de);
            if (ret < 0)
                return ret;
            if (metadata) {
                print_data_element_metadata(s, de);
                print_data_element_value(s,de);
            }
        }
        av_free(de);
    }
    return AVERROR_EOF;
}

static const AVOption options[] = {
    { "window", "window", offsetof(DICOMContext, window), AV_OPT_TYPE_INT, {.i64 = -1}, -1, 99999, AV_OPT_FLAG_DECODING_PARAM },
    { "level", "level", offsetof(DICOMContext, level), AV_OPT_TYPE_INT, {.i64 = -1}, -1, 99999   , AV_OPT_FLAG_DECODING_PARAM },
    { "metadata", "Print metadata present in dicom file", offsetof(DICOMContext, metadata)  , AV_OPT_TYPE_BOOL,{.i64 = 0}, 0,1, AV_OPT_FLAG_DECODING_PARAM },
    { NULL },
};

static const AVClass dicom_class = {
    .class_name = "dicomdec",
    .item_name  = av_default_item_name,
    .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
};

AVInputFormat ff_dicom_demuxer = {
    .name           = "dicom",
    .long_name      = NULL_IF_CONFIG_SMALL("DICOM (Digital Imaging and Communications in Medicine)"),
    .priv_data_size = sizeof(DICOMContext),
    .read_probe     = dicom_probe,
    .read_header    = dicom_read_header,
    .read_packet    = dicom_read_packet,
    .extensions     = "dcm",
    .priv_class     = &dicom_class,
};
