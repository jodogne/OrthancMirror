/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2018 Osimis S.A., Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * In addition, as a special exception, the copyright holders of this
 * program give permission to link the code of its release with the
 * OpenSSL project's "OpenSSL" library (or with modified versions of it
 * that use the same license as the "OpenSSL" library), and distribute
 * the linked executables. You must obey the GNU General Public License
 * in all respects for all of the code used other than "OpenSSL". If you
 * modify file(s) with this exception, you may extend this exception to
 * your version of the file(s), but you are not obligated to do so. If
 * you do not wish to do so, delete this exception statement from your
 * version. If you delete this exception statement from all source files
 * in the program, then also delete it here.
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

#include <stdint.h>

namespace OrthancPlugins
{
  class DicomTag
  {
  private:
    uint16_t  group_;
    uint16_t  element_;

    DicomTag();  // Forbidden

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

    const char* GetName() const;

    bool operator== (const DicomTag& other) const
    {
      return group_ == other.group_ && element_ == other.element_;
    }

    bool operator!= (const DicomTag& other) const
    {
      return !(*this == other);
    }
  };


  static const DicomTag DICOM_TAG_BITS_STORED(0x0028, 0x0101);
  static const DicomTag DICOM_TAG_COLUMNS(0x0028, 0x0011);
  static const DicomTag DICOM_TAG_COLUMN_POSITION_IN_TOTAL_IMAGE_PIXEL_MATRIX(0x0048, 0x021e);
  static const DicomTag DICOM_TAG_IMAGE_ORIENTATION_PATIENT(0x0020, 0x0037);
  static const DicomTag DICOM_TAG_IMAGE_POSITION_PATIENT(0x0020, 0x0032);
  static const DicomTag DICOM_TAG_MODALITY(0x0008, 0x0060);
  static const DicomTag DICOM_TAG_NUMBER_OF_FRAMES(0x0028, 0x0008);
  static const DicomTag DICOM_TAG_PER_FRAME_FUNCTIONAL_GROUPS_SEQUENCE(0x5200, 0x9230);
  static const DicomTag DICOM_TAG_PHOTOMETRIC_INTERPRETATION(0x0028, 0x0004);
  static const DicomTag DICOM_TAG_PIXEL_REPRESENTATION(0x0028, 0x0103);
  static const DicomTag DICOM_TAG_PIXEL_SPACING(0x0028, 0x0030);
  static const DicomTag DICOM_TAG_PLANE_POSITION_SLIDE_SEQUENCE(0x0048, 0x021a);
  static const DicomTag DICOM_TAG_RESCALE_INTERCEPT(0x0028, 0x1052);
  static const DicomTag DICOM_TAG_RESCALE_SLOPE(0x0028, 0x1053);
  static const DicomTag DICOM_TAG_ROWS(0x0028, 0x0010);
  static const DicomTag DICOM_TAG_ROW_POSITION_IN_TOTAL_IMAGE_PIXEL_MATRIX(0x0048, 0x021f);
  static const DicomTag DICOM_TAG_SAMPLES_PER_PIXEL(0x0028, 0x0002);
  static const DicomTag DICOM_TAG_SERIES_INSTANCE_UID(0x0020, 0x000e);
  static const DicomTag DICOM_TAG_SLICE_THICKNESS(0x0018, 0x0050);
  static const DicomTag DICOM_TAG_SOP_CLASS_UID(0x0008, 0x0016);
  static const DicomTag DICOM_TAG_SOP_INSTANCE_UID(0x0008, 0x0018);
  static const DicomTag DICOM_TAG_TOTAL_PIXEL_MATRIX_COLUMNS(0x0048, 0x0006);
  static const DicomTag DICOM_TAG_TOTAL_PIXEL_MATRIX_ROWS(0x0048, 0x0007);
  static const DicomTag DICOM_TAG_TRANSFER_SYNTAX_UID(0x0002, 0x0010);
  static const DicomTag DICOM_TAG_WINDOW_CENTER(0x0028, 0x1050);
  static const DicomTag DICOM_TAG_WINDOW_WIDTH(0x0028, 0x1051);
}
