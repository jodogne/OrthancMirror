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


#include "../PrecompiledHeaders.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "DicomImageInformation.h"

#include "../OrthancException.h"
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
      width_ = boost::lexical_cast<unsigned int>(values.GetValue(DICOM_TAG_COLUMNS).AsString());
      height_ = boost::lexical_cast<unsigned int>(values.GetValue(DICOM_TAG_ROWS).AsString());
      bitsAllocated_ = boost::lexical_cast<unsigned int>(values.GetValue(DICOM_TAG_BITS_ALLOCATED).AsString());

      try
      {
        samplesPerPixel_ = boost::lexical_cast<unsigned int>(values.GetValue(DICOM_TAG_SAMPLES_PER_PIXEL).AsString());
      }
      catch (OrthancException&)
      {
        samplesPerPixel_ = 1;  // Assume 1 color channel
      }

      try
      {
        bitsStored_ = boost::lexical_cast<unsigned int>(values.GetValue(DICOM_TAG_BITS_STORED).AsString());
      }
      catch (OrthancException&)
      {
        bitsStored_ = bitsAllocated_;
      }

      try
      {
        highBit_ = boost::lexical_cast<unsigned int>(values.GetValue(DICOM_TAG_HIGH_BIT).AsString());
      }
      catch (OrthancException&)
      {
        highBit_ = bitsStored_ - 1;
      }

      try
      {
        pixelRepresentation = boost::lexical_cast<unsigned int>(values.GetValue(DICOM_TAG_PIXEL_REPRESENTATION).AsString());
      }
      catch (OrthancException&)
      {
        pixelRepresentation = 0;  // Assume unsigned pixels
      }

      if (samplesPerPixel_ > 1)
      {
        // The "Planar Configuration" is only set when "Samples per Pixels" is greater than 1
        // https://www.dabsoft.ch/dicom/3/C.7.6.3.1.3/
        try
        {
          planarConfiguration = boost::lexical_cast<unsigned int>(values.GetValue(DICOM_TAG_PLANAR_CONFIGURATION).AsString());
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

    try
    {
      numberOfFrames_ = boost::lexical_cast<unsigned int>(values.GetValue(DICOM_TAG_NUMBER_OF_FRAMES).AsString());
    }
    catch (OrthancException)
    {
      // If the tag "NumberOfFrames" is absent, assume there is a single frame
      numberOfFrames_ = 1;
    }
    catch (boost::bad_lexical_cast&)
    {
      throw OrthancException(ErrorCode_NotImplemented);
    }

    if ((bitsAllocated_ != 8 && bitsAllocated_ != 16 && 
         bitsAllocated_ != 24 && bitsAllocated_ != 32) ||
        numberOfFrames_ == 0 ||
        (planarConfiguration != 0 && planarConfiguration != 1))
    {
      throw OrthancException(ErrorCode_NotImplemented);
    }

    if (bitsAllocated_ > 32 ||
        bitsStored_ >= 32)
    {
      // Not available, as the accessor internally uses int32_t values
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


  bool DicomImageInformation::ExtractPixelFormat(PixelFormat& format) const
  {
    if (IsPlanar())
    {
      return false;
    }

    if (GetBitsStored() == 8 && GetChannelCount() == 1 && !IsSigned())
    {
      format = PixelFormat_Grayscale8;
      return true;
    }

    if (GetBitsStored() == 8 && GetChannelCount() == 3 && !IsSigned())
    {
      format = PixelFormat_RGB24;
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

    return false;
  }
}
