/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012 Medical Physics Department, CHU of Liege,
 * Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#pragma once

#include <string>
#include <stdint.h>


namespace Orthanc
{
  class DicomTag
  {
    // This must stay a POD (plain old data structure) 

  private:
    uint16_t group_;
    uint16_t element_;

  public:
    DicomTag(uint16_t group,
             uint16_t element) :
      group_(group),
      element_(element)
    {
    }

    uint16_t GetGroup() const
    {
      return group_;
    }

    uint16_t GetElement() const
    {
      return element_;
    }

    bool operator< (const DicomTag& other) const;

    std::string Format() const;

    friend std::ostream& operator<< (std::ostream& o, const DicomTag& tag);
  };

  // Aliases for the most useful tags
  static const DicomTag DICOM_TAG_ACCESSION_NUMBER(0x0008, 0x0050);
  static const DicomTag DICOM_TAG_SOP_INSTANCE_UID(0x0008, 0x0018);
  static const DicomTag DICOM_TAG_PATIENT_ID(0x0010, 0x0020);
  static const DicomTag DICOM_TAG_SERIES_INSTANCE_UID(0x0020, 0x000e);
  static const DicomTag DICOM_TAG_STUDY_INSTANCE_UID(0x0020, 0x000d);
  static const DicomTag DICOM_TAG_PIXEL_DATA(0x7fe0, 0x0010);

  static const DicomTag DICOM_TAG_IMAGE_INDEX(0x0054, 0x1330);
  static const DicomTag DICOM_TAG_INSTANCE_NUMBER(0x0020, 0x0013);

  static const DicomTag DICOM_TAG_NUMBER_OF_SLICES(0x0054, 0x0081);
  static const DicomTag DICOM_TAG_NUMBER_OF_FRAMES(0x0028, 0x0008);
  static const DicomTag DICOM_TAG_CARDIAC_NUMBER_OF_IMAGES(0x0018, 0x1090);
  static const DicomTag DICOM_TAG_IMAGES_IN_ACQUISITION(0x0020, 0x1002);
}
