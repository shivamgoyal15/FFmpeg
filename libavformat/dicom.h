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


#define DICOM_PREAMBLE_SIZE 128
#define DICOM_PREFIX_SIZE   4

#define IMAGE_GR_NB     0x0028
#define MF_GR_NB        0x0018
#define PIXEL_GR_NB     0x7FE0
#define PIXELDATA_EL_NB 0x0010
#define TS_GR_NB        0x0002
#define TS_EL_NB        0x0010
#define UNDEFINED_VL    0xFFFFFFFF
#define DEFAULT_WINDOW  1100
#define DEFAULT_LEVEL   125
#define DECODER_ED_SIZE 20

#define SEQ_GR_NB           0xFFFE
#define SEQ_DEL_EL_NB       0xE0DD
#define SEQ_ITEM_BEG_EL_NB  0xE000
#define SEQ_ITEM_DEL_EL_NB  0xE00D
#define MAX_UNDEF_LENGTH    5000   // max undefined length
#define MAX_SEQ_LENGTH      20     // max sequence length (items)

typedef enum {
    UNSUPPORTED_TS = 0,
    IMPLICIT_VR = 1,
    EXPLICIT_VR = 2,
    DEFLATE_EXPLICIT_VR = 3,
    JPEG_BASE_8 = 4,
    JPEG_EXT_12 = 5,
    JPEG_LOSSLESS_NH_P14 = 6,
    JPEG_LOSSLESS_NH_P14_S1 = 7,
    JPEG_LS_LOSSLESS = 8,
    JPEG_LS_LOSSY = 9,
    JPEG2000_LOSSLESS = 10,
    JPEG2000 = 11,
    RLE = 12
} TransferSyntax;

typedef enum {
    AE = 0x4145,
    AS = 0x4153,
    AT = 0x4154,
    CS = 0x4353,
    DA = 0x4441,
    DS = 0x4453,
    DT = 0x4454,
    FD = 0x4644,
    FL = 0x464c,
    IS = 0x4953,
    LO = 0x4c4f,
    LT = 0x4c54,
    OB = 0x4f42,
    OD = 0x4f44,
    OF = 0x4f46,
    OL = 0x4f4c,
    OV = 0x4f56,
    OW = 0x4f57,
    PN = 0x504e,
    SH = 0x5348,
    SL = 0x534c,
    SQ = 0x5351,
    SS = 0x5353,
    ST = 0x5354,
    SV = 0x5356,
    TM = 0x544d,
    UC = 0x5543,
    UI = 0x5549,
    UL = 0x554c,
    UN = 0x554e,
    UR = 0x5552,
    US = 0x5553,
    UT = 0x5554,
    UV = 0x5556
} ValueRepresentation;


typedef struct DataElement {
    uint16_t GroupNumber;
    uint16_t ElementNumber;
    ValueRepresentation VR;
    int64_t VL;
    void *bytes;
    int is_found; // is found in DICOM dictionary
    char *desc; // description (from dicom dictionary)
} DataElement;

int dicom_dict_find_elem_info (DataElement *de);
