/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2021-2023 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 **/


#pragma once

#include <string>
#include <set>
#include <stdint.h>

#include "../Compatibility.h"
#include "../Enumerations.h"

namespace Orthanc
{
  class ORTHANC_PUBLIC DicomTag
  {
    // This must stay a POD (plain old data structure) 

  private:
    uint16_t group_;
    uint16_t element_;

  public:
    DicomTag(uint16_t group,
             uint16_t element);

    uint16_t GetGroup() const;

    uint16_t GetElement() const;

    bool IsPrivate() const;

    bool operator< (const DicomTag& other) const;

    bool operator<= (const DicomTag& other) const;

    bool operator> (const DicomTag& other) const;

    bool operator>= (const DicomTag& other) const;

    bool operator== (const DicomTag& other) const;

    bool operator!= (const DicomTag& other) const;

    std::string Format() const;

    std::ostream& FormatStream(std::ostream& o) const;

    static bool ParseHexadecimal(DicomTag& tag,
                                 const char* value);

    static void AddTagsForModule(std::set<DicomTag>& target,
                                 DicomModule module);

#if ORTHANC_BUILDING_FRAMEWORK_LIBRARY == 1
    ORTHANC_PUBLIC ORTHANC_DEPRECATED(friend std::ostream& operator<<(std::ostream& o, const DicomTag& tag));
#endif
  };

  // Aliases for the most useful tags
  static const DicomTag DICOM_TAG_ACCESSION_NUMBER(0x0008, 0x0050);
  static const DicomTag DICOM_TAG_SOP_INSTANCE_UID(0x0008, 0x0018);
  static const DicomTag DICOM_TAG_PATIENT_ID(0x0010, 0x0020);
  static const DicomTag DICOM_TAG_SERIES_INSTANCE_UID(0x0020, 0x000e);
  static const DicomTag DICOM_TAG_STUDY_INSTANCE_UID(0x0020, 0x000d);
  static const DicomTag DICOM_TAG_PIXEL_DATA(0x7fe0, 0x0010);
  static const DicomTag DICOM_TAG_TRANSFER_SYNTAX_UID(0x0002, 0x0010);

  static const DicomTag DICOM_TAG_IMAGE_INDEX(0x0054, 0x1330);
  static const DicomTag DICOM_TAG_INSTANCE_NUMBER(0x0020, 0x0013);

  static const DicomTag DICOM_TAG_NUMBER_OF_SLICES(0x0054, 0x0081);
  static const DicomTag DICOM_TAG_NUMBER_OF_TIME_SLICES(0x0054, 0x0101);
  static const DicomTag DICOM_TAG_NUMBER_OF_FRAMES(0x0028, 0x0008);
  static const DicomTag DICOM_TAG_CARDIAC_NUMBER_OF_IMAGES(0x0018, 0x1090);
  static const DicomTag DICOM_TAG_IMAGES_IN_ACQUISITION(0x0020, 0x1002);
  static const DicomTag DICOM_TAG_PATIENT_NAME(0x0010, 0x0010);
  static const DicomTag DICOM_TAG_ENCAPSULATED_DOCUMENT(0x0042, 0x0011);

  static const DicomTag DICOM_TAG_STUDY_DESCRIPTION(0x0008, 0x1030);
  static const DicomTag DICOM_TAG_SERIES_DESCRIPTION(0x0008, 0x103e);
  static const DicomTag DICOM_TAG_MODALITY(0x0008, 0x0060);

  // The following is used for "modify/anonymize" operations
  static const DicomTag DICOM_TAG_SOP_CLASS_UID(0x0008, 0x0016);
  static const DicomTag DICOM_TAG_MEDIA_STORAGE_SOP_CLASS_UID(0x0002, 0x0002);
  static const DicomTag DICOM_TAG_MEDIA_STORAGE_SOP_INSTANCE_UID(0x0002, 0x0003);
  static const DicomTag DICOM_TAG_DEIDENTIFICATION_METHOD(0x0012, 0x0063);

  // DICOM tags used for fMRI (thanks to Will Ryder)
  static const DicomTag DICOM_TAG_NUMBER_OF_TEMPORAL_POSITIONS(0x0020, 0x0105);
  static const DicomTag DICOM_TAG_TEMPORAL_POSITION_IDENTIFIER(0x0020, 0x0100);

