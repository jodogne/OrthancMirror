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


#include "../PrecompiledHeaders.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "DicomImageInformation.h"

#include "../Compatibility.h"
#include "../Logging.h"
#include "../OrthancException.h"
#include "../Toolbox.h"

#include <boost/lexical_cast.hpp>
#include <limits>
#include <cassert>
#include <stdio.h>
#include <memory>

namespace Orthanc
{
  DicomImageInformation::DicomImageInformation(const DicomMap& values)
  {
    std::string sopClassUid;
    if (values.LookupStringValue(sopClassUid, DICOM_TAG_SOP_CLASS_UID, false))
    {
      sopClassUid = Toolbox::StripSpaces(sopClassUid);
      if (sopClassUid == "1.2.840.10008.5.1.4.1.1.481.3" /* RT-STRUCT */)
      {
        LOG(WARNING) << "Orthanc::DicomImageInformation() should not be applied to SOP Class UID: " << sopClassUid;
      }
    }

    uint32_t pixelRepresentation = 0;
    uint32_t planarConfiguration = 0;

    try
    {
      std::string p = values.GetValue(DICOM_TAG_PHOTOMETRIC_INTERPRETATION).GetContent();
      Toolbox::ToUpperCase(p);

      if (p == "RGB")
      {
        photometric_ = PhotometricInterpretation_RGB;
      }
      else if (p == "MONOCHROME1")
      {
        photometric_ = PhotometricInterpretation_Monochrome1;
      }
      else if (p == "MONOCHROME2")
      {
        photometric_ = PhotometricInterpretation_Monochrome2;
      }
      else if (p == "PALETTE COLOR")
      {
        photometric_ = PhotometricInterpretation_Palette;
      }
      else if (p == "HSV")
      {
        photometric_ = PhotometricInterpretation_HSV;
      }
      else if (p == "ARGB")
      {
        photometric_ = PhotometricInterpretation_ARGB;
      }
      else if (p == "CMYK")
      {
        photometric_ = PhotometricInterpretation_CMYK;
      }
      else if (p == "YBR_FULL")
      {
        photometric_ = PhotometricInterpretation_YBRFull;
      }
      else if (p == "YBR_FULL_422")
      {
        photometric_ = PhotometricInterpretation_YBRFull422;
      }
      else if (p == "YBR_PARTIAL_420")
      {
        photometric_ = PhotometricInterpretation_YBRPartial420;
      }
      else if (p == "YBR_PARTIAL_422")
      {
        photometric_ = PhotometricInterpretation_YBRPartial422;
      }
      else if (p == "YBR_ICT")
      {
        photometric_ = PhotometricInterpretation_YBR_ICT;
      }
      else if (p == "YBR_RCT")
      {
        photometric_ = PhotometricInterpretation_YBR_RCT;
      }
      else
      {
        photometric_ = PhotometricInterpretation_Unknown;
      }

      values.GetValue(DICOM_TAG_COLUMNS).ParseFirstUnsignedInteger(width_); // in some US images, we've seen tag values of "800\0"; that's why we parse the 'first' value
      values.GetValue(DICOM_TAG_ROWS).ParseFirstUnsignedInteger(height_);

      if (!values.ParseUnsignedInteger32(bitsAllocated_, DICOM_TAG_BITS_ALLOCATED))
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }

      if (!values.ParseUnsignedInteger32(samplesPerPixel_, DICOM_TAG_SAMPLES_PER_PIXEL))
      {
        samplesPerPixel_ = 1;  // Assume 1 color channel
      }

      if (!values.ParseUnsignedInteger32(bitsStored_, DICOM_TAG_BITS_STORED))
      {
        bitsStored_ = bitsAllocated_;
      }

      if (bitsStored_ > bitsAllocated_)
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }

      if (!values.ParseUnsignedInteger32(highBit_, DICOM_TAG_HIGH_BIT))
      {
        highBit_ = bitsStored_ - 1;
      }

