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

#include "../ParsedDicomFile.h"

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
  class DicomImageDecoder : public boost::noncopyable
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

    static bool TruncateDecodedImage(std::auto_ptr<ImageAccessor>& image,
                                     PixelFormat format,
                                     bool allowColorConversion);

    static bool PreviewDecodedImage(std::auto_ptr<ImageAccessor>& image);

    static void ApplyExtractionMode(std::auto_ptr<ImageAccessor>& image,
                                    ImageExtractionMode mode,
                                    bool invert);

  public:
    static bool IsPsmctRle1(DcmDataset& dataset);

    static bool DecodePsmctRle1(std::string& output,
                                DcmDataset& dataset);

    static ImageAccessor *Decode(ParsedDicomFile& dicom,
                                 unsigned int frame);

#if ORTHANC_ENABLE_PNG == 1
    static void ExtractPngImage(std::string& result,
                                std::auto_ptr<ImageAccessor>& image,
                                ImageExtractionMode mode,
                                bool invert);
#endif

#if ORTHANC_ENABLE_JPEG == 1
    static void ExtractJpegImage(std::string& result,
                                 std::auto_ptr<ImageAccessor>& image,
                                 ImageExtractionMode mode,
                                 bool invert,
                                 uint8_t quality);
#endif
  };
}