  // Tags for C-FIND and C-MOVE
  static const DicomTag DICOM_TAG_MESSAGE_ID(0x0000, 0x0110);
  static const DicomTag DICOM_TAG_SPECIFIC_CHARACTER_SET(0x0008, 0x0005);
  static const DicomTag DICOM_TAG_QUERY_RETRIEVE_LEVEL(0x0008, 0x0052);
  static const DicomTag DICOM_TAG_MODALITIES_IN_STUDY(0x0008, 0x0061);
  static const DicomTag DICOM_TAG_RETRIEVE_AE_TITLE(0x0008, 0x0054);
  static const DicomTag DICOM_TAG_INSTANCE_AVAILABILITY(0x0008, 0x0056);

  // Tags for images
  static const DicomTag DICOM_TAG_COLUMNS(0x0028, 0x0011);
  static const DicomTag DICOM_TAG_ROWS(0x0028, 0x0010);
  static const DicomTag DICOM_TAG_SAMPLES_PER_PIXEL(0x0028, 0x0002);
  static const DicomTag DICOM_TAG_BITS_ALLOCATED(0x0028, 0x0100);
  static const DicomTag DICOM_TAG_BITS_STORED(0x0028, 0x0101);
  static const DicomTag DICOM_TAG_HIGH_BIT(0x0028, 0x0102);
  static const DicomTag DICOM_TAG_PIXEL_REPRESENTATION(0x0028, 0x0103);
  static const DicomTag DICOM_TAG_PLANAR_CONFIGURATION(0x0028, 0x0006);
  static const DicomTag DICOM_TAG_PHOTOMETRIC_INTERPRETATION(0x0028, 0x0004);
  static const DicomTag DICOM_TAG_IMAGE_ORIENTATION_PATIENT(0x0020, 0x0037);
  static const DicomTag DICOM_TAG_IMAGE_POSITION_PATIENT(0x0020, 0x0032);
  static const DicomTag DICOM_TAG_LARGEST_IMAGE_PIXEL_VALUE(0x0028, 0x0107);
  static const DicomTag DICOM_TAG_SMALLEST_IMAGE_PIXEL_VALUE(0x0028, 0x0106);

  // Tags related to date and time
  static const DicomTag DICOM_TAG_ACQUISITION_DATE(0x0008, 0x0022);
  static const DicomTag DICOM_TAG_ACQUISITION_TIME(0x0008, 0x0032);
  static const DicomTag DICOM_TAG_CONTENT_DATE(0x0008, 0x0023);
  static const DicomTag DICOM_TAG_CONTENT_TIME(0x0008, 0x0033);
  static const DicomTag DICOM_TAG_INSTANCE_CREATION_DATE(0x0008, 0x0012);
  static const DicomTag DICOM_TAG_INSTANCE_CREATION_TIME(0x0008, 0x0013);
  static const DicomTag DICOM_TAG_PATIENT_BIRTH_DATE(0x0010, 0x0030);
  static const DicomTag DICOM_TAG_PATIENT_BIRTH_TIME(0x0010, 0x0032);
  static const DicomTag DICOM_TAG_SERIES_DATE(0x0008, 0x0021);
  static const DicomTag DICOM_TAG_SERIES_TIME(0x0008, 0x0031);
  static const DicomTag DICOM_TAG_STUDY_DATE(0x0008, 0x0020);
  static const DicomTag DICOM_TAG_STUDY_TIME(0x0008, 0x0030);

