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
#include "libavcodec/bytestream.h"
#include "avformat.h"
#include "internal.h"
#include "dicom.h"


typedef struct DICOMContext {
    const AVClass *class;

    int interpret, pixrep, slope, intcpt, samples_ppix;
    uint16_t width;
    uint16_t height;
    uint64_t nb_frames;
    uint64_t frame_size;
    int frame_nb;
    double delay;
    int duration;
    int inheader;
    int inseq;
    int32_t pixpad;
    TransferSyntax transfer_syntax;

    int window; // Options
    int level;
    int metadata;
} DICOMContext;


static int dicom_probe(const AVProbeData *p)
{
    if (!strncmp(p->buf + DICOM_PREAMBLE_SIZE, "DICM", 4))
        return AVPROBE_SCORE_MAX;
    return 0;
}

static DataElement *alloc_de(void) {
    DataElement *de = (DataElement *) av_malloc(sizeof(DataElement));
    de->is_found = 0;
    de->desc = NULL;
    de->bytes = NULL;
    return de;
}

static void free_seq(DataElement *de) {
    int i = 0;
    DataElement *seq_data = de->bytes;
    for(; i < MAX_UNDEF_LENGTH; ++i) {
        if (seq_data[i].ElementNumber == SEQ_DEL_EL_NB)
            return;
        av_free(seq_data[i].bytes);
    }
}

static void free_de(DataElement *de) {
    if (!de)
        return;
    if (de->VL == UNDEFINED_VL)
        free_seq(de);
    av_free(de->bytes);
    av_free(de->desc);
    av_free(de);
}

