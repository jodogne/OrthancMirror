/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2015 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
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


/*=========================================================================

  This file is based on portions of the following project
  (cf. function "DecodePsmctRle1()"):

  Program: GDCM (Grassroots DICOM). A DICOM library
  Module:  http://gdcm.sourceforge.net/Copyright.html

  Copyright (c) 2006-2011 Mathieu Malaterre
  Copyright (c) 1993-2005 CREATIS
  (CREATIS = Centre de Recherche et d'Applications en Traitement de l'Image)
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

  * Neither name of Mathieu Malaterre, or CREATIS, nor the names of any
  contributors (CNRS, INSERM, UCB, Universite Lyon I), may be used to
  endorse or promote products derived from this software without specific
  prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR
  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  =========================================================================*/



#include "../PrecompiledHeadersServer.h"
#include "DicomImageDecoder.h"

#include "../../Core/Logging.h"
#include "../../Core/OrthancException.h"
#include "../../Core/Images/ImageProcessing.h"
#include "../../Core/Images/PngWriter.h"  // TODO REMOVE THIS
#include "../../Core/DicomFormat/DicomIntegerPixelAccessor.h"
#include "../ToDcmtkBridge.h"
#include "../FromDcmtkBridge.h"

#include <boost/lexical_cast.hpp>

#if ORTHANC_JPEG_LOSSLESS_ENABLED == 1
#include <dcmtk/dcmjpls/djcodecd.h>
#include <dcmtk/dcmjpls/djcparam.h>
#include <dcmtk/dcmjpeg/djrplol.h>
#endif


namespace Orthanc
{
  static const DicomTag DICOM_TAG_CONTENT(0x07a1, 0x100a);
  static const DicomTag DICOM_TAG_COMPRESSION_TYPE(0x07a1, 0x1011);


  static bool IsJpegLossless(const DcmDataset& dataset)
  {
    // http://support.dcmtk.org/docs/dcxfer_8h-source.html
    return (dataset.getOriginalXfer() == EXS_JPEGLSLossless ||
            dataset.getOriginalXfer() == EXS_JPEGLSLossy);
  }


  static bool IsPsmctRle1(DcmDataset& dataset)
  {
    DcmElement* e;
    char* c;

    // Check whether the DICOM instance contains an image encoded with
    // the PMSCT_RLE1 scheme.
    if (!dataset.findAndGetElement(ToDcmtkBridge::Convert(DICOM_TAG_COMPRESSION_TYPE), e).good() ||
        e == NULL ||
        !e->isaString() ||
        !e->getString(c).good() ||
        c == NULL ||
        strcmp("PMSCT_RLE1", c))
    {
      return false;
    }
    else
    {
      return true;
    }
  }


  static bool DecodePsmctRle1(std::string& output,
                              DcmDataset& dataset)
  {
    // Check whether the DICOM instance contains an image encoded with
    // the PMSCT_RLE1 scheme.
    if (!IsPsmctRle1(dataset))
    {
      return false;
    }

    // OK, this is a custom RLE encoding from Philips. Get the pixel
    // data from the appropriate private DICOM tag.
    Uint8* pixData = NULL;
    DcmElement* e;
    if (!dataset.findAndGetElement(ToDcmtkBridge::Convert(DICOM_TAG_CONTENT), e).good() ||
        e == NULL ||
        e->getUint8Array(pixData) != EC_Normal)
    {
      return false;
    }    

    // The "unsigned" below IS VERY IMPORTANT
    const uint8_t* inbuffer = reinterpret_cast<const uint8_t*>(pixData);
    const size_t length = e->getLength();

    /**
     * The code below is an adaptation of a sample code for GDCM by
     * Mathieu Malaterre (under a BSD license).
     * http://gdcm.sourceforge.net/html/rle2img_8cxx-example.html
     **/

    // RLE pass
    std::vector<uint8_t> temp;
    temp.reserve(length);
    for (size_t i = 0; i < length; i++)
    {
      if (inbuffer[i] == 0xa5)
      {
        temp.push_back(inbuffer[i+2]);
        for (uint8_t repeat = inbuffer[i + 1]; repeat != 0; repeat--)
        {
          temp.push_back(inbuffer[i+2]);
        }
        i += 2;
      }
      else
      {
        temp.push_back(inbuffer[i]);
      }
    }

    // Delta encoding pass
    uint16_t delta = 0;
    output.clear();
    output.reserve(temp.size());
    for (size_t i = 0; i < temp.size(); i++)
    {
      uint16_t value;

      if (temp[i] == 0x5a)
      {
        uint16_t v1 = temp[i + 1];
        uint16_t v2 = temp[i + 2];
        value = (v2 << 8) + v1;
        i += 2;
      }
      else
      {
        value = delta + (int8_t) temp[i];
      }

      output.push_back(value & 0xff);
      output.push_back(value >> 8);
      delta = value;
    }

    if (output.size() % 2)
    {
      output.resize(output.size() - 1);
    }

    return true;
  }


