/**
 * Palanthir - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012 Medical Physics Department, CHU of Liege,
 * Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#include "DicomIntegerPixelAccessor.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "../Core/PalanthirException.h"
#include "FromDcmtkBridge.h"
#include <boost/lexical_cast.hpp>
#include <limits>

namespace Palanthir
{
  DicomIntegerPixelAccessor::DicomIntegerPixelAccessor(const DicomMap& values,
                                                       const void* pixelData,
                                                       size_t size) :
    pixelData_(pixelData),
    size_(size)
  {
    unsigned int bitsAllocated;
    unsigned int bitsStored;
    unsigned int highBit;
    unsigned int pixelRepresentation;

    try
    {
      width_ = boost::lexical_cast<unsigned int>(FromDcmtkBridge::GetValue(values, "Columns").AsString());
      height_ = boost::lexical_cast<unsigned int>(FromDcmtkBridge::GetValue(values, "Rows").AsString());
      samplesPerPixel_ = boost::lexical_cast<unsigned int>(FromDcmtkBridge::GetValue(values, "SamplesPerPixel").AsString());
      bitsAllocated = boost::lexical_cast<unsigned int>(FromDcmtkBridge::GetValue(values, "BitsAllocated").AsString());
      bitsStored = boost::lexical_cast<unsigned int>(FromDcmtkBridge::GetValue(values, "BitsStored").AsString());
      highBit = boost::lexical_cast<unsigned int>(FromDcmtkBridge::GetValue(values, "HighBit").AsString());
      pixelRepresentation = boost::lexical_cast<unsigned int>(FromDcmtkBridge::GetValue(values, "PixelRepresentation").AsString());
    }
    catch (boost::bad_lexical_cast)
    {
      throw PalanthirException(ErrorCode_NotImplemented);
    }

    frame_ = 0;
    try
    {
      numberOfFrames_ = boost::lexical_cast<unsigned int>(FromDcmtkBridge::GetValue(values, "NumberOfFrames").AsString());
    }
    catch (PalanthirException)
    {
      // If the tag "NumberOfFrames" is absent, assume there is a single frame
      numberOfFrames_ = 1;
    }
    catch (boost::bad_lexical_cast)
    {
      throw PalanthirException(ErrorCode_NotImplemented);
    }

    if ((bitsAllocated != 8 && bitsAllocated != 16 && 
         bitsAllocated != 24 && bitsAllocated != 32) ||
        numberOfFrames_ == 0)
    {
      throw PalanthirException(ErrorCode_NotImplemented);
    }

    if (bitsAllocated > 32 ||
        bitsStored >= 32)
    {
      // Not available, as the accessor internally uses int32_t values
      throw PalanthirException(ErrorCode_NotImplemented);
    }

    if (samplesPerPixel_ != 1)
    {
      throw PalanthirException(ErrorCode_NotImplemented);
    }

    if (width_ * height_ * bitsAllocated / 8 * numberOfFrames_ != size)
    {
      throw PalanthirException(ErrorCode_NotImplemented);
    }

    /*printf("%d %d %d %d %d %d %d %d\n", width_, height_, samplesPerPixel_, bitsAllocated,
           bitsStored, highBit, pixelRepresentation, numberOfFrames_);*/

    bytesPerPixel_ = bitsAllocated / 8;
    shift_ = highBit + 1 - bitsStored;

    if (pixelRepresentation)
    {
      mask_ = (1 << (bitsStored - 1)) - 1;
      signMask_ = (1 << (bitsStored - 1));
    }
    else
    {
      mask_ = (1 << bitsStored) - 1;
      signMask_ = 0;
    }

    rowOffset_ = width_ * bytesPerPixel_;
    frameOffset_ = height_ * width_ * bytesPerPixel_;
  }


  void DicomIntegerPixelAccessor::GetExtremeValues(int32_t& min, 
                                                   int32_t& max) const
  {
    if (height_ == 0 || width_ == 0)
    {
      min = max = 0;
      return;
    }

    min = std::numeric_limits<int32_t>::max();
    max = std::numeric_limits<int32_t>::min();
    
    for (unsigned int y = 0; y < height_; y++)
    {
      for (unsigned int x = 0; x < width_; x++)
      {
        int32_t v = GetValue(x, y);
        if (v < min)
          min = v;
        if (v > max)
          max = v;
      }
    }
  }


  int32_t DicomIntegerPixelAccessor::GetValue(unsigned int x, unsigned int y) const
  {
    assert(x < width_ && y < height_);
    
    const uint8_t* pixel = reinterpret_cast<const uint8_t*>(pixelData_) + 
      y * rowOffset_ + x * bytesPerPixel_ + frame_ * frameOffset_;

    int32_t v;
    v = pixel[0];
    if (bytesPerPixel_ >= 2)
      v = v + (static_cast<int32_t>(pixel[1]) << 8);
    if (bytesPerPixel_ >= 3)
      v = v + (static_cast<int32_t>(pixel[2]) << 16);
    if (bytesPerPixel_ >= 4)
      v = v + (static_cast<int32_t>(pixel[3]) << 24);

    v = (v >> shift_) & mask_;

    if (v & signMask_)
    {
      // Signed value: Not implemented yet
      //throw PalanthirException(ErrorCode_NotImplemented);
      v = 0;
    }

    return v;
  }


  void DicomIntegerPixelAccessor::SetCurrentFrame(unsigned int frame)
  {
    if (frame >= numberOfFrames_)
    {
      throw PalanthirException(ErrorCode_ParameterOutOfRange);
    }

    frame_ = frame;
  }

}
