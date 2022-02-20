/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

#include "../../Compatibility.h"
#include "../../Images/ImageAccessor.h"

#include <memory>

#if !defined(ORTHANC_ENABLE_JPEG)
#  error The macro ORTHANC_ENABLE_JPEG must be defined
#endif

#if !defined(ORTHANC_ENABLE_PNG)
#  error The macro ORTHANC_ENABLE_PNG must be defined
#endif

#if !defined(ORTHANC_ENABLE_DCMTK_JPEG)
#  error The macro ORTHANC_ENABLE_DCMTK_JPEG must be defined
#endif

#if !defined(ORTHANC_ENABLE_DCMTK_JPEG_LOSSLESS)
#  error The macro ORTHANC_ENABLE_DCMTK_JPEG_LOSSLESS must be defined
#endif


class DcmDataset;
class DcmCodec;
class DcmCodecParameter;
class DcmRepresentationParameter;

namespace Orthanc
{
  class ParsedDicomFile;
  
  class ORTHANC_PUBLIC DicomImageDecoder : public boost::noncopyable
  {
  private:
    class ImageSource;

    DicomImageDecoder()   // This is a fully abstract class, no constructor
    {
    }

    static ImageAccessor* CreateImage(DcmDataset& dataset,
                                      bool ignorePhotometricInterpretation);

    static ImageAccessor* DecodeUncompressedImage(DcmDataset& dataset,
                                                  unsigned int frame);

    static ImageAccessor* ApplyCodec(const DcmCodec& codec,
                                     const DcmCodecParameter& parameters,
                                     const DcmRepresentationParameter& representationParameter,
                                     DcmDataset& dataset,
                                     unsigned int frame);

    static bool TruncateDecodedImage(std::unique_ptr<ImageAccessor>& image,
                                     PixelFormat format,
                                     bool allowColorConversion);

    static bool PreviewDecodedImage(std::unique_ptr<ImageAccessor>& image);

    static void ApplyExtractionMode(std::unique_ptr<ImageAccessor>& image,
                                    ImageExtractionMode mode,
                                    bool invert);

#if ORTHANC_BUILDING_FRAMEWORK_LIBRARY == 1
    // Alias for binary compatibility with Orthanc Framework 1.7.2 => don't use it anymore
    static ImageAccessor *Decode(ParsedDicomFile& dataset,
                                 unsigned int frame);
#endif

  public:
    static bool IsPsmctRle1(DcmDataset& dataset);

    static bool DecodePsmctRle1(std::string& output,
                                DcmDataset& dataset);

    static ImageAccessor *Decode(DcmDataset& dataset,
                                 unsigned int frame);

    static void ExtractPamImage(std::string& result,
                                std::unique_ptr<ImageAccessor>& image,
                                ImageExtractionMode mode,
                                bool invert);

#if ORTHANC_ENABLE_PNG == 1
    static void ExtractPngImage(std::string& result,
                                std::unique_ptr<ImageAccessor>& image,
                                ImageExtractionMode mode,
                                bool invert);
#endif

#if ORTHANC_ENABLE_JPEG == 1
    static void ExtractJpegImage(std::string& result,
                                 std::unique_ptr<ImageAccessor>& image,
                                 ImageExtractionMode mode,
                                 bool invert,
                                 uint8_t quality);
#endif
  };
}