static void set_context_defaults(DICOMContext *dicom) {
    dicom->interpret = 0x02; // 2 for MONOCHROME2, 1 for MONOCHROME1
    dicom->pixrep = 1;
    dicom->pixpad = 1 << 31;
    dicom->slope = 1;
    dicom->intcpt = 0;
    dicom->samples_ppix = 1;

    dicom->delay = 100;
    dicom->frame_nb = 1;
    dicom->nb_frames = 1;
    dicom->inseq = 0;
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

static int find_PI(const char *pi) {
    if (!strcmp(pi, "MONOCHROME1 "))
        return 0x01;
    else if (!strcmp(pi, "MONOCHROME2 "))
        return 0x02;
    else if (!strcmp(pi, "PALETTE COLOR "))
        return 0x03;
    else if (!strcmp(pi, "RGB "))
        return 0x04;
    else return 0x00;
}

static void set_dec_extradata(AVFormatContext *s, AVStream *st) {
    DICOMContext *dicom = s->priv_data;
    uint8_t *edp;
    int size = DECODER_ED_SIZE + AV_INPUT_BUFFER_PADDING_SIZE;

    st->codecpar->extradata = (uint8_t *)av_malloc(size);
    edp = st->codecpar->extradata;
    bytestream_put_le32(&edp, dicom->interpret);
    bytestream_put_le32(&edp, dicom->pixrep);
    bytestream_put_le32(&edp, dicom->pixpad);
    bytestream_put_le32(&edp, dicom->slope);
    bytestream_put_le32(&edp, dicom->intcpt);
    st->codecpar->extradata_size = size;
}

static double conv_DS(DataElement *de) {
    double ret;
    char *cbytes = av_malloc(de->VL + 1);

    memcpy(cbytes, de->bytes, de->VL);
    cbytes[de->VL] = 0;
    ret = atof(cbytes);
    av_free(cbytes);
    return ret;
}

static int conv_IS(DataElement *de) {
    int ret;
    char *cbytes = av_malloc(de->VL + 1);

    memcpy(cbytes, de->bytes, de->VL);
    cbytes[de->VL] = 0;
    ret = atoi(cbytes);
    av_free(cbytes);
    return ret;
}


static char *get_key_str(DataElement *de) {
    int len;
    char *key;
    if (!de->GroupNumber || !de->ElementNumber)
        return 0;
    len = 15 + strlen(de->desc);
    key = (char *) av_malloc(len);
    snprintf(key, len, "(%04x,%04x) %s", de->GroupNumber, de->ElementNumber, de->desc);
    return key;
}

static char *get_val_str(DataElement *de) {
    char *val;

    switch(de->VR) {
    case AT:
    case OB:
    case OD:
    case OF:
    case OL:
    case OV:
    case OW:
        val = av_strdup("[Binary data]");
        break;
    case UN:
    case SQ:
        val = av_strdup("[Sequence of items]");
        break;
    case FL:
        val = (char *) av_malloc(15);
        snprintf(val, 15, "%.3f", ((float *)de->bytes)[0]);
        break;
    case FD:
        val = (char *) av_malloc(20);
        snprintf(val, 20, "%.3lf", ((double *)de->bytes)[0]);
        break;
    case SL:
        val = (char *) av_malloc(15);
        snprintf(val, 15, "%d", AV_RL32(de->bytes));
        break;
    case SS:
        val = (char *) av_malloc(10);
        snprintf(val, 10, "%d", AV_RL16(de->bytes));
        break;
    case SV:
        val = (char *) av_malloc(25);
        snprintf(val, 25, "%"PRId64, AV_RL64(de->bytes));
        break;
    case UL:
        val = (char *) av_malloc(15);
        snprintf(val, 15, "%u", AV_RL32(de->bytes));
        break;
    case US:
        val = (char *) av_malloc(10);
        snprintf(val, 10, "%u", AV_RL16(de->bytes));
        break;
    case UV:
        val = (char *) av_malloc(25);
        snprintf(val, 25, "%"PRId64, AV_RL64(de->bytes));
        break;
    default:
        val = (char *) av_malloc(de->VL + 1);
        memcpy(val, de->bytes, de->VL);
        val[de->VL] = 0;
        break;
    }
    return val;
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
    case 0x0010: // rows
        dicom->height = AV_RL16(bytes);
        st->codecpar->height = dicom->height;
        break;
    case 0x0011: // columns
        dicom->width = AV_RL16(bytes);
        st->codecpar->width = dicom->width;
        break;
    case 0x0100: // bits allocated
        st->codecpar->bits_per_raw_sample = AV_RL16(bytes);
        break;
    case 0x0101: // bits stored
        st->codecpar->bits_per_coded_sample = AV_RL16(bytes);
        break;
    case 0x0008: // number of frames
        dicom->nb_frames = conv_IS(de);
        st->nb_frames = dicom->nb_frames;
        st->duration = dicom->delay * dicom->nb_frames;
        break;
    case 0x1050: // window center/level
        if (dicom->level == -1) {
            st->codecpar->level = conv_IS(de);
            dicom->level = st->codecpar->level;
        }
        break;
    case 0x1051: // window width/window
        if (dicom->window == -1) {
            st->codecpar->profile = conv_IS(de);
            dicom->window = st->codecpar->profile;
        }
        break;
    case 0x0120: // pixel padding
        dicom->pixpad = AV_RL16(bytes);
        break;
    case 0x0004: // photometric interpret
        cbytes = av_malloc(len + 1);
        memcpy(cbytes, bytes, len);
        cbytes[len] = 0;
        dicom->interpret = find_PI(cbytes);
        av_free(cbytes);
        break;
    case 0x0103: // pix rep
        dicom->pixrep = AV_RL16(bytes);
        break;
    case 0x1053: // rescale slope
        dicom->slope = conv_IS(de);
        break;
    case 0x1052: // rescale intercept
        dicom->intcpt = conv_IS(de);
        break;
    }
    return 0;
}

static int set_multiframe_data(AVFormatContext *s, AVStream* st, DataElement *de)
{
    DICOMContext *dicom = s->priv_data;

    if (de->GroupNumber != MF_GR_NB)
        return 0;

    switch (de->ElementNumber) {
    case 0x1063: // frame time
        dicom->delay = conv_DS(de);
        dicom->duration = dicom->delay * dicom->nb_frames;
        break;
    }
    return 0;
}


static int read_de_metainfo(AVFormatContext *s, DataElement *de)
{
    DICOMContext *dicom = s->priv_data;
    ValueRepresentation vr;
    int ret;
    int bytes_read = 0;

    ret = avio_rl16(s->pb);
    if (ret < 0)
        return ret;
    de->GroupNumber = ret;
    de->ElementNumber =  avio_rl16(s->pb);
    if (dicom->inseq || (dicom->transfer_syntax == IMPLICIT_VR && !dicom->inheader)) {
        de->VL = avio_rl32(s->pb);
        bytes_read += 8;
        return bytes_read;
    }
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
        break;
    default:
        de->VL = avio_rl16(s->pb);
        bytes_read += 2;
        break;
    }
    if (de->VL < 0)
        return AVERROR_INVALIDDATA;
    if (de->VL != UNDEFINED_VL && de->VL % 2)
        av_log(s, AV_LOG_WARNING,"Data Element Value length: %"PRIi64" can't be odd\n", de->VL);
    return bytes_read;
}

