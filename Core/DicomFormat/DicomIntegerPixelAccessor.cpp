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
  static const DicomTag COLUMNS(0x0028, 0x0011);
  static const DicomTag ROWS(0x0028, 0x0010);
  static const DicomTag SAMPLES_PER_PIXEL(0x0028, 0x0002);
  static const DicomTag BITS_ALLOCATED(0x0028, 0x0100);
  static const DicomTag BITS_STORED(0x0028, 0x0101);
  static const DicomTag HIGH_BIT(0x0028, 0x0102);
  static const DicomTag PIXEL_REPRESENTATION(0x0028, 0x0103);
  static const DicomTag PLANAR_CONFIGURATION(0x0028, 0x0006);

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
    planarConfiguration_ = 0;

    try
    {
      width_ = boost::lexical_cast<unsigned int>(values.GetValue(COLUMNS).AsString());
      height_ = boost::lexical_cast<unsigned int>(values.GetValue(ROWS).AsString());
      samplesPerPixel_ = boost::lexical_cast<unsigned int>(values.GetValue(SAMPLES_PER_PIXEL).AsString());
      bitsAllocated = boost::lexical_cast<unsigned int>(values.GetValue(BITS_ALLOCATED).AsString());
      bitsStored = boost::lexical_cast<unsigned int>(values.GetValue(BITS_STORED).AsString());
      highBit = boost::lexical_cast<unsigned int>(values.GetValue(HIGH_BIT).AsString());
      pixelRepresentation = boost::lexical_cast<unsigned int>(values.GetValue(PIXEL_REPRESENTATION).AsString());

      if (samplesPerPixel_ > 1)
      {
        // The "Planar Configuration" is only set when "Samples per Pixels" is greater than 1
        // https://www.dabsoft.ch/dicom/3/C.7.6.3.1.3/
        planarConfiguration_ = boost::lexical_cast<unsigned int>(values.GetValue(PLANAR_CONFIGURATION).AsString());
      }
    }
    catch (boost::bad_lexical_cast)
    {
      throw OrthancException(ErrorCode_NotImplemented);
    }
    catch (OrthancException)
    {
      throw OrthancException(ErrorCode_NotImplemented);
    }

    frame_ = 0;
    try
    {
      numberOfFrames_ = boost::lexical_cast<unsigned int>(values.GetValue(DICOM_TAG_NUMBER_OF_FRAMES).AsString());
    }
    catch (OrthancException)
    {
      // If the tag "NumberOfFrames" is absent, assume there is a single frame
      numberOfFrames_ = 1;
    }
    catch (boost::bad_lexical_cast)
    {
      throw OrthancException(ErrorCode_NotImplemented);
    }

    if ((bitsAllocated != 8 && bitsAllocated != 16 && 
         bitsAllocated != 24 && bitsAllocated != 32) ||
        numberOfFrames_ == 0 ||
        (planarConfiguration_ != 0 && planarConfiguration_ != 1))
    {
      throw OrthancException(ErrorCode_NotImplemented);
    }

    if (bitsAllocated > 32 ||
        bitsStored >= 32)
    {
      // Not available, as the accessor internally uses int32_t values
      throw OrthancException(ErrorCode_NotImplemented);
    }

    if (samplesPerPixel_ == 0)
    {
      throw OrthancException(ErrorCode_NotImplemented);
    }

    bytesPerPixel_ = bitsAllocated / 8;
    shift_ = highBit + 1 - bitsStored;
    frameOffset_ = height_ * width_ * bytesPerPixel_ * samplesPerPixel_;

    if (numberOfFrames_ * frameOffset_ > size)
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    /*printf("%d %d %d %d %d %d %d %d\n", width_, height_, samplesPerPixel_, bitsAllocated,
      bitsStored, highBit, pixelRepresentation, numberOfFrames_);*/

    if (pixelRepresentation)
    {
      // Pixels are signed
      mask_ = (1 << (bitsStored - 1)) - 1;
      signMask_ = (1 << (bitsStored - 1));
    }
    else
    {
      // Pixels are unsigned
      mask_ = (1 << bitsStored) - 1;
      signMask_ = 0;
    }

    if (planarConfiguration_ == 0)
    {
      /**
       * The sample values for the first pixel are followed by the
       * sample values for the second pixel, etc. For RGB images, this
       * means the order of the pixel values sent shall be R1, G1, B1,
       * R2, G2, B2, ..., etc.
       **/
      rowOffset_ = width_ * bytesPerPixel_ * samplesPerPixel_;
    }
    else
    {
      /**
       * Each color plane shall be sent contiguously. For RGB images,
       * this means the order of the pixel values sent is R1, R2, R3,
       * ..., G1, G2, G3, ..., B1, B2, B3, etc.
       **/
      rowOffset_ = width_ * bytesPerPixel_;
    }
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
        for (unsigned int c = 0; c < GetChannelCount(); c++)
        {
          int32_t v = GetValue(x, y);
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
    assert(x < width_ && y < height_ && channel < samplesPerPixel_);
    
    const uint8_t* pixel = reinterpret_cast<const uint8_t*>(pixelData_) + 
      y * rowOffset_ + frame_ * frameOffset_;

    // https://www.dabsoft.ch/dicom/3/C.7.6.3.1.3/
    if (planarConfiguration_ == 0)
    {
      /**
       * The sample values for the first pixel are followed by the
       * sample values for the second pixel, etc. For RGB images, this
       * means the order of the pixel values sent shall be R1, G1, B1,
       * R2, G2, B2, ..., etc.
       **/
      pixel += channel * bytesPerPixel_ + x * samplesPerPixel_ * bytesPerPixel_;
    }
    else
    {
      /**
       * Each color plane shall be sent contiguously. For RGB images,
       * this means the order of the pixel values sent is R1, R2, R3,
       * ..., G1, G2, G3, ..., B1, B2, B3, etc.
       **/
      assert(frameOffset_ % samplesPerPixel_ == 0);
      pixel += channel * frameOffset_ / samplesPerPixel_ + x * bytesPerPixel_;
    }

    uint32_t v;
    v = pixel[0];
    if (bytesPerPixel_ >= 2)
      v = v + (static_cast<uint32_t>(pixel[1]) << 8);
    if (bytesPerPixel_ >= 3)
      v = v + (static_cast<uint32_t>(pixel[2]) << 16);
    if (bytesPerPixel_ >= 4)
      v = v + (static_cast<uint32_t>(pixel[3]) << 24);

    v = v >> shift_;

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


  void DicomIntegerPixelAccessor::SetCurrentFrame(unsigned int frame)
  {
    if (frame >= numberOfFrames_)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    frame_ = frame;
  }

}
