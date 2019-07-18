/*
 * DICOM Dictionary
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
#include "dicom.h"

typedef struct DICOMDictionary {
    uint16_t GroupNumber;
    uint16_t ElementNumber;
    ValueRepresentation vr;
    char *desc;
} DICOMDictionary;

DICOMDictionary dicom_dictionary[] = {
    {0x0002, 0x0000, UL, "File Meta Elements Group Len"},
    {0x0002, 0x0001, OB, "File Meta Information Version"},
    {0x0002, 0x0002, UI, "Media Storage SOP Class UID"},
    {0x0002, 0x0003, UI, "Media Storage SOP Inst UID"},
    {0x0002, 0x0010, UI, "Transfer Syntax UID"},
    {0x0002, 0x0012, UI, "Implementation Class UID"},
    {0x0002, 0x0013, SH, "Implementation Version Name"},
    {0x0002, 0x0016, AE, "Source Application Entity Title"},
    {0x0002, 0x0017, AE, "Sending Application Entity Title"},
    {0x0002, 0x0018, AE, "Receiving Application Entity Title"},
    {0x0002, 0x0100, UI, "Private Information Creator UID"},
    {0x0002, 0x0102, OB, "Private Information"},


    {0x0004, 0x1130, CS, "File-set ID"},
    {0x0004, 0x1141, CS, "File-set Descriptor File ID"},
    {0x0004, 0x1142, CS, "Specific Character Set of File-set Descriptor File"},
    {0x0004, 0x1200, UL, "Offset of the First Directory Record of the Root Directory Entity"},
    {0x0004, 0x1202, UL, "Offset of the Last Directory Record of the Root Directory Entity"},
    {0x0004, 0x1212, US, "File-set Consistency Flag"},
    {0x0004, 0x1220, SQ, "Directory Record Sequence"},
    {0x0004, 0x1400, UL, "Offset of the Next Directory Record"},
    {0x0004, 0x1410, US, "Record In-use Flag"},
    {0x0004, 0x1420, UL, "Offset of Referenced Lower-Level Directory Entity"},
    {0x0004, 0x1430, CS, "Directory Record Type"},
    {0x0004, 0x1432, UI, "Private Record UID"},
    {0x0004, 0x1500, CS, "Referenced File ID"},
    {0x0004, 0x1504, UL, "MRDR Directory Record Offset"},
    {0x0004, 0x1510, UI, "Referenced SOP Class UID in File"},
    {0x0004, 0x1511, UI, "Referenced SOP Instance UID in File"},
    {0x0004, 0x1512, UI, "Referenced Transfer Syntax UID in File"},
    {0x0004, 0x151A, UI, "Referenced Related General SOP Class UID in File"},
    {0x0004, 0x1600, UL, "Number of References"},


    {0x0008, 0x0001, UL, "Length to End"},
    {0x0008, 0x0005, CS, "Specific Character Set"},
    {0x0008, 0x0006, SQ, "Language Code Sequence"},
    {0x0008, 0x0008, CS, "Image Type"},
    {0x0008, 0x0010, SH, "Recognition Code"},
    {0x0008, 0x0012, DA, "Instance Creation Date"},
    {0x0008, 0x0013, TM, "Instance Creation Time"},
    {0x0008, 0x0014, UI, "Instance Creator UID"},
    {0x0008, 0x0015, DT, "Instance Create UID"},
    {0x0008, 0x0016, UI, "SOP Class UID"},
    {0x0008, 0x0018, UI, "SOP Instance UID"},
    {0x0008, 0x001A, UI, "Related General SOP Class UID"},
    {0x0008, 0x001B, UI, "Original Specialized SOP Class UID"},
    {0x0008, 0x0020, DA, "Study Date"},
    {0x0008, 0x0021, DA, "Series Date"},
    {0x0008, 0x0022, DA, "Acquisition Date"},
    {0x0008, 0x0023, DA, "Content Date"},
    {0x0008, 0x0024, DA, "Overlay Date"},
    {0x0008, 0x0025, DA, "Curve Date"},
    {0x0008, 0x002A, DT, "Acquisition DateTime"},
    {0x0008, 0x0030, TM, "Study Time"},
    {0x0008, 0x0031, TM, "Series Time"},
    {0x0008, 0x0032, TM, "Acquisition Time"},
    {0x0008, 0x0033, TM, "Content Time"},
    {0x0008, 0x0034, TM, "Overlay Time"},
    {0x0008, 0x0035, TM, "Curve Time"},
    {0x0008, 0x0040, US, "Data Set Type"},
    {0x0008, 0x0041, LO, "Data Set Subtype"},
    {0x0008, 0x0042, CS, "Nuclear Medicine Series Type"},
    {0x0008, 0x0050, SH, "Accession Number"},
    {0x0008, 0x0051, SQ, "Issuer of Accession Number Sequence"},
    {0x0008, 0x0052, CS, "Query/Retrieve Level"},
    {0x0008, 0x0053, CS, "Query/Retrieve View"},
    {0x0008, 0x0054, AE, "Retrieve AE Title"},
    {0x0008, 0x0055, AE, "Station AE Title"},
    {0x0008, 0x0056, CS, "Instance Availability"},
    {0x0008, 0x0058, UI, "Failed SOP Instance UID List"},
    {0x0008, 0x0060, CS, "Modality"},
    {0x0008, 0x0061, CS, "Modalities in Study"},
    {0x0008, 0x0062, UI, "SOP Classes in Study"},
    {0x0008, 0x0064, CS, "Conversion Type"},
    {0x0008, 0x0068, CS, "Presentation Intent Type"},
    {0x0008, 0x0070, LO, "Manufacturer"},
    {0x0008, 0x0080, LO, "Institution Name"},
    {0x0008, 0x0081, ST, "Institution Address"},
    {0x0008, 0x0082, SQ, "Institution Code Sequence"},
    {0x0008, 0x0090, PN, "Referring Physician's Name"},
    {0x0008, 0x0092, ST, "Referring Physician's Address"},
    {0x0008, 0x0094, SH, "Referring Physician's Telephone Numbers"},
    {0x0008, 0x0096, SQ, "Referring Physician Identification Sequence"},
    {0x0008, 0x009C, PN, "Consulting Physician's Name"},
    {0x0008, 0x009D, SQ, "Consulting Physician Identification Sequence"},
    {0x0008, 0x0100, SH, "Code Value"},
    {0x0008, 0x0101, LO, "Extended Code Value"},
    {0x0008, 0x0102, SH, "Coding Scheme Designator"},
    {0x0008, 0x0104, LO, "Code Meaning"},
    {0x0008, 0x0105, CS, "Mapping Resource"},
    {0x0008, 0x0106, DT, "Context Group Version"},
    {0x0008, 0x0107, DT, "Context Group Local Version"},
    {0x0008, 0x0108, LT, "Extended Code Meaning"},
    {0x0008, 0x010C, UI, "Coding Scheme UID"},
    {0x0008, 0x010D, UI, "Context Group Extension Creator UID"},
    {0x0008, 0x010F, CS, "Context Identifier"},
    {0x0008, 0x0110, SQ, "Coding Scheme Identification Sequence"},
    {0x0008, 0x0112, LO, "Coding Scheme Registry"},
    {0x0008, 0x0114, ST, "Coding Scheme External ID"},
    {0x0008, 0x0115, ST, "Coding Scheme Name"},
    {0x0008, 0x0116, ST, "Coding Scheme Responsible Organization"},
    {0x0008, 0x0117, UI, "Context UID"},
    {0x0008, 0x0118, UI, "Mapping Resource UID"},
    {0x0008, 0x0119, UC, "Long Code Value"},
    {0x0008, 0x0120, UR, "URN Code Value"},
    {0x0008, 0x0121, SQ, "Equivalent Code Sequence"},
    {0x0008, 0x0122, LO, "Mapping Resource Name"},
    {0x0008, 0x0123, SQ, "Context Group Identification Sequence"},
    {0x0008, 0x0124, SQ, "Mapping Resource Identification Sequence"},
    {0x0008, 0x0201, SH, "Timezone Offset From UTC"},
    {0x0008, 0x0300, SQ, "Private Data Element Characteristics Sequence"},
    {0x0008, 0x0301, US, "Private Group Reference"},
    {0x0008, 0x0302, LO, "Private Creator Reference"},
    {0x0008, 0x0303, CS, "Block Identifying Information Status"},
    {0x0008, 0x0304, US, "Nonidentifying PrivateElements"},
    {0x0008, 0x0305, SQ, "Deidentification ActionSequence"},
    {0x0008, 0x0306, US, "Identifying PrivateElements"},
    {0x0008, 0x0307, CS, "Deidentification Action"},
    {0x0008, 0x1000, AE, "Network ID"},
    {0x0008, 0x1010, SH, "Station Name"},
    {0x0008, 0x1030, LO, "Study Description"},
    {0x0008, 0x1032, SQ, "Procedure Code Sequence"},
    {0x0008, 0x103E, LO, "Series Description"},
    {0x0008, 0x103F, SQ, "Series Description CodeSequence"},
    {0x0008, 0x1040, LO, "Institutional Department Name"},
    {0x0008, 0x1048, PN, "Physician(s) of Record"},
    {0x0008, 0x1049, SQ, "Physician(s) of Record Identification Sequence"},
    {0x0008, 0x1050, PN, "Attending Physician's Name"},
    {0x0008, 0x1052, SQ, "Performing Physician Identification Sequence"},
    {0x0008, 0x1060, PN, "Name of Physician(s) Reading Study"},
    {0x0008, 0x1062, SQ, "Physician(s) ReadingStudy Identification Sequenc"},
    {0x0008, 0x1070, PN, "Operator's Name"},
    {0x0008, 0x1072, SQ, "Operator Identification Sequence"},
    {0x0008, 0x1080, LO, "Admitting Diagnosis Description"},
    {0x0008, 0x1084, SQ, "Admitting Diagnosis Code Sequence"},
    {0x0008, 0x1090, LO, "Manufacturer's Model Name"},
    {0x0008, 0x1100, SQ, "Referenced Results Sequence"},
    {0x0008, 0x1110, SQ, "Referenced Study Sequence"},
    {0x0008, 0x1111, SQ, "Referenced Study Component Sequence"},
    {0x0008, 0x1115, SQ, "Referenced Series Sequence"},
    {0x0008, 0x1120, SQ, "Referenced Patient Sequence"},
    {0x0008, 0x1125, SQ, "Referenced Visit Sequence"},
    {0x0008, 0x1130, SQ, "Referenced Overlay Sequence"},
    {0x0008, 0x1134, SQ, "Referenced Stereometric Instance Sequence"},
    {0x0008, 0x113A, SQ, "Referenced Waveform Sequence"},
    {0x0008, 0x1140, SQ, "Referenced Image Sequence"},
    {0x0008, 0x1145, SQ, "Referenced Curve Sequence"},
    {0x0008, 0x114A, SQ, "Referenced InstanceSequence"},
    {0x0008, 0x114B, SQ, "Referenced Real World Value Mapping InstanceSequence"},
    {0x0008, 0x1150, UI, "Referenced SOP Class UID"},
    {0x0008, 0x1155, UI, "Referenced SOP Instance UID"},
    {0x0008, 0x115A, UI, "SOP Classes Supported"},
    {0x0008, 0x1160, IS, "Referenced Frame Number"},
    {0x0008, 0x1161, UL, "Simple Frame List"},
    {0x0008, 0x1162, UL, "Calculated Frame List"},
    {0x0008, 0x1163, FD, "Time Range"},
    {0x0008, 0x1164, SQ, "Frame Extraction Sequence"},
    {0x0008, 0x1167, UI, "Multi-frame Source SOP Instance UID"},
    {0x0008, 0x1190, UR, "Retrieve URL"},
    {0x0008, 0x1195, UI, "Transaction UID"},
    {0x0008, 0x1196, US, "Warning Reason"},
    {0x0008, 0x1197, US, "Failure Reason"},
    {0x0008, 0x1198, SQ, "Failed SOP Sequence"},
    {0x0008, 0x1199, SQ, "Referenced SOP Sequence"},
    {0x0008, 0x119A, SQ, "Other Failures Sequence"},
    {0x0008, 0x1200, SQ, "Studies Containing OtherReferenced InstancesSequence"},
    {0x0008, 0x1250, SQ, "Related Series Sequence"},
    {0x0008, 0x2110, CS, "Lossy Image Compression(Retired)"},
    {0x0008, 0x2111, ST, "Derivation Description"},
    {0x0008, 0x2112, SQ, "Source Image Sequence"},
    {0x0008, 0x2120, SH, "Stage Name"},
    {0x0008, 0x2122, IS, "Stage Number"},
    {0x0008, 0x2124, IS, "Number of Stages"},
    {0x0008, 0x2127, SH, "View Name"},
    {0x0008, 0x2128, IS, "View Number"},
    {0x0008, 0x2129, IS, "Number of Event Timers"},
    {0x0008, 0x212A, IS, "Number of Views in Stage"},
    {0x0008, 0x2130, DS, "Event Elapsed Time(s)"},
    {0x0008, 0x2132, LO, "Event Timer Name(s)"},
    {0x0008, 0x2133, SQ, "Event Timer Sequence"},
    {0x0008, 0x2134, FD, "Event Time Offset"},
    {0x0008, 0x2135, SQ, "Event Code Sequence"},
    {0x0008, 0x2142, IS, "Start Trim"},
    {0x0008, 0x2143, IS, "Stop Trim"},
    {0x0008, 0x2144, IS, "Recommended Display Frame Rate"},
    {0x0008, 0x2200, CS, "Transducer Position"},
    {0x0008, 0x2204, CS, "Transducer Orientation"},
    {0x0008, 0x2208, CS, "Anatomic Structure"},
    {0x0008, 0x2218, SQ, "Anatomic RegionSequence"},
    {0x0008, 0x2220, SQ, "Anatomic Region ModifierSequence"},
    {0x0008, 0x2228, SQ, "Primary Anatomic Structure Sequence"},
    {0x0008, 0x2229, SQ, "Anatomic Structure, Spaceor Region Sequence"},
    {0x0008, 0x2230, SQ, "Primary Anatomic Structure ModifierSequence"},
    {0x0008, 0x2240, SQ, "Transducer Position Sequence"},
    {0x0008, 0x2242, SQ, "Transducer Position Modifier Sequence"},
    {0x0008, 0x2244, SQ, "Transducer Orientation Sequence"},
    {0x0008, 0x2246, SQ, "Transducer Orientation Modifier Sequence"},
    {0x0008, 0x2251, SQ, "Anatomic Structure SpaceOr Region Code Sequence(Trial)"},
    {0x0008, 0x2253, SQ, "Anatomic Portal Of Entrance Code Sequence(Trial)"},
    {0x0008, 0x2255, SQ, "Anatomic ApproachDirection Code Sequence(Trial)"},
    {0x0008, 0x2256, ST, "Anatomic Perspective Description (Trial)"},
    {0x0008, 0x2257, SQ, "Anatomic Perspective Code Sequence (Trial)"},
    {0x0008, 0x2258, ST, "Anatomic Location Of Examining InstrumentDescription (Trial)"},
    {0x0008, 0x2259, SQ, "Anatomic Location Of Examining InstrumentCode Sequence (Trial)"},
    {0x0008, 0x225A, SQ, "Anatomic Structure SpaceOr Region Modifier CodeSequence (Trial)"},
    {0x0008, 0x225C, SQ, "On Axis Background Anatomic Structure CodeSequence (Trial)"},
    {0x0008, 0x3001, SQ, "Alternate Representation Sequence"},
    {0x0008, 0x3010, UI, "Irradiation Event UID"},
    {0x0008, 0x3011, SQ, "Source Irradiation Event Sequence"},
    {0x0008, 0x2012, UI, "Radiopharmaceutical Administration Event UID"},
    {0x0008, 0x4000, LT, "Identifying Comments"},
    {0x0008, 0x9007, CS, "Frame Type"},
    {0x0008, 0x9092, SQ, "Referenced ImageEvidence Sequence"},
    {0x0008, 0x9121, SQ, "Referenced Raw DataSequence"},
    {0x0008, 0x9123, UI, "Creator-Version UID"},
    {0x0008, 0x9124, SQ, "Derivation ImageSequence"},
    {0x0008, 0x9154, SQ, "Source Image EvidenceSequence"},
    {0x0008, 0x9205, CS, "Pixel Presentation"},
    {0x0008, 0x9206, CS, "Volumetric Properties"},
    {0x0008, 0x9207, CS, "Volume Based Calculation Technique"},
    {0x0008, 0x9208, CS, "Complex Image Component"},
    {0x0008, 0x9209, CS, "Acquisition Contrast"},
    {0x0008, 0x9215, SQ, "Derivation Code Sequence"},
    {0x0008, 0x9237, SQ, "Referenced Presentation State Sequence"},
    {0x0008, 0x9410, SQ, "Referenced Other Plane Sequence"},
    {0x0008, 0x9458, SQ, "Frame Display Sequence"},
    {0x0008, 0x9459, FL, "Recommended DisplayFrame Rate in Float"},
    {0x0008, 0x9460, CS, "Skip Frame Range Flag"},
};

int dicom_dict_find_elem_info(DataElement *de) {
    int i, len;

    if (!de->GroupNumber || !de->ElementNumber)
        return -2;
    len = sizeof(dicom_dictionary) / sizeof(dicom_dictionary[0]);
    for (int i = 0; i < len; i++) {
        if (de->GroupNumber == dicom_dictionary[i].GroupNumber 
            && de->ElementNumber == dicom_dictionary[i].ElementNumber) {
            de->VR = dicom_dictionary[i].vr;
            return 0;
        }
    }
    return -1;
}