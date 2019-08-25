/*
 * DICOM decoder
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

#include <inttypes.h>
#include "libavutil/dict.h"
#include "libavformat/dicom.h"
#include "avcodec.h"
#include "bytestream.h"
#include "internal.h"

typedef struct DICOMopts {
    const AVClass *class;
    int interpret; // streams extradata
    int pixrep;
    int pixpad;
    int slope;
    int intcpt;
} DICOMopts;


static int extract_ed(AVCodecContext *avctx)
{
    DICOMopts *dcmopts =  avctx->priv_data;
    uint8_t *ed = avctx->extradata;
    int ed_size = avctx->extradata_size;
    const uint8_t **b = &ed;

    dcmopts->interpret = 0x02; // Set defaults
    dcmopts->slope = 1;
    dcmopts->intcpt = 0;
    dcmopts->pixpad = 1 << 31;
    dcmopts->pixrep = 0;

    if (ed_size < DECODER_ED_SIZE + AV_INPUT_BUFFER_PADDING_SIZE)
        return -1;
    dcmopts->interpret = bytestream_get_le32(b);
    dcmopts->pixrep = bytestream_get_le32(b);
    dcmopts->pixpad = bytestream_get_le32(b);
    dcmopts->slope = bytestream_get_le32(b);
    dcmopts->intcpt = bytestream_get_le32(b);
    return 0;
}

static uint8_t apply_transform(int64_t val, int64_t bitmask, int pix_pad,
                             int slope, int intercept, int w, int l, int interpret)
{
    int max, min;
    uint8_t ret;

    if (val == pix_pad)
        return 0;
    if (val > 0)
        val = (val & bitmask);
    val = slope * val + intercept; // Linear Transformation

    max = l + w / 2 - 1;
    min = l - w / 2;
    if (val > max)
        ret = 255;
    else if (val <= min)
        ret = 0;
    else
        ret = (val - min) * 255 / (max - min);
    if (interpret == 0x01) // Monochrome1
        ret = 255 - ret;
    return ret;
}

// DICOM MONOCHROME1 and MONOCHROME2 decoder
static int decode_mono( AVCodecContext *avctx,
                        const uint8_t *buf,
                        AVFrame *p)
{
    int w, l, bitsalloc, pixrep, pixpad, slope, intcpt, interpret, ret;
    DICOMopts *dcmopts = avctx->priv_data;
    uint8_t *out;
    int64_t bitmask, pix;
    uint64_t i, size;

    avctx->pix_fmt = AV_PIX_FMT_GRAY8;
    if ((ret = ff_get_buffer(avctx, p, 0)) < 0)
        return ret;
    p->pict_type = AV_PICTURE_TYPE_I;
    p->key_frame = 1;
    out = p->data[0];

    w = avctx->profile;
    l = avctx->level;
    size = avctx->width * avctx->height;
    bitsalloc = avctx->bits_per_raw_sample;
    bitmask = (1 << avctx->bits_per_coded_sample) - 1;
    interpret = dcmopts->interpret;
    pixrep = dcmopts->pixrep;
    pixpad = dcmopts->pixpad;
    slope = dcmopts->slope;
    intcpt = dcmopts->intcpt;

    switch (bitsalloc) {
    case 8:
        for (i = 0; i < size; i++) {
            pix = bytestream_get_byte(&buf);
            pix = pixrep ? pix : FFABS(pix);
            out[i] = pix;
        }
        break;
    case 16:
        for (i = 0; i < size; i++) {
            if (pixrep)
                pix = (int16_t)bytestream_get_le16(&buf);
            else
                pix = (uint16_t)bytestream_get_le16(&buf);
            out[i] = apply_transform(pix, bitmask, pixpad, slope, intcpt, w, l, interpret);
        }
        break;
    case 32:
        for (i = 0; i < size; i++) {
            if (pixrep)
                pix = (int32_t)bytestream_get_le32(&buf);
            else
                pix = (uint32_t)bytestream_get_le32(&buf);
            out[i] = apply_transform(pix, bitmask, pixpad, slope, intcpt, w, l, interpret);
        }
        break;
    default:
        av_log(avctx, AV_LOG_ERROR, "Bits allocated %d not supported\n", bitsalloc);
        return AVERROR_INVALIDDATA;
    }
    return 0;
}

static int dicom_decode_frame(AVCodecContext *avctx,
                            void *data, int *got_frame,
                            AVPacket *avpkt)
{
    DICOMopts *dcmopts = avctx->priv_data;
    const uint8_t *buf = avpkt->data;
    int buf_size       = avpkt->size;
    AVFrame *p         = data;
    int ret, req_buf_size;

    req_buf_size = avctx->width * avctx->height * avctx->bits_per_raw_sample / 8;
    if (buf_size < req_buf_size) {
        av_log(avctx, AV_LOG_ERROR, "Required buffer size is %d but received only %d\n", req_buf_size, buf_size);
        return AVERROR_INVALIDDATA;
    }
    extract_ed(avctx);
    switch (dcmopts->interpret) {
    case 0x01: // MONOCHROME1
    case 0x02: // MONOCHROME2
        ret = decode_mono(avctx, buf, p);
        if (ret < 0)
            return ret;
        break;
    default:
        av_log(avctx, AV_LOG_ERROR, "Provided photometric interpretation not supported\n");
        return AVERROR_INVALIDDATA;
    }
    *got_frame = 1;
    return buf_size;
}

AVCodec ff_dicom_decoder = {
    .name           = "dicom",
    .long_name      = NULL_IF_CONFIG_SMALL("DICOM (Digital Imaging and Communications in Medicine)"),
    .type           = AVMEDIA_TYPE_VIDEO,
    .priv_data_size = sizeof(DICOMopts),
    .id             = AV_CODEC_ID_DICOM,
    .decode         = dicom_decode_frame,
};
