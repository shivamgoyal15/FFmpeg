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
#include "avcodec.h"
#include "bytestream.h"
#include "bmp.h"
#include "internal.h"


static uint8_t apply_window_level(int64_t val, int window, int level, int bitmask, int type)
{
    int max, min;
    uint8_t ret;

    max = level + window / 2;
    min = level - window / 2;
    if (val > max && type == 2)
        ret = 255;
    else if (val < min && type == 2)
        ret = 0;
    else
        ret = ((val & bitmask) - min) * 255 / (max - min);
    if (type == 0x01) // Monochrome1
        ret = 255 - ret;
    return ret;
}

// DICOM MONOCHROME1 and MONOCHROME2 decoder
static int decode_mono( AVCodecContext *avctx, 
                        const uint8_t *buf,
                        uint8_t *out)
{
    int bitmask, window, level, bitsalloc, pixrep, type = 0x02;
    uint64_t i, size;
    int64_t pix;

    window = avctx->profile;
    level = avctx->level;
    size = avctx->width * avctx->height;
    bitsalloc = avctx->bits_per_raw_sample;
    bitmask = (1 << avctx->bits_per_coded_sample) - 1;
    pixrep = 1;

    switch (bitsalloc) {
        case 1:
        case 8:
            for (i = 0; i < size; i++) {
                if (pixrep)
                    pix = (int8_t)bytestream_get_byte(&buf);
                else
                    pix = (uint8_t)bytestream_get_byte(&buf);
                out[i] = apply_window_level(pix, window, level, bitmask, type);
            }
            break;
        case 16:
            for (i = 0; i < size; i++) {
                if (pixrep)
                    pix = (int16_t)bytestream_get_le16(&buf);
                else
                    pix = (uint16_t)bytestream_get_le16(&buf);
                out[i] = apply_window_level(pix, window, level, bitmask, type);
            }
            break;
        case 32:
            for (i = 0; i < size; i++) {
                if (pixrep)
                    pix = (int32_t)bytestream_get_le32(&buf);
                else
                    pix = (uint32_t)bytestream_get_le32(&buf);
                out[i] = apply_window_level(pix, window, level, bitmask, type);
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
    const uint8_t *buf = avpkt->data;
    int buf_size       = avpkt->size;
    AVFrame *p         = data;
    int ret, req_buf_size, photo_interpret = 0x01;
    uint8_t *ptr;

    avctx->pix_fmt = AV_PIX_FMT_GRAY8;
    if ((ret = ff_get_buffer(avctx, p, 0)) < 0)
        return ret;
    p->pict_type = AV_PICTURE_TYPE_I;
    p->key_frame = 1;
    req_buf_size = avctx->width * avctx->height * avctx->bits_per_raw_sample / 8;
    ptr = p->data[0];
    if (buf_size < req_buf_size) {
        av_log(avctx, AV_LOG_ERROR, "Required buffer size is %d but recieved only %d\n", req_buf_size, buf_size);
        return AVERROR_INVALIDDATA;
    }
    switch (photo_interpret) {
        case 0x01: // MONOCHROME1
        case 0x02: // MONOCHROME2
            ret = decode_mono(avctx, buf, ptr);
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
    .id             = AV_CODEC_ID_DICOM,
    .decode         = dicom_decode_frame,
};
