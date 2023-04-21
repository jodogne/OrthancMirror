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


#include "../../PrecompiledHeaders.h"
#include "DicomImageDecoder.h"

#include "../ParsedDicomFile.h"


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


#include "../../Logging.h"
#include "../../OrthancException.h"
#include "../../Images/Image.h"
#include "../../Images/ImageProcessing.h"
#include "../../DicomFormat/DicomIntegerPixelAccessor.h"
#include "../ToDcmtkBridge.h"
#include "../FromDcmtkBridge.h"

#if ORTHANC_ENABLE_PNG == 1
#  include "../../Images/PngWriter.h"
#endif

#if ORTHANC_ENABLE_JPEG == 1
#  include "../../Images/JpegWriter.h"
#endif
#include "../../Images/PamWriter.h"

#include <boost/lexical_cast.hpp>

#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcrleccd.h>
#include <dcmtk/dcmdata/dcrlecp.h>
#include <dcmtk/dcmdata/dcrlerp.h>
#include <dcmtk/dcmdata/dcswap.h>

#if ORTHANC_ENABLE_DCMTK_JPEG_LOSSLESS == 1
#  include <dcmtk/dcmjpeg/djrplol.h>
#  include <dcmtk/dcmjpls/djcodecd.h>
#  include <dcmtk/dcmjpls/djcparam.h>
#  include <dcmtk/dcmjpls/djrparam.h>
#endif

#if ORTHANC_ENABLE_DCMTK_JPEG == 1
#  include <dcmtk/dcmjpeg/djcodecd.h>
#  include <dcmtk/dcmjpeg/djcparam.h>
#  include <dcmtk/dcmjpeg/djdecbas.h>
#  include <dcmtk/dcmjpeg/djdecext.h>
#  include <dcmtk/dcmjpeg/djdeclol.h>
#  include <dcmtk/dcmjpeg/djdecpro.h>
#  include <dcmtk/dcmjpeg/djdecsps.h>
#  include <dcmtk/dcmjpeg/djdecsv1.h>
#  include <dcmtk/dcmjpeg/djrploss.h>
#endif

#if DCMTK_VERSION_NUMBER <= 360
#  define EXS_JPEGProcess1      EXS_JPEGProcess1TransferSyntax
#  define EXS_JPEGProcess2_4    EXS_JPEGProcess2_4TransferSyntax
#  define EXS_JPEGProcess6_8    EXS_JPEGProcess6_8TransferSyntax
#  define EXS_JPEGProcess10_12  EXS_JPEGProcess10_12TransferSyntax
#  define EXS_JPEGProcess14     EXS_JPEGProcess14TransferSyntax
#  define EXS_JPEGProcess14SV1  EXS_JPEGProcess14SV1TransferSyntax
#endif

namespace Orthanc
{
  static const Endianness ENDIANNESS = Toolbox::DetectEndianness();
  static const DicomTag DICOM_TAG_CONTENT(0x07a1, 0x100a);
  static const DicomTag DICOM_TAG_COMPRESSION_TYPE(0x07a1, 0x1011);


