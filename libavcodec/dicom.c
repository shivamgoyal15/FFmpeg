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

#include "avcodec.h"
#include "bytestream.h"
#include "bmp.h"
#include "internal.h"


static void apply_window_level(short * vals, uint8_t *ptr, int window, int level, int bitmask, int size)
{
    int i, max, min;
    short val;

    max = level + window / 2;
    min = level - window / 2;

    for (i = 0; i < size; i++){
        val = vals[i];

        if (val > max) {
            ptr[i] = 255;
        } else if (val < min) {
            ptr[i] = 0;
        } else {
            ptr[i] = ((val & bitmask) - min) * 255 / (max - min);
        }
    }
    return;

}

static int dicom_decode_frame(AVCodecContext *avctx,
                            void *data, int *got_frame,
                            AVPacket *avpkt)
{
    const uint8_t *buf = avpkt->data;
    int buf_size       = avpkt->size;
    AVFrame *p         = data;
    short *vals;
    int width;
    int height;
    uint8_t *ptr;
    int ret, bitmask, window, level;

    width = avctx->width;
    height = avctx->height;
    window = avctx->profile;
    level = avctx->level;
    avctx->pix_fmt = AV_PIX_FMT_GRAY8;
    if ((ret = ff_get_buffer(avctx, p, 0)) < 0)
        return ret;
    p->pict_type = AV_PICTURE_TYPE_I;
    p->key_frame = 1;

    vals = (short *) buf;
    bitmask = (1 << 16) - 1;

    ptr      = p->data[0];

    apply_window_level(vals, ptr, window, level, bitmask, width * height);

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