  class DicomImageDecoder::ImageSource
  {
  private:
    std::string psmct_;
    std::auto_ptr<DicomIntegerPixelAccessor> slowAccessor_;

  public:
    void Setup(DcmDataset& dataset,
               unsigned int frame)
    {
      psmct_.clear();
      slowAccessor_.reset(NULL);

      // See also: http://support.dcmtk.org/wiki/dcmtk/howto/accessing-compressed-data

      DicomMap m;
      FromDcmtkBridge::Convert(m, dataset);

      /**
       * Create an accessor to the raw values of the DICOM image.
       **/

      DcmElement* e;
      if (dataset.findAndGetElement(ToDcmtkBridge::Convert(DICOM_TAG_PIXEL_DATA), e).good() &&
          e != NULL)
      {
        Uint8* pixData = NULL;
        if (e->getUint8Array(pixData) == EC_Normal)
        {    
          slowAccessor_.reset(new DicomIntegerPixelAccessor(m, pixData, e->getLength()));
        }
      }
      else if (DecodePsmctRle1(psmct_, dataset))
      {
        LOG(INFO) << "The PMSCT_RLE1 decoding has succeeded";
        Uint8* pixData = NULL;
        if (psmct_.size() > 0)
        {
          pixData = reinterpret_cast<Uint8*>(&psmct_[0]);
        }

        slowAccessor_.reset(new DicomIntegerPixelAccessor(m, pixData, psmct_.size()));
      }
    
      if (slowAccessor_.get() == NULL)
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }

      slowAccessor_->SetCurrentFrame(frame);
    }

    unsigned int GetWidth() const
    {
      assert(slowAccessor_.get() != NULL);
      return slowAccessor_->GetInformation().GetWidth();
    }

    unsigned int GetHeight() const
    {
      assert(slowAccessor_.get() != NULL);
      return slowAccessor_->GetInformation().GetHeight();
    }

    unsigned int GetChannelCount() const
    {
      assert(slowAccessor_.get() != NULL);
      return slowAccessor_->GetInformation().GetChannelCount();
    }

    const DicomIntegerPixelAccessor& GetAccessor() const
    {
      assert(slowAccessor_.get() != NULL);
      return *slowAccessor_;
    }

