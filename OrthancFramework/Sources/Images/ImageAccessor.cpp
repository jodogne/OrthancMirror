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
#include "ImageAccessor.h"

#include "../Logging.h"
#include "../OrthancException.h"
#include "../ChunkedBuffer.h"

#include <stdint.h>
#include <cassert>
#include <boost/lexical_cast.hpp>



namespace Orthanc
{
  template <typename PixelType>
  static void ToMatlabStringInternal(ChunkedBuffer& target,
                                     const ImageAccessor& source)
  {
    target.AddChunk("double([ ");

    const unsigned int width = source.GetWidth();
    const unsigned int height = source.GetHeight();

    for (unsigned int y = 0; y < height; y++)
    {
      const PixelType* p = reinterpret_cast<const PixelType*>(source.GetConstRow(y));

      std::string s;
      if (y > 0)
      {
        s = "; ";
      }

      s.reserve(width * 8);

      for (unsigned int x = 0; x < width; x++, p++)
      {
        s += boost::lexical_cast<std::string>(static_cast<double>(*p)) + " ";
      }

      target.AddChunk(s);
    }

    target.AddChunk("])");
  }


  static void RGB24ToMatlabString(ChunkedBuffer& target,
                                  const ImageAccessor& source)
  {
    assert(source.GetFormat() == PixelFormat_RGB24);

    target.AddChunk("double(permute(reshape([ ");

    const unsigned int width = source.GetWidth();
    const unsigned int height = source.GetHeight();

    for (unsigned int y = 0; y < height; y++)
    {
      const uint8_t* p = reinterpret_cast<const uint8_t*>(source.GetConstRow(y));
      
      std::string s;
      s.reserve(width * 3 * 8);
      
      for (unsigned int x = 0; x < 3 * width; x++, p++)
      {
        s += boost::lexical_cast<std::string>(static_cast<int>(*p)) + " ";
      }
      
      target.AddChunk(s);
    }

    target.AddChunk("], [ 3 " + boost::lexical_cast<std::string>(height) +
                    " " + boost::lexical_cast<std::string>(width) + " ]), [ 3 2 1 ]))");
  }
  

  ImageAccessor::ImageAccessor()
  {
    AssignEmpty(PixelFormat_Grayscale8);
  }

  ImageAccessor::~ImageAccessor()
  {
  }

  bool ImageAccessor::IsReadOnly() const
  {
    return readOnly_;
  }

  PixelFormat ImageAccessor::GetFormat() const
  {
    return format_;
  }

  unsigned int ImageAccessor::GetBytesPerPixel() const
  {
    return ::Orthanc::GetBytesPerPixel(format_);
  }

  unsigned int ImageAccessor::GetWidth() const
  {
    return width_;
  }

  unsigned int ImageAccessor::GetHeight() const
  {
    return height_;
  }

  unsigned int ImageAccessor::GetPitch() const
  {
    return pitch_;
  }

  unsigned int ImageAccessor::GetSize() const
  {
    return GetHeight() * GetPitch();
  }

  const void *ImageAccessor::GetConstBuffer() const
  {
    return buffer_;
  }

  void* ImageAccessor::GetBuffer()
  {
    if (readOnly_)
    {
      throw OrthancException(ErrorCode_ReadOnly,
                             "Trying to write to a read-only image");
    }

    return buffer_;
  }


  const void* ImageAccessor::GetConstRow(unsigned int y) const
  {
    if (buffer_ != NULL)
    {
      return buffer_ + y * pitch_;
    }
    else
    {
      return NULL;
    }
  }


  void* ImageAccessor::GetRow(unsigned int y)
  {
    if (readOnly_)
    {
      throw OrthancException(ErrorCode_ReadOnly,
                             "Trying to write to a read-only image");
    }

    if (buffer_ != NULL)
    {
      return buffer_ + y * pitch_;
    }
    else
    {
      return NULL;
    }
  }


  void ImageAccessor::AssignEmpty(PixelFormat format)
  {
    readOnly_ = false;
    format_ = format;
    width_ = 0;
    height_ = 0;
    pitch_ = 0;
    buffer_ = NULL;
  }


