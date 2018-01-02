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


#include "../PrecompiledHeaders.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "DicomImageInformation.h"

#include "../OrthancException.h"
#include "../Toolbox.h"
#include <boost/lexical_cast.hpp>
#include <limits>
#include <cassert>
#include <stdio.h>

namespace Orthanc
{
  DicomImageInformation::DicomImageInformation(const DicomMap& values)
  {
    unsigned int pixelRepresentation;
    unsigned int planarConfiguration = 0;

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

      width_ = boost::lexical_cast<unsigned int>(values.GetValue(DICOM_TAG_COLUMNS).GetContent());
      height_ = boost::lexical_cast<unsigned int>(values.GetValue(DICOM_TAG_ROWS).GetContent());
      bitsAllocated_ = boost::lexical_cast<unsigned int>(values.GetValue(DICOM_TAG_BITS_ALLOCATED).GetContent());

      try
      {
        samplesPerPixel_ = boost::lexical_cast<unsigned int>(values.GetValue(DICOM_TAG_SAMPLES_PER_PIXEL).GetContent());
      }
      catch (OrthancException&)
      {
        samplesPerPixel_ = 1;  // Assume 1 color channel
      }

      try
      {
        bitsStored_ = boost::lexical_cast<unsigned int>(values.GetValue(DICOM_TAG_BITS_STORED).GetContent());
      }
      catch (OrthancException&)
      {
        bitsStored_ = bitsAllocated_;
      }

      try
      {
        highBit_ = boost::lexical_cast<unsigned int>(values.GetValue(DICOM_TAG_HIGH_BIT).GetContent());
      }
      catch (OrthancException&)
      {
        highBit_ = bitsStored_ - 1;
      }

      try
      {
        pixelRepresentation = boost::lexical_cast<unsigned int>(values.GetValue(DICOM_TAG_PIXEL_REPRESENTATION).GetContent());
      }
      catch (OrthancException&)
      {
        pixelRepresentation = 0;  // Assume unsigned pixels
      }

      if (samplesPerPixel_ > 1)
      {
        // The "Planar Configuration" is only set when "Samples per Pixels" is greater than 1
        // http://dicom.nema.org/medical/dicom/current/output/html/part03.html#sect_C.7.6.3.1.3
        try
        {
          planarConfiguration = boost::lexical_cast<unsigned int>(values.GetValue(DICOM_TAG_PLANAR_CONFIGURATION).GetContent());
        }
        catch (OrthancException&)
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
      try
      {
        numberOfFrames_ = boost::lexical_cast<unsigned int>(values.GetValue(DICOM_TAG_NUMBER_OF_FRAMES).GetContent());
      }
      catch (boost::bad_lexical_cast&)
      {
        throw OrthancException(ErrorCode_NotImplemented);
      }
    }
    else
    {
      numberOfFrames_ = 1;
    }

    if ((bitsAllocated_ != 8 && bitsAllocated_ != 16 && 
         bitsAllocated_ != 24 && bitsAllocated_ != 32) ||
        numberOfFrames_ == 0 ||
        (planarConfiguration != 0 && planarConfiguration != 1))
    {
      throw OrthancException(ErrorCode_NotImplemented);
    }

    if (samplesPerPixel_ == 0)
    {
      throw OrthancException(ErrorCode_NotImplemented);
    }

    bytesPerValue_ = bitsAllocated_ / 8;

    isPlanar_ = (planarConfiguration != 0 ? true : false);
    isSigned_ = (pixelRepresentation != 0 ? true : false);
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
    }

    if (GetBitsStored() == 8 && 
        GetChannelCount() == 3 && 
        !IsSigned() &&
        (ignorePhotometricInterpretation || photometric_ == PhotometricInterpretation_RGB))
    {
      format = PixelFormat_RGB24;
      return true;
    }

    return false;
  }


  size_t DicomImageInformation::GetFrameSize() const
  {
    return (GetHeight() * 
            GetWidth() * 
            GetBytesPerValue() * 
            GetChannelCount());
  }
}