    unsigned int GetSize() const
    {
      assert(slowAccessor_.get() != NULL);
      return slowAccessor_->GetSize();
    }
  };


  void DicomImageDecoder::SetupImageBuffer(ImageBuffer& target,
                                           DcmDataset& dataset)
  {
    DicomMap m;
    FromDcmtkBridge::Convert(m, dataset);

    DicomImageInformation info(m);
    PixelFormat format;
    
    if (!info.ExtractPixelFormat(format))
    {
      LOG(WARNING) << "Unsupported DICOM image: " << info.GetBitsStored() 
                   << "bpp, " << info.GetChannelCount() << " channels, " 
                   << (info.IsSigned() ? "signed" : "unsigned")
                   << (info.IsPlanar() ? ", planar, " : ", non-planar, ")
                   << EnumerationToString(info.GetPhotometricInterpretation())
                   << " photometric interpretation";
      throw OrthancException(ErrorCode_NotImplemented);
    }

    target.SetHeight(info.GetHeight());
    target.SetWidth(info.GetWidth());
    target.SetFormat(format);
  }


  bool DicomImageDecoder::IsUncompressedImage(const DcmDataset& dataset)
  {
    // http://support.dcmtk.org/docs/dcxfer_8h-source.html
    return (dataset.getOriginalXfer() == EXS_Unknown ||
            dataset.getOriginalXfer() == EXS_LittleEndianImplicit ||
            dataset.getOriginalXfer() == EXS_BigEndianImplicit ||
            dataset.getOriginalXfer() == EXS_LittleEndianExplicit ||
            dataset.getOriginalXfer() == EXS_BigEndianExplicit);
  }


  template <typename PixelType>
  static void CopyPixels(ImageAccessor& target,
                         const DicomIntegerPixelAccessor& source)
  {
    const PixelType minValue = std::numeric_limits<PixelType>::min();
    const PixelType maxValue = std::numeric_limits<PixelType>::max();

    for (unsigned int y = 0; y < source.GetInformation().GetHeight(); y++)
    {
      PixelType* pixel = reinterpret_cast<PixelType*>(target.GetRow(y));
      for (unsigned int x = 0; x < source.GetInformation().GetWidth(); x++)
      {
        for (unsigned int c = 0; c < source.GetInformation().GetChannelCount(); c++, pixel++)
        {
          int32_t v = source.GetValue(x, y, c);
          if (v < static_cast<int32_t>(minValue))
          {
            *pixel = minValue;
          }
          else if (v > static_cast<int32_t>(maxValue))
          {
            *pixel = maxValue;
          }
          else
          {
            *pixel = static_cast<PixelType>(v);
          }
        }
      }
    }
  }


  void DicomImageDecoder::DecodeUncompressedImage(ImageBuffer& target,
                                                  DcmDataset& dataset,
                                                  unsigned int frame)
  {
    if (!IsUncompressedImage(dataset))
    {
      throw OrthancException(ErrorCode_BadParameterType);
    }

    DecodeUncompressedImageInternal(target, dataset, frame);
  }


  void DicomImageDecoder::DecodeUncompressedImageInternal(ImageBuffer& target,
                                                          DcmDataset& dataset,
                                                          unsigned int frame)
  {
    ImageSource source;
    source.Setup(dataset, frame);


    /**
     * Resize the target image.
     **/

    SetupImageBuffer(target, dataset);

    if (source.GetWidth() != target.GetWidth() ||
        source.GetHeight() != target.GetHeight())
    {
      throw OrthancException(ErrorCode_InternalError);
    }


    /**
     * If the format of the DICOM buffer is natively supported, use a
     * direct access to copy its values.
     **/

    ImageAccessor targetAccessor(target.GetAccessor());
    const DicomImageInformation& info = source.GetAccessor().GetInformation();

    bool fastVersionSuccess = false;
    PixelFormat sourceFormat;
    if (!info.IsPlanar() &&
        info.ExtractPixelFormat(sourceFormat))
    {
      try
      {
        size_t frameSize = info.GetHeight() * info.GetWidth() * GetBytesPerPixel(sourceFormat);
        if ((frame + 1) * frameSize <= source.GetSize())
        {
          const uint8_t* buffer = reinterpret_cast<const uint8_t*>(source.GetAccessor().GetPixelData());

          ImageAccessor sourceImage;
          sourceImage.AssignReadOnly(sourceFormat, 
                                     info.GetWidth(), 
                                     info.GetHeight(),
                                     info.GetWidth() * GetBytesPerPixel(sourceFormat),
                                     buffer + frame * frameSize);

          ImageProcessing::Convert(targetAccessor, sourceImage);
          ImageProcessing::ShiftRight(targetAccessor, info.GetShift());
          fastVersionSuccess = true;
        }
      }
      catch (OrthancException&)
      {
        // Unsupported conversion, use the slow version
      }
    }

    /**
     * Slow version : loop over the DICOM buffer, storing its value
     * into the target image.
     **/

    if (!fastVersionSuccess)
    {
      switch (target.GetFormat())
      {
        case PixelFormat_RGB24:
        case PixelFormat_RGBA32:
        case PixelFormat_Grayscale8:
          CopyPixels<uint8_t>(targetAccessor, source.GetAccessor());
          break;
        
        case PixelFormat_Grayscale16:
          CopyPixels<uint16_t>(targetAccessor, source.GetAccessor());
          break;

        case PixelFormat_SignedGrayscale16:
          CopyPixels<int16_t>(targetAccessor, source.GetAccessor());
          break;

        default:
          throw OrthancException(ErrorCode_InternalError);
      }
    }
  }


#if ORTHANC_JPEG_LOSSLESS_ENABLED == 1
  void DicomImageDecoder::DecodeJpegLossless(ImageBuffer& target,
                                             DcmDataset& dataset,
                                             unsigned int frame)
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

    ImageAccessor targetAccessor(target.GetAccessor());

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
                                        &dataset, frame, startFragment, targetAccessor.GetBuffer(), 
                                        targetAccessor.GetSize(), decompressedColorModel);

    if (!c.good())
    {
      throw OrthancException(ErrorCode_InternalError);
    }
  }
