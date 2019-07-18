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
#define DICOM_PREFIX_SIZE 4

#define IMAGE_GR_NB 0x0028
#define PIXEL_GR_NB 0x7FE0
#define PIXELDATA_EL_NB 0x0010
#define TRANSFER_SYNTEX_UID_GR_NB 0x0002
#define TRANSFER_SYNTEX_UID_EL_NB 0x0010
#define DEFAULT_WINDOW 1100
#define DEFAULT_LEVEL 125

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


typedef struct DataElement{
    uint16_t GroupNumber;
    uint16_t ElementNumber;
    ValueRepresentation VR;
    uint32_t VL;
    void* bytes;
    int index;  // Index in dicom dictionary
    char* desc;
} DataElement;

int dicom_dict_find_elem_info (DataElement *de);
