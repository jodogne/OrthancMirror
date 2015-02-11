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


#include "../PrecompiledHeaders.h"
#include "ImageProcessing.h"

#include "../OrthancException.h"

#include <boost/math/special_functions/round.hpp>

#include <cassert>
#include <string.h>
#include <limits>
#include <stdint.h>

namespace Orthanc
{
  template <typename TargetType, typename SourceType>
  static void ConvertInternal(ImageAccessor& target,
                              const ImageAccessor& source)
  {
    const TargetType minValue = std::numeric_limits<TargetType>::min();
    const TargetType maxValue = std::numeric_limits<TargetType>::max();

    for (unsigned int y = 0; y < source.GetHeight(); y++)
    {
      TargetType* t = reinterpret_cast<TargetType*>(target.GetRow(y));
      const SourceType* s = reinterpret_cast<const SourceType*>(source.GetConstRow(y));

      for (unsigned int x = 0; x < source.GetWidth(); x++, t++, s++)
      {
        if (static_cast<int32_t>(*s) < static_cast<int32_t>(minValue))
        {
          *t = minValue;
        }
        else if (static_cast<int32_t>(*s) > static_cast<int32_t>(maxValue))
        {
          *t = maxValue;
        }
        else
        {
          *t = static_cast<TargetType>(*s);
        }
      }
    }
  }


  template <typename TargetType>
  static void ConvertColorToGrayscale(ImageAccessor& target,
                              const ImageAccessor& source)
  {
    assert(source.GetFormat() == PixelFormat_RGB24);

    const TargetType minValue = std::numeric_limits<TargetType>::min();
    const TargetType maxValue = std::numeric_limits<TargetType>::max();

    for (unsigned int y = 0; y < source.GetHeight(); y++)
    {
      TargetType* t = reinterpret_cast<TargetType*>(target.GetRow(y));
      const uint8_t* s = reinterpret_cast<const uint8_t*>(source.GetConstRow(y));

      for (unsigned int x = 0; x < source.GetWidth(); x++, t++, s += 3)
      {
        // Y = 0.2126 R + 0.7152 G + 0.0722 B
        int32_t v = (2126 * static_cast<int32_t>(s[0]) +
                     7152 * static_cast<int32_t>(s[1]) +
                     0722 * static_cast<int32_t>(s[2])) / 1000;
        
        if (static_cast<int32_t>(v) < static_cast<int32_t>(minValue))
        {
          *t = minValue;
        }
        else if (static_cast<int32_t>(v) > static_cast<int32_t>(maxValue))
        {
          *t = maxValue;
        }
        else
        {
          *t = static_cast<TargetType>(v);
        }
      }
    }
  }


  template <typename PixelType>
  static void SetInternal(ImageAccessor& image,
                          int64_t constant)
  {
    for (unsigned int y = 0; y < image.GetHeight(); y++)
    {
      PixelType* p = reinterpret_cast<PixelType*>(image.GetRow(y));

      for (unsigned int x = 0; x < image.GetWidth(); x++, p++)
      {
        *p = static_cast<PixelType>(constant);
      }
    }
  }


  template <typename PixelType>
  static void GetMinMaxValueInternal(PixelType& minValue,
                                     PixelType& maxValue,
                                     const ImageAccessor& source)
  {
    // Deal with the special case of empty image
    if (source.GetWidth() == 0 ||
        source.GetHeight() == 0)
    {
      minValue = 0;
      maxValue = 0;
      return;
    }

    minValue = std::numeric_limits<PixelType>::max();
    maxValue = std::numeric_limits<PixelType>::min();

    for (unsigned int y = 0; y < source.GetHeight(); y++)
    {
      const PixelType* p = reinterpret_cast<const PixelType*>(source.GetConstRow(y));

      for (unsigned int x = 0; x < source.GetWidth(); x++, p++)
      {
        if (*p < minValue)
        {
          minValue = *p;
        }

        if (*p > maxValue)
        {
          maxValue = *p;
        }
      }
    }
  }



  template <typename PixelType>
  static void AddConstantInternal(ImageAccessor& image,
                                  int64_t constant)
  {
    if (constant == 0)
    {
      return;
    }

    const int64_t minValue = std::numeric_limits<PixelType>::min();
    const int64_t maxValue = std::numeric_limits<PixelType>::max();

    for (unsigned int y = 0; y < image.GetHeight(); y++)
    {
      PixelType* p = reinterpret_cast<PixelType*>(image.GetRow(y));

      for (unsigned int x = 0; x < image.GetWidth(); x++, p++)
      {
        int64_t v = static_cast<int64_t>(*p) + constant;

        if (v > maxValue)
        {
          *p = std::numeric_limits<PixelType>::max();
        }
        else if (v < minValue)
        {
          *p = std::numeric_limits<PixelType>::min();
        }
        else
        {
          *p = static_cast<PixelType>(v);
        }
      }
    }
  }



