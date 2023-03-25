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


#include "../PrecompiledHeaders.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "DicomIntegerPixelAccessor.h"

#include "../OrthancException.h"
#include <boost/lexical_cast.hpp>
#include <limits>
#include <cassert>
#include <stdio.h>

namespace Orthanc
{
  DicomIntegerPixelAccessor::DicomIntegerPixelAccessor(const DicomMap& values,
                                                       const void* pixelData,
                                                       size_t size) :
    information_(values),
    pixelData_(pixelData),
    size_(size)
  {
    if (information_.GetBitsAllocated() > 32 ||
        information_.GetBitsStored() >= 32)
    {
      // Not available, as the accessor internally uses int32_t values
      throw OrthancException(ErrorCode_NotImplemented);
    }

    frame_ = 0;
    frameOffset_ = information_.GetFrameSize();

    if (information_.GetNumberOfFrames() * frameOffset_ > size)
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    if (information_.IsSigned())
    {
      // Pixels are signed
      mask_ = (1 << (information_.GetBitsStored() - 1)) - 1;
      signMask_ = (1 << (information_.GetBitsStored() - 1));
    }
    else
    {
      // Pixels are unsigned
      mask_ = (1 << information_.GetBitsStored()) - 1;
      signMask_ = 0;
    }

    if (information_.IsPlanar())
    {
      /**
       * Each color plane shall be sent contiguously. For RGB images,
       * this means the order of the pixel values sent is R1, R2, R3,
       * ..., G1, G2, G3, ..., B1, B2, B3, etc.
       **/
      rowOffset_ = information_.GetWidth() * information_.GetBytesPerValue();
    }
    else
    {
      /**
       * The sample values for the first pixel are followed by the
       * sample values for the second pixel, etc. For RGB images, this
       * means the order of the pixel values sent shall be R1, G1, B1,
       * R2, G2, B2, ..., etc.
       **/
      if (information_.GetBitsStored() == 1)
      {
        if (information_.GetChannelCount() == 1 &&
            information_.GetBitsAllocated() == 1)
        {
          assert(information_.GetWidth() % 8 == 0);  // Tested by DicomImageInformation
          rowOffset_ = information_.GetWidth() / 8;
        }
        else
        {
          throw OrthancException(ErrorCode_IncompatibleImageFormat,
                                 "Image not supported (multi-channel black-and-image image)");
        }
      }
      else
      {
        rowOffset_ = information_.GetWidth() * information_.GetBytesPerValue() * information_.GetChannelCount();
      }
    }
  }


  void DicomIntegerPixelAccessor::GetExtremeValues(int32_t& min, 
                                                   int32_t& max) const
  {
    if (information_.GetHeight() == 0 || information_.GetWidth() == 0)
    {
      min = max = 0;
      return;
    }

    min = std::numeric_limits<int32_t>::max();
    max = std::numeric_limits<int32_t>::min();

    const unsigned int height = information_.GetHeight();
    const unsigned int width = information_.GetWidth();
    const unsigned int channels = information_.GetChannelCount();
    
    for (unsigned int y = 0; y < height; y++)
    {
      for (unsigned int x = 0; x < width; x++)
      {
        for (unsigned int c = 0; c < channels; c++)
        {
          int32_t v = GetValue(x, y, c);
          if (v < min)
            min = v;
          if (v > max)
            max = v;
        }
      }
    }
  }


  int32_t DicomIntegerPixelAccessor::GetValue(unsigned int x, 
                                              unsigned int y,
                                              unsigned int channel) const
  {
    assert(x < information_.GetWidth() && 
           y < information_.GetHeight() && 
           channel < information_.GetChannelCount());

    const uint8_t* pixel = (reinterpret_cast<const uint8_t*>(pixelData_) + 
                            y * rowOffset_ + frame_ * frameOffset_);
    
    if (information_.GetBitsStored() == 1)
    {
      // New in Orthanc 1.10.0, notably for DICOM SEG
      assert(information_.GetBitsAllocated() == 1 &&
             information_.GetChannelCount() == 1 &&
             !information_.IsPlanar());
      
      uint8_t b = pixel[x / 8];

      if (b & (1 << (x % 8)))
      {
        return 255;
      }
      else
      {
        return 0;
      }
    }
    else
    {
      // http://dicom.nema.org/medical/dicom/current/output/html/part03.html#sect_C.7.6.3.1.3
      if (information_.IsPlanar())
      {
        /**
         * Each color plane shall be sent contiguously. For RGB images,
         * this means the order of the pixel values sent is R1, R2, R3,
         * ..., G1, G2, G3, ..., B1, B2, B3, etc.
         **/
        assert(frameOffset_ % information_.GetChannelCount() == 0);
        pixel += channel * frameOffset_ / information_.GetChannelCount() + x * information_.GetBytesPerValue();
      }
      else
      {
        /**
         * The sample values for the first pixel are followed by the
         * sample values for the second pixel, etc. For RGB images, this
         * means the order of the pixel values sent shall be R1, G1, B1,
         * R2, G2, B2, ..., etc.
         **/
        pixel += channel * information_.GetBytesPerValue() + x * information_.GetChannelCount() * information_.GetBytesPerValue();
      }

      uint32_t v;
      v = pixel[0];
      if (information_.GetBytesPerValue() >= 2)
        v = v + (static_cast<uint32_t>(pixel[1]) << 8);
      if (information_.GetBytesPerValue() >= 3)
        v = v + (static_cast<uint32_t>(pixel[2]) << 16);
      if (information_.GetBytesPerValue() >= 4)
        v = v + (static_cast<uint32_t>(pixel[3]) << 24);

      v = v >> information_.GetShift();

      if (v & signMask_)
      {
        // Signed value
        // http://en.wikipedia.org/wiki/Two%27s_complement#Subtraction_from_2N
        return -static_cast<int32_t>(mask_) + static_cast<int32_t>(v & mask_) - 1;
      }
      else
      {
        // Unsigned value
        return static_cast<int32_t>(v & mask_);
      }
    }
  }


  void DicomIntegerPixelAccessor::SetCurrentFrame(unsigned int frame)
  {
    if (frame >= information_.GetNumberOfFrames())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    frame_ = frame;
  }

}
