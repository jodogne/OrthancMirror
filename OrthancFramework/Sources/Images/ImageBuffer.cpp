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
#include "ImageBuffer.h"

#include "../OrthancException.h"

#include <boost/lexical_cast.hpp>
#include <stdio.h>
#include <stdlib.h>

namespace Orthanc
{
  void ImageBuffer::Allocate()
  {
    if (changed_)
    {
      Deallocate();

      /*
        if (forceMinimalPitch_)
        {
        TODO: Align pitch and memory buffer to optimal size for SIMD.
        }
      */

      pitch_ = GetBytesPerPixel() * width_;
      size_t size = pitch_ * height_;

      if (size == 0)
      {
        buffer_ = NULL;
      }
      else
      {
        buffer_ = malloc(size);
        if (buffer_ == NULL)
        {
          throw OrthancException(ErrorCode_NotEnoughMemory,
                                 "Failed to allocate an image buffer of size " + boost::lexical_cast<std::string>(width_) + "x" + boost::lexical_cast<std::string>(height_));
        }
      }

      changed_ = false;
    }
  }


  void ImageBuffer::Deallocate()
  {
    if (buffer_ != NULL)
    {
      free(buffer_);
      buffer_ = NULL;
      changed_ = true;
    }
  }


  ImageBuffer::ImageBuffer(PixelFormat format,
                           unsigned int width,
                           unsigned int height,
                           bool forceMinimalPitch) :
    forceMinimalPitch_(forceMinimalPitch)
  {
    Initialize();
    SetWidth(width);
    SetHeight(height);
    SetFormat(format);
  }

  ImageBuffer::ImageBuffer()
  {
    Initialize();
  }

  ImageBuffer::~ImageBuffer()
  {
    Deallocate();
  }

  PixelFormat ImageBuffer::GetFormat() const
  {
    return format_;
  }


  void ImageBuffer::Initialize()
  {
    changed_ = false;
    forceMinimalPitch_ = true;
    format_ = PixelFormat_Grayscale8;
    width_ = 0;
    height_ = 0;
    pitch_ = 0;
    buffer_ = NULL;
  }


  void ImageBuffer::SetFormat(PixelFormat format)
  {
    if (format != format_)
    {
      changed_ = true;
      format_ = format;
    }
  }

  unsigned int ImageBuffer::GetWidth() const
  {
    return width_;
  }


  void ImageBuffer::SetWidth(unsigned int width)
  {
    if (width != width_)
    {
      changed_ = true;
      width_ = width;     
    }
  }

  unsigned int ImageBuffer::GetHeight() const
  {
    return height_;
  }


  void ImageBuffer::SetHeight(unsigned int height)
  {
    if (height != height_)
    {
      changed_ = true;
      height_ = height;     
    }
  }

  unsigned int ImageBuffer::GetBytesPerPixel() const
  {
    return ::Orthanc::GetBytesPerPixel(format_);
  }

  
  void ImageBuffer::GetReadOnlyAccessor(ImageAccessor& accessor)
  {
    Allocate();
    accessor.AssignReadOnly(format_, width_, height_, pitch_, buffer_);
  }
  

  void ImageBuffer::GetWriteableAccessor(ImageAccessor& accessor)
  {
    Allocate();
    accessor.AssignWritable(format_, width_, height_, pitch_, buffer_);
  }

  bool ImageBuffer::IsMinimalPitchForced() const
  {
    return forceMinimalPitch_;
  }


  void ImageBuffer::AcquireOwnership(ImageBuffer& other)
  {
    // Remove the content of the current image
    Deallocate();

    // Force the allocation of the other image (if not already
    // allocated)
    other.Allocate();

    // Transfer the content of the other image
    changed_ = false;
    forceMinimalPitch_ = other.forceMinimalPitch_;
    format_ = other.format_;
    width_ = other.width_;
    height_ = other.height_;
    pitch_ = other.pitch_;
    buffer_ = other.buffer_;

    // Force the reinitialization of the other image
    other.Initialize();
  }
}