static int read_de(AVFormatContext *s, DataElement *de)
{
    int ret;
    uint32_t len = de->VL;
    de->bytes = av_malloc(len);
    ret = avio_read(s->pb, de->bytes, len);
    return ret;
}

static int read_implicit_seq_item(AVFormatContext *s, DataElement *de)
{
    int ret, f = -2, i = 0;
    uint8_t *bytes = de->bytes;

    bytes = av_malloc(MAX_UNDEF_LENGTH);
    for(; i < MAX_UNDEF_LENGTH; ++i) {
        ret = avio_rl16(s->pb);
        if (ret < 0)
            return ret;

        if (ret == SEQ_GR_NB)
            f = i;
        else if (ret == SEQ_ITEM_DEL_EL_NB && f == i - 1) {
            avio_skip(s->pb, 4);
            break;
        }
        bytestream_put_le16(&bytes, ret);
    }
    de->VL = (i - 1) * 2;
    return 0;
}

static int read_seq(AVFormatContext *s, DataElement *de) {
    int i = 0, ret;
    DICOMContext *dicom = s->priv_data;
    DataElement *seq_data = av_malloc_array(MAX_SEQ_LENGTH, sizeof(DataElement));

    de->bytes = seq_data;
    dicom->inseq = 1;
    for (;i < MAX_SEQ_LENGTH; ++i) {
        seq_data[i].bytes = NULL;
        seq_data[i].desc = NULL;
        seq_data[i].is_found = 0;
        read_de_metainfo(s, seq_data + i);
        if (seq_data[i].GroupNumber == SEQ_GR_NB
            && seq_data[i].ElementNumber == SEQ_DEL_EL_NB) {
            ret = i;
            break;
        }
        if (seq_data[i].VL == UNDEFINED_VL)
            ret = read_implicit_seq_item(s, seq_data + i);
        else
            ret = read_de(s, seq_data + i);
        if (ret < 0)
            break;
    }

    dicom->inseq = 0;
    return ret;
}

static int read_de_valuefield(AVFormatContext *s, DataElement *de) {
    if (de->VL == UNDEFINED_VL)
        return read_seq(s, de);
    else
        return read_de(s, de);
}

static int dicom_read_header(AVFormatContext *s)
{
    AVIOContext  *pb = s->pb;
    AVDictionary **m = &s->metadata;
    DICOMContext *dicom = s->priv_data;
    DataElement *de;
    char *key, *value;
    uint32_t header_size, bytes_read = 0;
    int ret;

    ret = avio_skip(pb, DICOM_PREAMBLE_SIZE + DICOM_PREFIX_SIZE);
    if (ret < 0)
        return ret;
    dicom->inheader = 1;
    de = alloc_de();
    if (!de)
        return AVERROR(ENOMEM);
    ret = read_de_metainfo(s, de);
    if (ret < 0) {
        free_de(de);
        return ret;
    }

    ret = read_de_valuefield(s, de);
    if (ret < 0) {
        free_de(de);
        return ret;
    }
    if (de->GroupNumber != 0x02 || de->ElementNumber != 0x00) {
        av_log(s, AV_LOG_WARNING, "First data element is not File MetaInfo Group Length, may fail to demux\n");
        header_size = 200; // Fallback to default length
    } else
        header_size = AV_RL32(de->bytes);

    free_de(de);
    while (bytes_read < header_size) {
        de = alloc_de();
        ret = read_de_metainfo(s, de);
        if (ret < 0) {
            free_de(de);
            return ret;
        }
        bytes_read += ret;
        dicom_dict_find_elem_info(de);
        key = get_key_str(de);
        ret = read_de_valuefield(s, de);
        if (ret < 0) {
            free_de(de);
            return ret;
        }
        bytes_read += ret;
        value = get_val_str(de);
        if (de->GroupNumber == TS_GR_NB && de->ElementNumber == TS_EL_NB) {
            dicom->transfer_syntax = get_transfer_sytax(value);
            if (dicom->transfer_syntax == UNSUPPORTED_TS) {
                free_de(de);
                av_log(s, AV_LOG_ERROR, "Provided Transfer syntax is not supported\n");
                return AVERROR_INVALIDDATA;
            }
        }

        av_dict_set(m, key, value, AV_DICT_DONT_STRDUP_KEY | AV_DICT_DONT_STRDUP_VAL);
        free_de(de);
    }
    set_context_defaults(dicom);
    s->ctx_flags |= AVFMTCTX_NOHEADER;
    s->start_time = 0;
    return 0;
}

