/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2014 Medical Physics Department, CHU of Liege,
 * Belgium
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


#include "../PrecompiledHeadersServer.h"
#include "DicomImageDecoder.h"

#include "../../Core/OrthancException.h"
#include "../ToDcmtkBridge.h"

#include <dcmtk/dcmjpls/djcodecd.h>
#include <dcmtk/dcmjpls/djcparam.h>
#include <dcmtk/dcmjpeg/djrplol.h>
#include <boost/lexical_cast.hpp>

#if ORTHANC_JPEG_LOSSLESS_ENABLED == 1
#endif


namespace Orthanc
{
  void DicomImageDecoder::SetupImageBuffer(ImageBuffer& target,
                                           DcmDataset& dataset)
  {
    OFString value;

    if (!dataset.findAndGetOFString(ToDcmtkBridge::Convert(DICOM_TAG_COLUMNS), value).good())
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    unsigned int width = boost::lexical_cast<unsigned int>(value.c_str());

    if (!dataset.findAndGetOFString(ToDcmtkBridge::Convert(DICOM_TAG_ROWS), value).good())
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    unsigned int height = boost::lexical_cast<unsigned int>(value.c_str());

    if (!dataset.findAndGetOFString(ToDcmtkBridge::Convert(DICOM_TAG_BITS_STORED), value).good())
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    unsigned int bitsStored = boost::lexical_cast<unsigned int>(value.c_str());

    if (!dataset.findAndGetOFString(ToDcmtkBridge::Convert(DICOM_TAG_PIXEL_REPRESENTATION), value).good())
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    bool isSigned = (boost::lexical_cast<unsigned int>(value.c_str()) != 0);

    unsigned int samplesPerPixel = 1; // By default
    if (dataset.findAndGetOFString(ToDcmtkBridge::Convert(DICOM_TAG_SAMPLES_PER_PIXEL), value).good())
    {
      samplesPerPixel = boost::lexical_cast<unsigned int>(value.c_str());
    }

    target.SetHeight(height);
    target.SetWidth(width);

    if (bitsStored == 8 && samplesPerPixel == 1 && !isSigned)
    {
      target.SetFormat(PixelFormat_Grayscale8);
    }
    else if (bitsStored == 8 && samplesPerPixel == 3 && !isSigned)
    {
      target.SetFormat(PixelFormat_RGB24);
    }
    else if (bitsStored == 16 && samplesPerPixel == 1 && !isSigned)
    {
      target.SetFormat(PixelFormat_Grayscale16);
    }
    else if (bitsStored == 16 && samplesPerPixel == 1 && isSigned)
    {
      target.SetFormat(PixelFormat_SignedGrayscale16);
    }
    else
    {
      throw OrthancException(ErrorCode_NotImplemented);
    }
  }


  bool DicomImageDecoder::IsJpegLossless(const DcmDataset& dataset)
  {
    return (dataset.getOriginalXfer() == EXS_JPEGLSLossless ||
            dataset.getOriginalXfer() == EXS_JPEGLSLossy);
  }


#if ORTHANC_JPEG_LOSSLESS_ENABLED == 1
  void DicomImageDecoder::DecodeJpegLossless(ImageBuffer& target,
                                             DcmDataset& dataset)
  {
    if (!IsJpegLossless(dataset))
    {
      throw OrthancException(ErrorCode_BadParameterType);
    }

    DcmElement *element = NULL;
    if (!dataset.findAndGetElement(ToDcmtkBridge::Convert(DICOM_TAG_PIXEL_DATA), element).good())
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    DcmPixelData& pixelData = dynamic_cast<DcmPixelData&>(*element);
    DcmPixelSequence* pixelSequence = NULL;
    if (!pixelData.getEncapsulatedRepresentation
        (dataset.getOriginalXfer(), NULL, pixelSequence).good())
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    SetupImageBuffer(target, dataset);

    ImageAccessor accessor(target.GetAccessor());

    /**
     * The "DJLSLosslessDecoder" and "DJLSNearLosslessDecoder" in DCMTK
     * are exactly the same, except for the "supportedTransferSyntax()"
     * virtual function.
     * http://support.dcmtk.org/docs/classDJLSDecoderBase.html
     **/

    DJLSLosslessDecoder decoder; DJLSCodecParameter parameters;
    //DJLSNearLosslessDecoder decoder; DJLSCodecParameter parameters;

    Uint32 startFragment = 0;  // Default 
    OFString decompressedColorModel;  // Out
    DJ_RPLossless representationParameter;
    OFCondition c = decoder.decodeFrame(&representationParameter, pixelSequence, &parameters, 
                                        &dataset, 0, startFragment, accessor.GetBuffer(), 
                                        accessor.GetSize(), decompressedColorModel);

    if (!c.good())
    {
      throw OrthancException(ErrorCode_InternalError);
    }
  }
#endif
}