  template <typename PixelType>
  void MultiplyConstantInternal(ImageAccessor& image,
                                float factor)
  {
    if (abs(factor - 1.0f) <= std::numeric_limits<float>::epsilon())
    {
      return;
    }

    const int64_t minValue = std::numeric_limits<PixelType>::min();
    const int64_t maxValue = std::numeric_limits<PixelType>::max();

    for (unsigned int y = 0; y < image.GetHeight(); y++)
    {
      PixelType* p = reinterpret_cast<PixelType*>(image.GetRow(y));

      for (unsigned int x = 0; x < image.GetWidth(); x++, p++)
      {
        int64_t v = boost::math::llround(static_cast<float>(*p) * factor);

        if (v > maxValue)
        {
          *p = std::numeric_limits<PixelType>::max();
        }
        else if (v < minValue)
        {
          *p = std::numeric_limits<PixelType>::min();
        }
        else
        {
          *p = static_cast<PixelType>(v);
        }
      }
    }
  }


  template <typename PixelType>
  void ShiftScaleInternal(ImageAccessor& image,
                          float offset,
                          float scaling)
  {
    const float minValue = static_cast<float>(std::numeric_limits<PixelType>::min());
    const float maxValue = static_cast<float>(std::numeric_limits<PixelType>::max());

    for (unsigned int y = 0; y < image.GetHeight(); y++)
    {
      PixelType* p = reinterpret_cast<PixelType*>(image.GetRow(y));

      for (unsigned int x = 0; x < image.GetWidth(); x++, p++)
      {
        float v = (static_cast<float>(*p) + offset) * scaling;

        if (v > maxValue)
        {
          *p = std::numeric_limits<PixelType>::max();
        }
        else if (v < minValue)
        {
          *p = std::numeric_limits<PixelType>::min();
        }
        else
        {
          *p = static_cast<PixelType>(boost::math::iround(v));
        }
      }
    }
  }


  void ImageProcessing::Copy(ImageAccessor& target,
                             const ImageAccessor& source)
  {
    if (target.GetWidth() != source.GetWidth() ||
        target.GetHeight() != source.GetHeight())
    {
      throw OrthancException(ErrorCode_IncompatibleImageSize);
    }

    if (target.GetFormat() != source.GetFormat())
    {
      throw OrthancException(ErrorCode_IncompatibleImageFormat);
    }

    unsigned int lineSize = GetBytesPerPixel(source.GetFormat()) * source.GetWidth();

    assert(source.GetPitch() >= lineSize && target.GetPitch() >= lineSize);

    for (unsigned int y = 0; y < source.GetHeight(); y++)
    {
      memcpy(target.GetRow(y), source.GetConstRow(y), lineSize);
    }
  }


  void ImageProcessing::Convert(ImageAccessor& target,
                                const ImageAccessor& source)
  {
    if (target.GetWidth() != source.GetWidth() ||
        target.GetHeight() != source.GetHeight())
    {
      throw OrthancException(ErrorCode_IncompatibleImageSize);
    }

    if (source.GetFormat() == target.GetFormat())
    {
      Copy(target, source);
      return;
    }

    if (target.GetFormat() == PixelFormat_Grayscale16 &&
        source.GetFormat() == PixelFormat_Grayscale8)
    {
      ConvertInternal<uint16_t, uint8_t>(target, source);
      return;
    }

    if (target.GetFormat() == PixelFormat_SignedGrayscale16 &&
        source.GetFormat() == PixelFormat_Grayscale8)
    {
      ConvertInternal<int16_t, uint8_t>(target, source);
      return;
    }

    if (target.GetFormat() == PixelFormat_Grayscale8 &&
        source.GetFormat() == PixelFormat_Grayscale16)
    {
      ConvertInternal<uint8_t, uint16_t>(target, source);
      return;
    }

    if (target.GetFormat() == PixelFormat_SignedGrayscale16 &&
        source.GetFormat() == PixelFormat_Grayscale16)
    {
      ConvertInternal<int16_t, uint16_t>(target, source);
      return;
    }

    if (target.GetFormat() == PixelFormat_Grayscale8 &&
        source.GetFormat() == PixelFormat_SignedGrayscale16)
    {
      ConvertInternal<uint8_t, int16_t>(target, source);
      return;
    }

    if (target.GetFormat() == PixelFormat_Grayscale16 &&
        source.GetFormat() == PixelFormat_SignedGrayscale16)
    {
      ConvertInternal<uint16_t, int16_t>(target, source);
      return;
    }

    if (target.GetFormat() == PixelFormat_Grayscale8 &&
        source.GetFormat() == PixelFormat_RGB24)
    {
      ConvertColorToGrayscale<uint8_t>(target, source);
      return;
    }

    if (target.GetFormat() == PixelFormat_Grayscale16 &&
        source.GetFormat() == PixelFormat_RGB24)
    {
      ConvertColorToGrayscale<uint16_t>(target, source);
      return;
    }

    if (target.GetFormat() == PixelFormat_SignedGrayscale16 &&
        source.GetFormat() == PixelFormat_RGB24)
    {
      ConvertColorToGrayscale<int16_t>(target, source);
      return;
    }

    throw OrthancException(ErrorCode_NotImplemented);
  }