  // Various tags
  static const DicomTag DICOM_TAG_SERIES_TYPE(0x0054, 0x1000);
  static const DicomTag DICOM_TAG_REQUESTED_PROCEDURE_DESCRIPTION(0x0032, 0x1060);
  static const DicomTag DICOM_TAG_INSTITUTION_NAME(0x0008, 0x0080);
  static const DicomTag DICOM_TAG_REQUESTING_PHYSICIAN(0x0032, 0x1032);
  static const DicomTag DICOM_TAG_REFERRING_PHYSICIAN_NAME(0x0008, 0x0090);
  static const DicomTag DICOM_TAG_OPERATOR_NAME(0x0008, 0x1070);
  static const DicomTag DICOM_TAG_PERFORMED_PROCEDURE_STEP_DESCRIPTION(0x0040, 0x0254);
  static const DicomTag DICOM_TAG_IMAGE_COMMENTS(0x0020, 0x4000);
  static const DicomTag DICOM_TAG_ACQUISITION_DEVICE_PROCESSING_DESCRIPTION(0x0018, 0x1400);
  static const DicomTag DICOM_TAG_ACQUISITION_DEVICE_PROCESSING_CODE(0x0018, 0x1401);
  static const DicomTag DICOM_TAG_CASSETTE_ORIENTATION(0x0018, 0x1402);
  static const DicomTag DICOM_TAG_CASSETTE_SIZE(0x0018, 0x1403);
  static const DicomTag DICOM_TAG_CONTRAST_BOLUS_AGENT(0x0018, 0x0010);
  static const DicomTag DICOM_TAG_STUDY_ID(0x0020, 0x0010);
  static const DicomTag DICOM_TAG_SERIES_NUMBER(0x0020, 0x0011);
  static const DicomTag DICOM_TAG_PATIENT_SEX(0x0010, 0x0040);
  static const DicomTag DICOM_TAG_LATERALITY(0x0020, 0x0060);
  static const DicomTag DICOM_TAG_BODY_PART_EXAMINED(0x0018, 0x0015);
  static const DicomTag DICOM_TAG_SEQUENCE_NAME(0x0018, 0x0024);
  static const DicomTag DICOM_TAG_PROTOCOL_NAME(0x0018, 0x1030);
  static const DicomTag DICOM_TAG_VIEW_POSITION(0x0018, 0x5101);
  static const DicomTag DICOM_TAG_MANUFACTURER(0x0008, 0x0070);
  static const DicomTag DICOM_TAG_STATION_NAME(0x0008, 0x1010);
  static const DicomTag DICOM_TAG_PATIENT_ORIENTATION(0x0020, 0x0020);
  static const DicomTag DICOM_TAG_PATIENT_COMMENTS(0x0010, 0x4000);
  static const DicomTag DICOM_TAG_PATIENT_SPECIES_DESCRIPTION(0x0010, 0x2201);
  static const DicomTag DICOM_TAG_STUDY_COMMENTS(0x0032, 0x4000);
  static const DicomTag DICOM_TAG_OTHER_PATIENT_IDS(0x0010, 0x1000);
  static const DicomTag DICOM_TAG_PER_FRAME_FUNCTIONAL_GROUP_SEQUENCE(0x5200, 0x9230);
  static const DicomTag DICOM_TAG_PIXEL_VALUE_TRANSFORMATION_SEQUENCE(0x0028, 0x9145);
  static const DicomTag DICOM_TAG_FRAME_VOI_LUT_SEQUENCE(0x0028, 0x9132);
  static const DicomTag DICOM_TAG_ACQUISITION_NUMBER(0x0020, 0x0012);

  // Tags used within the Stone of Orthanc
  static const DicomTag DICOM_TAG_FRAME_INCREMENT_POINTER(0x0028, 0x0009);
  static const DicomTag DICOM_TAG_GRID_FRAME_OFFSET_VECTOR(0x3004, 0x000c);
  static const DicomTag DICOM_TAG_PIXEL_SPACING(0x0028, 0x0030);
  static const DicomTag DICOM_TAG_RESCALE_INTERCEPT(0x0028, 0x1052);
  static const DicomTag DICOM_TAG_RESCALE_SLOPE(0x0028, 0x1053);
  static const DicomTag DICOM_TAG_SLICE_THICKNESS(0x0018, 0x0050);
  static const DicomTag DICOM_TAG_WINDOW_CENTER(0x0028, 0x1050);
  static const DicomTag DICOM_TAG_WINDOW_WIDTH(0x0028, 0x1051);
  static const DicomTag DICOM_TAG_DOSE_GRID_SCALING(0x3004, 0x000e);
  static const DicomTag DICOM_TAG_RED_PALETTE_COLOR_LOOKUP_TABLE_DATA(0x0028, 0x1201);
  static const DicomTag DICOM_TAG_GREEN_PALETTE_COLOR_LOOKUP_TABLE_DATA(0x0028, 0x1202);
  static const DicomTag DICOM_TAG_BLUE_PALETTE_COLOR_LOOKUP_TABLE_DATA(0x0028, 0x1203);
  static const DicomTag DICOM_TAG_RED_PALETTE_COLOR_LOOKUP_TABLE_DESCRIPTOR(0x0028, 0x1101);
  static const DicomTag DICOM_TAG_GREEN_PALETTE_COLOR_LOOKUP_TABLE_DESCRIPTOR(0x0028, 0x1102);
  static const DicomTag DICOM_TAG_BLUE_PALETTE_COLOR_LOOKUP_TABLE_DESCRIPTOR(0x0028, 0x1103);
  static const DicomTag DICOM_TAG_CONTOUR_DATA(0x3006, 0x0050);
  static const DicomTag DICOM_TAG_CINE_RATE(0x0018, 0x0040);
                             
