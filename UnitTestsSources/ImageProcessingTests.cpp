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


#include "PrecompiledHeadersUnitTests.h"
#include "gtest/gtest.h"

#include "../Core/DicomFormat/DicomImageInformation.h"
#include "../Core/Images/ImageBuffer.h"
#include "../Core/Images/ImageProcessing.h"

using namespace Orthanc;


TEST(DicomImageInformation, ExtractPixelFormat1)
{
  // Cardiac/MR*
  DicomMap m;
  m.SetValue(DICOM_TAG_ROWS, "24", false);
  m.SetValue(DICOM_TAG_COLUMNS, "16", false);
  m.SetValue(DICOM_TAG_BITS_ALLOCATED, "16", false);
  m.SetValue(DICOM_TAG_SAMPLES_PER_PIXEL, "1", false);
  m.SetValue(DICOM_TAG_BITS_STORED, "12", false);
  m.SetValue(DICOM_TAG_HIGH_BIT, "11", false);
  m.SetValue(DICOM_TAG_PIXEL_REPRESENTATION, "0", false);
  m.SetValue(DICOM_TAG_PHOTOMETRIC_INTERPRETATION, "MONOCHROME2", false);

  DicomImageInformation info(m);
  PixelFormat format;
  ASSERT_TRUE(info.ExtractPixelFormat(format, false));
  ASSERT_EQ(PixelFormat_Grayscale16, format);
}


TEST(DicomImageInformation, ExtractPixelFormat2)
{
  // Delphine CT
  DicomMap m;
  m.SetValue(DICOM_TAG_ROWS, "24", false);
  m.SetValue(DICOM_TAG_COLUMNS, "16", false);
  m.SetValue(DICOM_TAG_BITS_ALLOCATED, "16", false);
  m.SetValue(DICOM_TAG_SAMPLES_PER_PIXEL, "1", false);
  m.SetValue(DICOM_TAG_BITS_STORED, "16", false);
  m.SetValue(DICOM_TAG_HIGH_BIT, "15", false);
  m.SetValue(DICOM_TAG_PIXEL_REPRESENTATION, "1", false);
  m.SetValue(DICOM_TAG_PHOTOMETRIC_INTERPRETATION, "MONOCHROME2", false);

  DicomImageInformation info(m);
  PixelFormat format;
  ASSERT_TRUE(info.ExtractPixelFormat(format, false));
  ASSERT_EQ(PixelFormat_SignedGrayscale16, format);
}