  void ImageProcessing::Set(ImageAccessor& image,
                            int64_t value)
  {
    switch (image.GetFormat())
    {
      case PixelFormat_Grayscale8:
        SetInternal<uint8_t>(image, value);
        return;

      case PixelFormat_Grayscale16:
        SetInternal<uint16_t>(image, value);
        return;

      case PixelFormat_SignedGrayscale16:
        SetInternal<int16_t>(image, value);
        return;

      default:
        throw OrthancException(ErrorCode_NotImplemented);
    }
  }


  void ImageProcessing::ShiftRight(ImageAccessor& image,
                                   unsigned int shift)
  {
    if (image.GetWidth() == 0 ||
        image.GetHeight() == 0 ||
        shift == 0)
    {
      // Nothing to do
      return;
    }

    throw OrthancException(ErrorCode_NotImplemented);
  }


  void ImageProcessing::GetMinMaxValue(int64_t& minValue,
                                       int64_t& maxValue,
                                       const ImageAccessor& image)
  {
    switch (image.GetFormat())
    {
      case PixelFormat_Grayscale8:
      {
        uint8_t a, b;
        GetMinMaxValueInternal<uint8_t>(a, b, image);
        minValue = a;
        maxValue = b;
        break;
      }

      case PixelFormat_Grayscale16:
      {
        uint16_t a, b;
        GetMinMaxValueInternal<uint16_t>(a, b, image);
        minValue = a;
        maxValue = b;
        break;
      }

      case PixelFormat_SignedGrayscale16:
      {
        int16_t a, b;
        GetMinMaxValueInternal<int16_t>(a, b, image);
        minValue = a;
        maxValue = b;
        break;
      }

      default:
        throw OrthancException(ErrorCode_NotImplemented);
    }
  }



  void ImageProcessing::AddConstant(ImageAccessor& image,
                                    int64_t value)
  {
    switch (image.GetFormat())
    {
      case PixelFormat_Grayscale8:
        AddConstantInternal<uint8_t>(image, value);
        return;

      case PixelFormat_Grayscale16:
        AddConstantInternal<uint16_t>(image, value);
        return;

      case PixelFormat_SignedGrayscale16:
        AddConstantInternal<int16_t>(image, value);
        return;

      default:
        throw OrthancException(ErrorCode_NotImplemented);
    }
  }


  void ImageProcessing::MultiplyConstant(ImageAccessor& image,
                                         float factor)
  {
    switch (image.GetFormat())
    {
      case PixelFormat_Grayscale8:
        MultiplyConstantInternal<uint8_t>(image, factor);
        return;

      case PixelFormat_Grayscale16:
        MultiplyConstantInternal<uint16_t>(image, factor);
        return;

      case PixelFormat_SignedGrayscale16:
        MultiplyConstantInternal<int16_t>(image, factor);
        return;

      default:
        throw OrthancException(ErrorCode_NotImplemented);
    }
  }


  void ImageProcessing::ShiftScale(ImageAccessor& image,
                                   float offset,
                                   float scaling)
  {
    switch (image.GetFormat())
    {
      case PixelFormat_Grayscale8:
        ShiftScaleInternal<uint8_t>(image, offset, scaling);
        return;

      case PixelFormat_Grayscale16:
        ShiftScaleInternal<uint16_t>(image, offset, scaling);
        return;

      case PixelFormat_SignedGrayscale16:
        ShiftScaleInternal<int16_t>(image, offset, scaling);
        return;

      default:
        throw OrthancException(ErrorCode_NotImplemented);
    }
  }
}