  // Counting patients, studies and series
  // https://www.medicalconnections.co.uk/kb/Counting_Studies_Series_and_Instances
  static const DicomTag DICOM_TAG_NUMBER_OF_PATIENT_RELATED_STUDIES(0x0020, 0x1200);  
  static const DicomTag DICOM_TAG_NUMBER_OF_PATIENT_RELATED_SERIES(0x0020, 0x1202);  
  static const DicomTag DICOM_TAG_NUMBER_OF_PATIENT_RELATED_INSTANCES(0x0020, 0x1204);  
  static const DicomTag DICOM_TAG_NUMBER_OF_STUDY_RELATED_SERIES(0x0020, 0x1206);  
  static const DicomTag DICOM_TAG_NUMBER_OF_STUDY_RELATED_INSTANCES(0x0020, 0x1208);  
  static const DicomTag DICOM_TAG_NUMBER_OF_SERIES_RELATED_INSTANCES(0x0020, 0x1209);  
  static const DicomTag DICOM_TAG_SOP_CLASSES_IN_STUDY(0x0008, 0x0062);  

  // Tags to preserve relationships during anonymization
  static const DicomTag DICOM_TAG_REFERENCED_IMAGE_SEQUENCE(0x0008, 0x1140);
  static const DicomTag DICOM_TAG_REFERENCED_SOP_INSTANCE_UID(0x0008, 0x1155);
  static const DicomTag DICOM_TAG_SOURCE_IMAGE_SEQUENCE(0x0008, 0x2112);
  static const DicomTag DICOM_TAG_FRAME_OF_REFERENCE_UID(0x0020, 0x0052);
  static const DicomTag DICOM_TAG_REFERENCED_FRAME_OF_REFERENCE_UID(0x3006, 0x0024);
  static const DicomTag DICOM_TAG_RELATED_FRAME_OF_REFERENCE_UID(0x3006, 0x00c2);
  static const DicomTag DICOM_TAG_CURRENT_REQUESTED_PROCEDURE_EVIDENCE_SEQUENCE(0x0040, 0xa375);
  static const DicomTag DICOM_TAG_REFERENCED_SERIES_SEQUENCE(0x0008, 0x1115);
  static const DicomTag DICOM_TAG_REFERENCED_FRAME_OF_REFERENCE_SEQUENCE(0x3006, 0x0010);
  static const DicomTag DICOM_TAG_RT_REFERENCED_STUDY_SEQUENCE(0x3006, 0x0012);
  static const DicomTag DICOM_TAG_RT_REFERENCED_SERIES_SEQUENCE(0x3006, 0x0014);

  // Tags for DICOMDIR
  static const DicomTag DICOM_TAG_DIRECTORY_RECORD_TYPE(0x0004, 0x1430);
  static const DicomTag DICOM_TAG_OFFSET_OF_THE_NEXT_DIRECTORY_RECORD(0x0004, 0x1400);
  static const DicomTag DICOM_TAG_OFFSET_OF_REFERENCED_LOWER_LEVEL_DIRECTORY_ENTITY(0x0004, 0x1420);
  static const DicomTag DICOM_TAG_REFERENCED_SOP_INSTANCE_UID_IN_FILE(0x0004, 0x1511);
  static const DicomTag DICOM_TAG_REFERENCED_FILE_ID(0x0004, 0x1500);

  // Tags for DicomWeb
  static const Orthanc::DicomTag DICOM_TAG_RETRIEVE_URL(0x0008, 0x1190);

}