static int dicom_read_packet(AVFormatContext *s, AVPacket *pkt)
{
    DICOMContext *dicom = s->priv_data;
    int metadata = dicom->metadata, ret;
    AVDictionary **st_md;
    char *key, *value;
    AVStream  *st;
    DataElement *de;

    if (s->nb_streams < 1) {
        st = avformat_new_stream(s, NULL);
        if (!st)
            return AVERROR(ENOMEM);
        st->codecpar->codec_id   = AV_CODEC_ID_DICOM;
        st->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
        st->start_time = 0;
        avpriv_set_pts_info(st, 64, 1, 1000);
        if (dicom->window != -1)
            st->codecpar->profile = dicom->window;
        if (dicom->level != -1)
            st->codecpar->level = dicom->level;
    } else
        st = s->streams[0];

    st_md = &st->metadata;
    dicom->inheader = 0;
    if (dicom->frame_nb > 1 && dicom->frame_nb <= dicom->nb_frames)
        goto inside_pix_data;

    for (;;) {
        ret = avio_feof(s->pb);
        if (ret)
            return AVERROR_EOF;

        de = alloc_de();
        if (!de)
            return AVERROR(ENOMEM);

        ret = read_de_metainfo(s,de);
        if (ret < 0)
            break;

        if (de->GroupNumber == PIXEL_GR_NB && de->ElementNumber == PIXELDATA_EL_NB) {
            dicom->frame_size = de->VL / dicom->nb_frames;
            free_de(de);
            set_dec_extradata(s, st);
        inside_pix_data:
            if (av_new_packet(pkt, dicom->frame_size) < 0)
                return AVERROR(ENOMEM);
            pkt->pos = avio_tell(s->pb);
            pkt->stream_index = 0;
            pkt->size = dicom->frame_size;
            pkt->duration = dicom->delay;
            ret = avio_read(s->pb, pkt->data, dicom->frame_size);
            if (ret < 0) {
                av_packet_unref(pkt);
                break;
            }
            dicom->frame_nb ++;
            return ret;
        } else if (de->GroupNumber == IMAGE_GR_NB || de->GroupNumber == MF_GR_NB) {
            ret = read_de_valuefield(s, de);
            if (ret < 0)
                break;
            set_imagegroup_data(s, st, de);
            set_multiframe_data(s, st, de);
        } else {
            if (metadata || de->VL == UNDEFINED_VL)
                ret = read_de_valuefield(s, de);
            else
                ret = avio_skip(s->pb, de->VL); // skip other elements
            if (ret < 0)
                break;
        }
        if (metadata) {
            ret = dicom_dict_find_elem_info(de);
            if (ret < 0)
                goto end_de;
            key = get_key_str(de);
            value = get_val_str(de);
            av_dict_set(st_md, key, value, AV_DICT_DONT_STRDUP_KEY | AV_DICT_DONT_STRDUP_VAL);
        }
    end_de:
        free_de(de);
    }
    free_de(de);
    if (ret < 0)
        return ret;
    return AVERROR_EOF;
}

static const AVOption options[] = {
    { "window"  , "Override default window found in file" , offsetof(DICOMContext, window), AV_OPT_TYPE_INT, {.i64 = -1}, -1, 99999, AV_OPT_FLAG_DECODING_PARAM },
    { "level"   , "Override default level found in file"  , offsetof(DICOMContext, level), AV_OPT_TYPE_INT, {.i64 = -1}, -1, 99999, AV_OPT_FLAG_DECODING_PARAM },
    { "metadata", "Set true to decode metadata (info about the patient, medical procedure)", offsetof(DICOMContext, metadata), AV_OPT_TYPE_BOOL,{.i64 = 0}, 0, 1, AV_OPT_FLAG_DECODING_PARAM },
    { NULL },
};

static const AVClass dicom_class = {
    .class_name = "DICOM demuxer",
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