      if (!values.ParseUnsignedInteger32(pixelRepresentation, DICOM_TAG_PIXEL_REPRESENTATION))
      {
        pixelRepresentation = 0;  // Assume unsigned pixels
      }

      if (samplesPerPixel_ > 1)
      {
        // The "Planar Configuration" is only set when "Samples per Pixels" is greater than 1
        // http://dicom.nema.org/medical/dicom/current/output/html/part03.html#sect_C.7.6.3.1.3

        if (!values.ParseUnsignedInteger32(planarConfiguration, DICOM_TAG_PLANAR_CONFIGURATION))
        {
          planarConfiguration = 0;  // Assume interleaved color channels
        }
      }
    }
    catch (boost::bad_lexical_cast&)
    {
      throw OrthancException(ErrorCode_NotImplemented);
    }
    catch (OrthancException&)
    {
      throw OrthancException(ErrorCode_NotImplemented);
    }

    
    if (values.HasTag(DICOM_TAG_NUMBER_OF_FRAMES))
    {
      if (!values.ParseUnsignedInteger32(numberOfFrames_, DICOM_TAG_NUMBER_OF_FRAMES))
      {
        throw OrthancException(ErrorCode_NotImplemented);
      }
    }
    else
    {
      numberOfFrames_ = 1;
    }

    if (bitsAllocated_ != 8 && bitsAllocated_ != 16 &&
        bitsAllocated_ != 24 && bitsAllocated_ != 32 &&
        bitsAllocated_ != 1 /* new in Orthanc 1.10.0 */)
    {
      throw OrthancException(ErrorCode_IncompatibleImageFormat, "Image not supported: " + boost::lexical_cast<std::string>(bitsAllocated_) + " bits allocated");
    }
    else if (numberOfFrames_ == 0)
    {
      throw OrthancException(ErrorCode_IncompatibleImageFormat, "Image not supported (no frames)");
    }
    else if (planarConfiguration != 0 && planarConfiguration != 1)
    {
      throw OrthancException(ErrorCode_IncompatibleImageFormat, "Image not supported: planar configuration is " + boost::lexical_cast<std::string>(planarConfiguration));
    }

    if (samplesPerPixel_ == 0)
    {
      throw OrthancException(ErrorCode_IncompatibleImageFormat, "Image not supported: samples per pixel is 0");
    }

    if (bitsStored_ == 1)
    {
      // This is the case of DICOM SEG, new in Orthanc 1.10.0
      if (bitsAllocated_ != 1)
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }
      else if (width_ % 8 != 0)
      {
        throw OrthancException(ErrorCode_BadFileFormat, "Bad number of columns for a black-and-white image");
      }
      else
      {
        bytesPerValue_ = 0;  // Arbitrary initialization
      }
    }
    else
    {
      bytesPerValue_ = bitsAllocated_ / 8;
    }

    isPlanar_ = (planarConfiguration != 0 ? true : false);
    isSigned_ = (pixelRepresentation != 0 ? true : false);
  }

  DicomImageInformation* DicomImageInformation::Clone() const
  {
    std::unique_ptr<DicomImageInformation> target(new DicomImageInformation);
    target->width_ = width_;
    target->height_ = height_;
    target->samplesPerPixel_ = samplesPerPixel_;
    target->numberOfFrames_ = numberOfFrames_;
    target->isPlanar_ = isPlanar_;
    target->isSigned_ = isSigned_;
    target->bytesPerValue_ = bytesPerValue_;
    target->bitsAllocated_ = bitsAllocated_;
    target->bitsStored_ = bitsStored_;
    target->highBit_ = highBit_;
    target->photometric_ = photometric_;

    return target.release();
  }

  unsigned int DicomImageInformation::GetWidth() const
  {
    return width_;
  }

  unsigned int DicomImageInformation::GetHeight() const
  {
    return height_;
  }

  unsigned int DicomImageInformation::GetNumberOfFrames() const
  {
    return numberOfFrames_;
  }

  unsigned int DicomImageInformation::GetChannelCount() const
  {
    return samplesPerPixel_;
  }

  unsigned int DicomImageInformation::GetBitsStored() const
  {
    return bitsStored_;
  }

  size_t DicomImageInformation::GetBytesPerValue() const
  {
    if (bitsStored_ == 1)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls,
                             "This call is incompatible with black-and-white images");
    }
    else
    {
      assert(bitsAllocated_ >= 8);
      return bytesPerValue_;
    }
  }

  bool DicomImageInformation::IsSigned() const
  {
    return isSigned_;
  }

  unsigned int DicomImageInformation::GetBitsAllocated() const
  {
    return bitsAllocated_;
  }

  unsigned int DicomImageInformation::GetHighBit() const
  {
    return highBit_;
  }

  bool DicomImageInformation::IsPlanar() const
  {
    return isPlanar_;
  }

  unsigned int DicomImageInformation::GetShift() const
  {
    return highBit_ + 1 - bitsStored_;
  }

  PhotometricInterpretation DicomImageInformation::GetPhotometricInterpretation() const
  {
    return photometric_;
  }

  bool DicomImageInformation::ExtractPixelFormat(PixelFormat& format,
                                                 bool ignorePhotometricInterpretation) const
  {
    if (photometric_ == PhotometricInterpretation_Palette)
    {
      if (GetBitsStored() == 8 && GetChannelCount() == 1 && !IsSigned())
      {
        format = PixelFormat_RGB24;
        return true;
      }

      if (GetBitsStored() == 16 && GetChannelCount() == 1 && !IsSigned())
      {
        format = PixelFormat_RGB48;
        return true;
      }
    }
    
    if (ignorePhotometricInterpretation ||
        photometric_ == PhotometricInterpretation_Monochrome1 ||
        photometric_ == PhotometricInterpretation_Monochrome2)
    {
      if (GetBitsStored() == 8 && GetChannelCount() == 1 && !IsSigned())
      {
        format = PixelFormat_Grayscale8;
        return true;
      }
      
      if (GetBitsAllocated() == 16 && GetChannelCount() == 1 && !IsSigned())
      {
        format = PixelFormat_Grayscale16;
        return true;
      }

      if (GetBitsAllocated() == 16 && GetChannelCount() == 1 && IsSigned())
      {
        format = PixelFormat_SignedGrayscale16;
        return true;
      }
      
      if (GetBitsAllocated() == 32 && GetChannelCount() == 1 && !IsSigned())
      {
        format = PixelFormat_Grayscale32;
        return true;
      }

      if (GetBitsStored() == 1 && GetChannelCount() == 1 && !IsSigned())
      {
        // This is the case of DICOM SEG, new in Orthanc 1.10.0
        format = PixelFormat_Grayscale8;
        return true;
      }
    }

    if (GetBitsStored() == 8 &&
        GetChannelCount() == 3 &&
        !IsSigned() &&
        (ignorePhotometricInterpretation || photometric_ == PhotometricInterpretation_RGB))
    {
      format = PixelFormat_RGB24;
      return true;
    }

    if (GetBitsStored() == 16 &&
        GetChannelCount() == 3 &&
        !IsSigned() &&
        (ignorePhotometricInterpretation || photometric_ == PhotometricInterpretation_RGB))
    {
      format = PixelFormat_RGB48;
      return true;
    }

    return false;
  }


  size_t DicomImageInformation::GetFrameSize() const
  {
    if (bitsStored_ == 1)
    {
      assert(GetWidth() % 8 == 0);
      
      if (GetChannelCount() == 1)
      {
        return GetHeight() * GetWidth() / 8;
      }
      else
      {
        throw OrthancException(ErrorCode_IncompatibleImageFormat,
                               "Image not supported (multi-channel black-and-image image)");
      }
    }
    else
    {
      return (GetHeight() *
              GetWidth() *
              GetBytesPerValue() *
              GetChannelCount());
    }
  }


  unsigned int DicomImageInformation::GetUsefulTagLength()
  {
    return 256;
  }
}