  bool DicomImageDecoder::IsPsmctRle1(DcmDataset& dataset)
  {
    DcmElement* e;
    char* c;

    // Check whether the DICOM instance contains an image encoded with
    // the PMSCT_RLE1 scheme.
    if (!dataset.findAndGetElement(ToDcmtkBridge::Convert(DICOM_TAG_COMPRESSION_TYPE), e).good() ||
        !dataset.tagExistsWithValue(ToDcmtkBridge::Convert(DICOM_TAG_CONTENT)) ||  // New in Orthanc 1.7.4
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


  bool DicomImageDecoder::DecodePsmctRle1(std::string& output,
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
    std::unique_ptr<DicomIntegerPixelAccessor> slowAccessor_;

  public:
    void Setup(DcmDataset& dataset,
               unsigned int frame)
    {
      psmct_.clear();
      slowAccessor_.reset(NULL);

      // See also: http://support.dcmtk.org/wiki/dcmtk/howto/accessing-compressed-data

      DicomMap m;
      std::set<DicomTag> ignoreTagLength;
      FromDcmtkBridge::ExtractDicomSummary(m, dataset, DicomImageInformation::GetUsefulTagLength(), ignoreTagLength);

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


  ImageAccessor* DicomImageDecoder::CreateImage(DcmDataset& dataset,
                                                bool ignorePhotometricInterpretation)
  {
    DicomMap m;
    std::set<DicomTag> ignoreTagLength;
    FromDcmtkBridge::ExtractDicomSummary(m, dataset, DicomImageInformation::GetUsefulTagLength(), ignoreTagLength);

    DicomImageInformation info(m);
    PixelFormat format;
    
    if (!info.ExtractPixelFormat(format, ignorePhotometricInterpretation))
    {
      LOG(WARNING) << "Unsupported DICOM image: " << info.GetBitsStored() 
                   << "bpp, " << info.GetChannelCount() << " channels, " 
                   << (info.IsSigned() ? "signed" : "unsigned")
                   << (info.IsPlanar() ? ", planar, " : ", non-planar, ")
                   << EnumerationToString(info.GetPhotometricInterpretation())
                   << " photometric interpretation";
      throw OrthancException(ErrorCode_NotImplemented);
    }

    return new Image(format, info.GetWidth(), info.GetHeight(), false);
  }


  template <typename PixelType>
  static void CopyPixels(ImageAccessor& target,
                         const DicomIntegerPixelAccessor& source)
  {
    // WARNING - "::min()" should be replaced by "::lowest()" if
    // dealing with float or double (which is not the case so far)
    const PixelType minValue = std::numeric_limits<PixelType>::min();
    const PixelType maxValue = std::numeric_limits<PixelType>::max();

    const unsigned int height = source.GetInformation().GetHeight();
    const unsigned int width = source.GetInformation().GetWidth();
    const unsigned int channels = source.GetInformation().GetChannelCount();
    
    for (unsigned int y = 0; y < height; y++)
    {
      PixelType* pixel = reinterpret_cast<PixelType*>(target.GetRow(y));
      for (unsigned int x = 0; x < width; x++)
      {
        for (unsigned int c = 0; c < channels; c++, pixel++)
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


  static ImageAccessor* DecodeLookupTable(std::unique_ptr<ImageAccessor>& target,
                                          const DicomImageInformation& info,
                                          DcmDataset& dataset,
                                          const uint8_t* pixelData,
                                          unsigned long pixelLength)
  {
    LOG(INFO) << "Decoding a lookup table";

    OFString r, g, b;
    PixelFormat format;
    const uint16_t* lutRed = NULL;
    const uint16_t* lutGreen = NULL;
    const uint16_t* lutBlue = NULL;
    unsigned long rc = 0;
    unsigned long gc = 0;
    unsigned long bc = 0;

    if (pixelData == NULL &&
        !dataset.findAndGetUint8Array(DCM_PixelData, pixelData, &pixelLength).good())
    {
      throw OrthancException(ErrorCode_NotImplemented);
    }

    if (info.IsPlanar() ||
        info.GetNumberOfFrames() != 1 ||
        !info.ExtractPixelFormat(format, false) ||
        !dataset.findAndGetOFStringArray(DCM_BluePaletteColorLookupTableDescriptor, b).good() ||
        !dataset.findAndGetOFStringArray(DCM_GreenPaletteColorLookupTableDescriptor, g).good() ||
        !dataset.findAndGetOFStringArray(DCM_RedPaletteColorLookupTableDescriptor, r).good() ||
        !dataset.findAndGetUint16Array(DCM_BluePaletteColorLookupTableData, lutBlue, &bc).good() ||
        !dataset.findAndGetUint16Array(DCM_GreenPaletteColorLookupTableData, lutGreen, &gc).good() ||
        !dataset.findAndGetUint16Array(DCM_RedPaletteColorLookupTableData, lutRed, &rc).good() ||
        r != g ||
        r != b ||
        g != b ||
        lutRed == NULL ||
        lutGreen == NULL ||
        lutBlue == NULL ||
        pixelData == NULL)
    {
      throw OrthancException(ErrorCode_NotImplemented);
    }

    switch (format)
    {
      case PixelFormat_RGB24:
      {
        if (r != "256\\0\\16" ||
            rc != 256 ||
            gc != 256 ||
            bc != 256)
        {
          throw OrthancException(ErrorCode_NotImplemented);
        }

        if (pixelLength != target->GetWidth() * target->GetHeight())
        {
          DcmElement *elem;
          Uint16 bitsAllocated = 0;

          if (!dataset.findAndGetUint16(DCM_BitsAllocated, bitsAllocated).good())
          {
            throw OrthancException(ErrorCode_NotImplemented);  
          }

          if (!dataset.findAndGetElement(DCM_PixelData, elem).good())
          {
            throw OrthancException(ErrorCode_NotImplemented);  
          }

          // In implicit VR files, pixelLength is expressed in words (OW) although pixels can actually be 8 bits
          // -> pixelLength is wrong by a factor of two and the image can still be decoded!
          // seen in some Philips ClearVue 650 images (using 8 bits LUT)
          if (!(elem->getVR() == EVR_OW && bitsAllocated == 8 && (2*pixelLength == target->GetWidth() * target->GetHeight())))  
          {
            throw OrthancException(ErrorCode_NotImplemented);
          }
        }

        const uint8_t* source = reinterpret_cast<const uint8_t*>(pixelData);
        const unsigned int width = target->GetWidth();
        const unsigned int height = target->GetHeight();
        
        for (unsigned int y = 0; y < height; y++)
        {
          uint8_t* p = reinterpret_cast<uint8_t*>(target->GetRow(y));

          for (unsigned int x = 0; x < width; x++)
          {
            p[0] = lutRed[*source] >> 8;
            p[1] = lutGreen[*source] >> 8;
            p[2] = lutBlue[*source] >> 8;
            source++;
            p += 3;
          }
        }

        return target.release();
      }

      case PixelFormat_RGB48:
      {
        if (r != "0\\0\\16" ||
            rc != 65536 ||
            gc != 65536 ||
            bc != 65536 ||
            pixelLength != 2 * target->GetWidth() * target->GetHeight())
        {
          throw OrthancException(ErrorCode_NotImplemented);
        }

        const uint16_t* source = reinterpret_cast<const uint16_t*>(pixelData);
        const unsigned int width = target->GetWidth();
        const unsigned int height = target->GetHeight();
        
        for (unsigned int y = 0; y < height; y++)
        {
          uint16_t* p = reinterpret_cast<uint16_t*>(target->GetRow(y));

          for (unsigned int x = 0; x < width; x++)
          {
            p[0] = lutRed[*source];
            p[1] = lutGreen[*source];
            p[2] = lutBlue[*source];
            source++;
            p += 3;
          }
        }

        return target.release();
      }

      default:
        break;
    }

    throw OrthancException(ErrorCode_InternalError);
  }                                          


  ImageAccessor* DicomImageDecoder::DecodeUncompressedImage(DcmDataset& dataset,
                                                            unsigned int frame)
  {
    /**
     * Create the target image.
     **/

    std::unique_ptr<ImageAccessor> target(CreateImage(dataset, false));

    ImageSource source;
    source.Setup(dataset, frame);

    if (source.GetWidth() != target->GetWidth() ||
        source.GetHeight() != target->GetHeight())
    {
      throw OrthancException(ErrorCode_InternalError);
    }

    
    /**
     * Deal with lookup tables
     **/

    const DicomImageInformation& info = source.GetAccessor().GetInformation();

    if (info.GetPhotometricInterpretation() == PhotometricInterpretation_Palette)
    {
      return DecodeLookupTable(target, info, dataset, NULL, 0);
    }       


    /**
     * If the format of the DICOM buffer is natively supported, use a
     * direct access to copy its values.
     **/

    bool fastVersionSuccess = false;
    PixelFormat sourceFormat;
    if (!info.IsPlanar() &&
        info.GetBitsStored() != 1 &&  // Black-and-white image, notably DICOM SEG (new in Orthanc 1.10.0)
        info.ExtractPixelFormat(sourceFormat, false))
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

          switch (ENDIANNESS)
          {
            case Endianness_Little:
              ImageProcessing::Convert(*target, sourceImage);
              break;

            case Endianness_Big:
            {
              // We cannot do byte swapping directly on the constant DcmDataset
              std::unique_ptr<ImageAccessor> copy(Image::Clone(sourceImage));
              ImageProcessing::SwapEndianness(*copy);
              ImageProcessing::Convert(*target, *copy);
              break;
            }

            default:
              throw OrthancException(ErrorCode_InternalError);
          }
            
          ImageProcessing::ShiftRight(*target, info.GetShift());
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
      switch (target->GetFormat())
      {
        case PixelFormat_RGB24:
        case PixelFormat_RGBA32:
        case PixelFormat_Grayscale8:
          CopyPixels<uint8_t>(*target, source.GetAccessor());
          break;
        
        case PixelFormat_Grayscale16:
          CopyPixels<uint16_t>(*target, source.GetAccessor());
          break;

        case PixelFormat_SignedGrayscale16:
          CopyPixels<int16_t>(*target, source.GetAccessor());
          break;

        default:
          throw OrthancException(ErrorCode_InternalError);
      }
    }

    return target.release();
  }


  static ImageAccessor* DecodePlanarConfiguration(const ImageAccessor& source)
  {
    /**
     * This function will interleave the RGB channels, if the source
     * DICOM image has the "Planar Configuration" (0028,0006) tag that
     * equals 1. This process was not applied to images using the RLE
     * codec, which led to the following issue:
     * https://groups.google.com/g/orthanc-users/c/CSVWfRasSR0/m/y1XDRXVnAgAJ
     **/

    const unsigned int height = source.GetHeight();
    const unsigned int width = source.GetWidth();
    const size_t size = static_cast<size_t>(height) * static_cast<size_t>(width);

    if (source.GetFormat() != PixelFormat_RGB24 ||
        3 * width != source.GetPitch())
    {
      throw OrthancException(ErrorCode_NotImplemented);
    }

    std::unique_ptr<ImageAccessor> target(new Image(PixelFormat_RGB24, width, height, false));

    const uint8_t* red = reinterpret_cast<const uint8_t*>(source.GetConstBuffer());
    const uint8_t* green = red + size;
    const uint8_t* blue = red + 2 * size;

    for (unsigned int y = 0; y < height; y++)
    {
      uint8_t* interleaved = reinterpret_cast<uint8_t*>(target->GetRow(y));
      for (unsigned int x = 0; x < width; x++)
      {
        interleaved[0] = *red;
        interleaved[1] = *green;
        interleaved[2] = *blue;
        interleaved += 3;
        red++;
        green++;
        blue++;
      }
    }

    return target.release();
  }


  ImageAccessor* DicomImageDecoder::ApplyCodec
  (const DcmCodec& codec,
   const DcmCodecParameter& parameters,
   const DcmRepresentationParameter& representationParameter,
   DcmDataset& dataset,
   unsigned int frame)
  {
    DcmPixelSequence* pixelSequence = FromDcmtkBridge::GetPixelSequence(dataset);
    if (pixelSequence == NULL)
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    DicomMap m;
    std::set<DicomTag> ignoreTagLength;
    FromDcmtkBridge::ExtractDicomSummary(m, dataset, DicomImageInformation::GetUsefulTagLength(), ignoreTagLength);
    DicomImageInformation info(m);

    std::unique_ptr<ImageAccessor> target(CreateImage(dataset, true));

    Uint32 startFragment = 0;  // Default 
    OFString decompressedColorModel;  // Out

    OFCondition c;
    
    if (info.GetPhotometricInterpretation() == PhotometricInterpretation_Palette &&
        info.GetChannelCount() == 1)
    {
      std::string uncompressed;
      uncompressed.resize(info.GetWidth() * info.GetHeight() * info.GetBytesPerValue());

      if (uncompressed.size() == 0 ||
          !codec.decodeFrame(&representationParameter, 
                             pixelSequence, &parameters, 
                             &dataset, frame, startFragment, &uncompressed[0],
                             uncompressed.size(), decompressedColorModel).good())
      {
        throw OrthancException(ErrorCode_BadFileFormat,
                               "Cannot decode a palette image");
      }

      return DecodeLookupTable(target, info, dataset,
                               reinterpret_cast<const uint8_t*>(uncompressed.c_str()),
                               uncompressed.size());
    }
    else
    {
      if (!codec.decodeFrame(&representationParameter, 
                             pixelSequence, &parameters, 
                             &dataset, frame, startFragment, target->GetBuffer(), 
                             target->GetSize(), decompressedColorModel).good())
      {
        throw OrthancException(ErrorCode_BadFileFormat,
                               "Cannot decode a non-palette image");
      }

      std::string colorModel = Orthanc::Toolbox::StripSpaces(decompressedColorModel.c_str());

      if (target->GetFormat() == PixelFormat_RGB24 &&
          (colorModel == "RGB" || colorModel == "YBR_FULL") &&
          info.IsPlanar())
      {
        std::unique_ptr<ImageAccessor> output(DecodePlanarConfiguration(*target));

        if (colorModel == "YBR_FULL")
        {
          ImageProcessing::ConvertJpegYCbCrToRgb(*output);
        }

        return output.release();
      }
      else
      {
        return target.release();
      }
    }
  }


  static void UndoBigEndianSwapping(ImageAccessor& decoded)
  {
    if (ENDIANNESS == Endianness_Big &&
        decoded.GetFormat() == PixelFormat_Grayscale8)
    {
      /**
       * Undo the call to "swapIfNecessary()" that is done in
       * "dcmjpeg/libsrc/djcodecd.cc" and "dcmjpls/libsrc/djcodecd.cc"
       * if "jpeg->bytesPerSample() == 1", presumably because DCMTK
       * plans for DICOM-to-DICOM conversion
       **/
      if (decoded.GetPitch() % 2 == 0)
      {
        swapBytes(decoded.GetBuffer(), decoded.GetPitch() * decoded.GetHeight(), sizeof(uint16_t));
      }
      else
      {
        throw OrthancException(ErrorCode_InternalError, "Cannot swap the bytes of an image that has an odd width");
      }
    }
  }
  

  ImageAccessor* DicomImageDecoder::Decode(DcmDataset& dataset,
                                           unsigned int frame)
  {
    E_TransferSyntax syntax = dataset.getCurrentXfer();

    /**
     * Deal with uncompressed, raw images.
     * http://support.dcmtk.org/docs/dcxfer_8h-source.html
     **/
    if (syntax == EXS_Unknown ||
        syntax == EXS_LittleEndianImplicit ||
        syntax == EXS_BigEndianImplicit ||
        syntax == EXS_LittleEndianExplicit ||
        syntax == EXS_BigEndianExplicit)
    {
      return DecodeUncompressedImage(dataset, frame);
    }


#if ORTHANC_ENABLE_DCMTK_JPEG_LOSSLESS == 1
    /**
     * Deal with JPEG-LS images.
     **/

    if (syntax == EXS_JPEGLSLossless ||
        syntax == EXS_JPEGLSLossy)
    {
      // The (2, OFTrue) are the default parameters as found in DCMTK 3.6.2
      // http://support.dcmtk.org/docs/classDJLSRepresentationParameter.html
      DJLSRepresentationParameter representationParameter(2, OFTrue);

      DJLSCodecParameter parameters;
      std::unique_ptr<DJLSDecoderBase> decoder;

      switch (syntax)
      {
        case EXS_JPEGLSLossless:
          LOG(INFO) << "Decoding a JPEG-LS lossless DICOM image";
          decoder.reset(new DJLSLosslessDecoder);
          break;
          
        case EXS_JPEGLSLossy:
          LOG(INFO) << "Decoding a JPEG-LS near-lossless DICOM image";
          decoder.reset(new DJLSNearLosslessDecoder);
          break;

        default:
          throw OrthancException(ErrorCode_InternalError);
      }
    
      std::unique_ptr<ImageAccessor> result(ApplyCodec(*decoder, parameters, representationParameter, dataset, frame));
      UndoBigEndianSwapping(*result);  // New in Orthanc 1.9.1 to decode on big-endian architectures
      return result.release();
    }
#endif


#if ORTHANC_ENABLE_DCMTK_JPEG == 1
    /**
     * Deal with JPEG images.
     **/

    if (syntax == EXS_JPEGProcess1     ||  // DJDecoderBaseline
        syntax == EXS_JPEGProcess2_4   ||  // DJDecoderExtended
        syntax == EXS_JPEGProcess6_8   ||  // DJDecoderSpectralSelection (retired)
        syntax == EXS_JPEGProcess10_12 ||  // DJDecoderProgressive (retired)
        syntax == EXS_JPEGProcess14    ||  // DJDecoderLossless
        syntax == EXS_JPEGProcess14SV1)    // DJDecoderP14SV1
    {
      // http://support.dcmtk.org/docs-snapshot/djutils_8h.html#a2a9695e5b6b0f5c45a64c7f072c1eb9d
      DJCodecParameter parameters(
        ECC_lossyYCbCr,  // Mode for color conversion for compression, Unused for decompression
        EDC_photometricInterpretation,  // Perform color space conversion from YCbCr to RGB if DICOM photometric interpretation indicates YCbCr
        EUC_default,     // Mode for UID creation, unused for decompression
        EPC_default);    // Automatically determine whether color-by-plane is required from the SOP Class UID and decompressed photometric interpretation
      DJ_RPLossy representationParameter;
      std::unique_ptr<DJCodecDecoder> decoder;

      switch (syntax)
      {
        case EXS_JPEGProcess1:
          LOG(INFO) << "Decoding a JPEG baseline (process 1) DICOM image";
          decoder.reset(new DJDecoderBaseline);
          break;
          
        case EXS_JPEGProcess2_4 :
          LOG(INFO) << "Decoding a JPEG baseline (processes 2 and 4) DICOM image";
          decoder.reset(new DJDecoderExtended);
          break;
          
        case EXS_JPEGProcess6_8:   // Retired
          LOG(INFO) << "Decoding a JPEG spectral section, nonhierarchical (processes 6 and 8) DICOM image";
          decoder.reset(new DJDecoderSpectralSelection);
          break;
          
        case EXS_JPEGProcess10_12:   // Retired
          LOG(INFO) << "Decoding a JPEG full progression, nonhierarchical (processes 10 and 12) DICOM image";
          decoder.reset(new DJDecoderProgressive);
          break;
          
        case EXS_JPEGProcess14:
          LOG(INFO) << "Decoding a JPEG lossless, nonhierarchical (process 14) DICOM image";
          decoder.reset(new DJDecoderLossless);
          break;
          
        case EXS_JPEGProcess14SV1:
          LOG(INFO) << "Decoding a JPEG lossless, nonhierarchical, first-order prediction (process 14 selection value 1) DICOM image";
          decoder.reset(new DJDecoderP14SV1);
          break;
          
        default:
          throw OrthancException(ErrorCode_InternalError);
      }

      std::unique_ptr<ImageAccessor> result(ApplyCodec(*decoder, parameters, representationParameter, dataset, frame));
      UndoBigEndianSwapping(*result);  // New in Orthanc 1.9.1 to decode on big-endian architectures
      return result.release();
    }
#endif


    if (syntax == EXS_RLELossless)
    {
      LOG(INFO) << "Decoding a RLE lossless DICOM image";
      DcmRLECodecParameter parameters;
      DcmRLECodecDecoder decoder;
      DcmRLERepresentationParameter representationParameter;
      return ApplyCodec(decoder, parameters, representationParameter, dataset, frame);
    }


    /**
     * This DICOM image format is not natively supported by
     * Orthanc. As a last resort, try and decode it through DCMTK by
     * converting its transfer syntax to Little Endian. This will
     * result in higher memory consumption. This is actually the
     * second example of the following page:
     * http://support.dcmtk.org/docs/mod_dcmjpeg.html#Examples
     **/
    
    {
      LOG(INFO) << "Trying to decode a compressed image by transcoding it to Little Endian Explicit";

      std::unique_ptr<DcmDataset> converted(dynamic_cast<DcmDataset*>(dataset.clone()));
      converted->chooseRepresentation(EXS_LittleEndianExplicit, NULL);

      if (converted->canWriteXfer(EXS_LittleEndianExplicit))
      {
        return DecodeUncompressedImage(*converted, frame);
      }
    }

    DicomTransferSyntax s;
    if (FromDcmtkBridge::LookupOrthancTransferSyntax(s, dataset.getCurrentXfer()))
    {
      throw OrthancException(ErrorCode_NotImplemented,
                             "The built-in DCMTK decoder cannot decode some DICOM instance "
                             "whose transfer syntax is: " + std::string(GetTransferSyntaxUid(s)));
    }
    else
    {
      throw OrthancException(ErrorCode_NotImplemented,
                             "The built-in DCMTK decoder cannot decode some DICOM instance");
    }
  }


  static bool IsColorImage(PixelFormat format)
  {
    return (format == PixelFormat_RGB24 ||
            format == PixelFormat_RGBA32);
  }


  bool DicomImageDecoder::TruncateDecodedImage(std::unique_ptr<ImageAccessor>& image,
                                               PixelFormat format,
                                               bool allowColorConversion)
  {
    // If specified, prevent the conversion between color and
    // grayscale images
    bool isSourceColor = IsColorImage(image->GetFormat());
    bool isTargetColor = IsColorImage(format);

    if (!allowColorConversion)
    {
      if (isSourceColor ^ isTargetColor)
      {
        return false;
      }
    }

    if (image->GetFormat() != format)
    {
      // A conversion is required
      std::unique_ptr<ImageAccessor> target
        (new Image(format, image->GetWidth(), image->GetHeight(), false));
      ImageProcessing::Convert(*target, *image);

#if __cplusplus < 201103L
      image.reset(target.release());
#else
      image = std::move(target);
#endif
    }

    return true;
  }


  bool DicomImageDecoder::PreviewDecodedImage(std::unique_ptr<ImageAccessor>& image)
  {
    switch (image->GetFormat())
    {
      case PixelFormat_RGB24:
      {
        // Directly return color images without modification (RGB)
        return true;
      }

      case PixelFormat_RGB48:
      {
        std::unique_ptr<ImageAccessor> target
          (new Image(PixelFormat_RGB24, image->GetWidth(), image->GetHeight(), false));
        ImageProcessing::Convert(*target, *image);

#if __cplusplus < 201103L
        image.reset(target.release());
#else
        image = std::move(target);
#endif

        return true;
      }

      case PixelFormat_Grayscale8:
      case PixelFormat_Grayscale16:
      case PixelFormat_SignedGrayscale16:
      {
        // Grayscale image: Stretch its dynamics to the [0,255] range
        int64_t a, b;
        ImageProcessing::GetMinMaxIntegerValue(a, b, *image);

        if (a == b)
        {
          ImageProcessing::Set(*image, 0);
        }
        else
        {
          ImageProcessing::ShiftScale(*image, static_cast<float>(-a),
                                      255.0f / static_cast<float>(b - a),
                                      true /* TODO - Consider using "false" to speed up */);
        }

        // If the source image is not grayscale 8bpp, convert it
        if (image->GetFormat() != PixelFormat_Grayscale8)
        {
          std::unique_ptr<ImageAccessor> target
            (new Image(PixelFormat_Grayscale8, image->GetWidth(), image->GetHeight(), false));
          ImageProcessing::Convert(*target, *image);

#if __cplusplus < 201103L
          image.reset(target.release());
#else
          image = std::move(target);
#endif
        }

        return true;
      }
      
      default:
        throw OrthancException(ErrorCode_NotImplemented);
    }
  }


  void DicomImageDecoder::ApplyExtractionMode(std::unique_ptr<ImageAccessor>& image,
                                              ImageExtractionMode mode,
                                              bool invert)
  {
    if (image.get() == NULL)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    bool ok = false;

    switch (mode)
    {
      case ImageExtractionMode_UInt8:
        ok = TruncateDecodedImage(image, PixelFormat_Grayscale8, false);
        break;

      case ImageExtractionMode_UInt16:
        ok = TruncateDecodedImage(image, PixelFormat_Grayscale16, false);
        break;

      case ImageExtractionMode_Int16:
        ok = TruncateDecodedImage(image, PixelFormat_SignedGrayscale16, false);
        break;

      case ImageExtractionMode_Preview:
        ok = PreviewDecodedImage(image);
        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    if (ok)
    {
      assert(image.get() != NULL);

      if (invert)
      {
        ImageProcessing::Invert(*image);
      }
    }
    else
    {
      throw OrthancException(ErrorCode_NotImplemented);
    }
  }


  void DicomImageDecoder::ExtractPamImage(std::string& result,
                                          std::unique_ptr<ImageAccessor>& image,
                                          ImageExtractionMode mode,
                                          bool invert)
  {
    ApplyExtractionMode(image, mode, invert);

    PamWriter writer;
    IImageWriter::WriteToMemory(writer, result, *image);
  }

#if ORTHANC_ENABLE_PNG == 1
  void DicomImageDecoder::ExtractPngImage(std::string& result,
                                          std::unique_ptr<ImageAccessor>& image,
                                          ImageExtractionMode mode,
                                          bool invert)
  {
    ApplyExtractionMode(image, mode, invert);

    PngWriter writer;
    IImageWriter::WriteToMemory(writer, result, *image);
  }
#endif


#if ORTHANC_ENABLE_JPEG == 1
  void DicomImageDecoder::ExtractJpegImage(std::string& result,
                                           std::unique_ptr<ImageAccessor>& image,
                                           ImageExtractionMode mode,
                                           bool invert,
                                           uint8_t quality)
  {
    if (mode != ImageExtractionMode_UInt8 &&
        mode != ImageExtractionMode_Preview)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    ApplyExtractionMode(image, mode, invert);

    JpegWriter writer;
    writer.SetQuality(quality);
    IImageWriter::WriteToMemory(writer, result, *image);
  }
#endif


#if ORTHANC_BUILDING_FRAMEWORK_LIBRARY == 1
  ImageAccessor *DicomImageDecoder::Decode(ParsedDicomFile& dataset,
                                           unsigned int frame)
  {
    return Decode(*dataset.GetDcmtkObject().getDataset(), frame);
  }
#endif
}
