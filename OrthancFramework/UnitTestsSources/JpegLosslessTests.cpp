/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
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


#if ORTHANC_UNIT_TESTS_LINK_FRAMEWORK == 1
// Must be the first to be sure to use the Orthanc framework shared library
#  include <OrthancFramework.h>
#endif

#if !defined(ORTHANC_ENABLE_DCMTK_JPEG_LOSSLESS)
#  error ORTHANC_ENABLE_DCMTK_JPEG_LOSSLESS is not defined
#endif

#include <gtest/gtest.h>

#if ORTHANC_ENABLE_DCMTK_JPEG_LOSSLESS == 1

#include "../Sources/DicomParsing/Internals/DicomImageDecoder.h"
#include "../Sources/DicomParsing/ParsedDicomFile.h"
#include "../Sources/Images/ImageBuffer.h"
#include "../Sources/Images/PngWriter.h"
#include "../Sources/OrthancException.h"

#include <dcmtk/dcmdata/dcfilefo.h>

using namespace Orthanc;



// TODO Write a test


#endif