  void ImageAccessor::AssignReadOnly(PixelFormat format,
                                     unsigned int width,
                                     unsigned int height,
                                     unsigned int pitch,
                                     const void *buffer)
  {
    readOnly_ = true;
    format_ = format;
    width_ = width;
    height_ = height;
    pitch_ = pitch;
    buffer_ = reinterpret_cast<uint8_t*>(const_cast<void*>(buffer));

    if (GetBytesPerPixel() * width_ > pitch_)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }

  void ImageAccessor::GetReadOnlyAccessor(ImageAccessor &target) const
  {
    target.AssignReadOnly(format_, width_, height_, pitch_, buffer_);
  }


  void ImageAccessor::AssignWritable(PixelFormat format,
                                     unsigned int width,
                                     unsigned int height,
                                     unsigned int pitch,
                                     void *buffer)
  {
    readOnly_ = false;
    format_ = format;
    width_ = width;
    height_ = height;
    pitch_ = pitch;
    buffer_ = reinterpret_cast<uint8_t*>(buffer);

    if (GetBytesPerPixel() * width_ > pitch_)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }


  void ImageAccessor::GetWriteableAccessor(ImageAccessor& target) const
  {
    if (readOnly_)
    {
      throw OrthancException(ErrorCode_ReadOnly);
    }
    else
    {
      target.AssignWritable(format_, width_, height_, pitch_, buffer_);
    }
  }


  void ImageAccessor::ToMatlabString(std::string& target) const
  {
    ChunkedBuffer buffer;

    switch (GetFormat())
    {
      case PixelFormat_Grayscale8:
        ToMatlabStringInternal<uint8_t>(buffer, *this);
        break;

      case PixelFormat_Grayscale16:
        ToMatlabStringInternal<uint16_t>(buffer, *this);
        break;

      case PixelFormat_Grayscale32:
        ToMatlabStringInternal<uint32_t>(buffer, *this);
        break;

      case PixelFormat_Grayscale64:
        ToMatlabStringInternal<uint64_t>(buffer, *this);
        break;

      case PixelFormat_SignedGrayscale16:
        ToMatlabStringInternal<int16_t>(buffer, *this);
        break;

      case PixelFormat_Float32:
        ToMatlabStringInternal<float>(buffer, *this);
        break;

      case PixelFormat_RGB24:
        RGB24ToMatlabString(buffer, *this);
        break;

      default:
        throw OrthancException(ErrorCode_NotImplemented);
    }   

    buffer.Flatten(target);
  }



  void ImageAccessor::GetRegion(ImageAccessor& accessor,
                                unsigned int x,
                                unsigned int y,
                                unsigned int width,
                                unsigned int height) const
  {
    if (x + width > width_ ||
        y + height > height_)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
    
    if (width == 0 ||
        height == 0)
    {
      accessor.AssignWritable(format_, 0, 0, 0, NULL);
    }
    else
    {
      uint8_t* p = (buffer_ + 
                    y * pitch_ + 
                    x * GetBytesPerPixel());

      if (readOnly_)
      {
        accessor.AssignReadOnly(format_, width, height, pitch_, p);
      }
      else
      {
        accessor.AssignWritable(format_, width, height, pitch_, p);
      }
    }
  }


  void ImageAccessor::SetFormat(PixelFormat format)
  {
    if (readOnly_)
    {
      throw OrthancException(ErrorCode_ReadOnly,
                             "Trying to modify the format of a read-only image");
    }

    if (::Orthanc::GetBytesPerPixel(format) != ::Orthanc::GetBytesPerPixel(format_))
    {
      throw OrthancException(ErrorCode_IncompatibleImageFormat);
    }

    format_ = format;
  }


#if ORTHANC_BUILDING_FRAMEWORK_LIBRARY == 1
  void* ImageAccessor::GetBuffer() const
  {
    return const_cast<ImageAccessor&>(*this).GetBuffer();
  }

  void* ImageAccessor::GetRow(unsigned int y) const
  {
    return const_cast<ImageAccessor&>(*this).GetRow(y);
  }
#endif
}