#endif




  bool DicomImageDecoder::Decode(ImageBuffer& target,
                                 DcmDataset& dataset,
                                 unsigned int frame)
  {
    if (IsUncompressedImage(dataset))
    {
      DecodeUncompressedImage(target, dataset, frame);
      return true;
    }


#if ORTHANC_JPEG_LOSSLESS_ENABLED == 1
    if (IsJpegLossless(dataset))
    {
      LOG(INFO) << "Decoding a JPEG-LS image";
      DecodeJpegLossless(target, dataset, frame);
      return true;
    }
#endif


#if ORTHANC_JPEG_ENABLED == 1
    // TODO Implement this part to speed up JPEG decompression
#endif


    /**
     * This DICOM image format is not natively supported by
     * Orthanc. As a last resort, try and decode it through
     * DCMTK. This will result in higher memory consumption. This is
     * actually the second example of the following page:
     * http://support.dcmtk.org/docs/mod_dcmjpeg.html#Examples
     **/
    
    {
      LOG(INFO) << "Using DCMTK to decode a compressed image";

      std::auto_ptr<DcmDataset> converted(dynamic_cast<DcmDataset*>(dataset.clone()));
      converted->chooseRepresentation(EXS_LittleEndianExplicit, NULL);

      if (converted->canWriteXfer(EXS_LittleEndianExplicit))
      {
        DecodeUncompressedImageInternal(target, *converted, frame);
        return true;
      }
    }

    return false;
  }


  static bool IsColorImage(PixelFormat format)
  {
    return (format == PixelFormat_RGB24 ||
            format == PixelFormat_RGBA32);
  }


  bool DicomImageDecoder::DecodeAndTruncate(ImageBuffer& target,
                                            DcmDataset& dataset,
                                            unsigned int frame,
                                            PixelFormat format,
                                            bool allowColorConversion)
  {
    // TODO Special case for uncompressed images
    
    ImageBuffer source;
    if (!Decode(source, dataset, frame))
    {
      return false;
    }

    // If specified, prevent the conversion between color and
    // grayscale images
    bool isSourceColor = IsColorImage(source.GetFormat());
    bool isTargetColor = IsColorImage(format);

    if (!allowColorConversion)
    {
      if (isSourceColor ^ isTargetColor)
      {
        return false;
      }
    }

    if (source.GetFormat() == format)
    {
      // No conversion is required, return the temporary image
      target.AcquireOwnership(source);
      return true;
    }

    target.SetFormat(format);
    target.SetWidth(source.GetWidth());
    target.SetHeight(source.GetHeight());

    ImageAccessor targetAccessor(target.GetAccessor());
    ImageAccessor sourceAccessor(source.GetAccessor());
    ImageProcessing::Convert(targetAccessor, sourceAccessor);

    return true;
  }


  bool DicomImageDecoder::DecodePreview(ImageBuffer& target,
                                        DcmDataset& dataset,
                                        unsigned int frame)
  {
    // TODO Special case for uncompressed images
    
    ImageBuffer source;
    if (!Decode(source, dataset, frame))
    {
      return false;
    }

    switch (source.GetFormat())
    {
      case PixelFormat_RGB24:
      {
        // Directly return color images (RGB)
        target.AcquireOwnership(source);
        return true;
      }

      case PixelFormat_Grayscale8:
      case PixelFormat_Grayscale16:
      case PixelFormat_SignedGrayscale16:
      {
        // Grayscale image: Stretch its dynamics to the [0,255] range
        target.SetFormat(PixelFormat_Grayscale8);
        target.SetWidth(source.GetWidth());
        target.SetHeight(source.GetHeight());

        ImageAccessor targetAccessor(target.GetAccessor());
        ImageAccessor sourceAccessor(source.GetAccessor());

        int64_t a, b;
        ImageProcessing::GetMinMaxValue(a, b, sourceAccessor);
        
        if (a == b)
        {
          ImageProcessing::Set(targetAccessor, 0);
        }
        else
        {
          ImageProcessing::ShiftScale(sourceAccessor, static_cast<float>(-a), 255.0f / static_cast<float>(b - a));

          if (source.GetFormat() == PixelFormat_Grayscale8)
          {
            target.AcquireOwnership(source);
          }
          else
          {
            ImageProcessing::Convert(targetAccessor, sourceAccessor);
          }
        }

        return true;
      }
      
      default:
        throw OrthancException(ErrorCode_NotImplemented);
    }
  }
}
