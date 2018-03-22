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
#include "ImageProcessing.h"

#include "PixelTraits.h"

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


  template <typename SourceType>
  static void ConvertGrayscaleToFloat(ImageAccessor& target,
                                      const ImageAccessor& source)
  {
    assert(sizeof(float) == 4);

    for (unsigned int y = 0; y < source.GetHeight(); y++)
    {
      float* t = reinterpret_cast<float*>(target.GetRow(y));
      const SourceType* s = reinterpret_cast<const SourceType*>(source.GetConstRow(y));

      for (unsigned int x = 0; x < source.GetWidth(); x++, t++, s++)
      {
        *t = static_cast<float>(*s);
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
                     0722 * static_cast<int32_t>(s[2])) / 10000;
        
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

    const unsigned int width = source.GetWidth();

    for (unsigned int y = 0; y < source.GetHeight(); y++)
    {
      const PixelType* p = reinterpret_cast<const PixelType*>(source.GetConstRow(y));

      for (unsigned int x = 0; x < width; x++, p++)
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



  template <typename PixelType,
            bool UseRound>
  static void MultiplyConstantInternal(ImageAccessor& image,
                                       float factor)
  {
    if (std::abs(factor - 1.0f) <= std::numeric_limits<float>::epsilon())
    {
      return;
    }

    const int64_t minValue = std::numeric_limits<PixelType>::min();
    const int64_t maxValue = std::numeric_limits<PixelType>::max();
    const unsigned int width = image.GetWidth();

    for (unsigned int y = 0; y < image.GetHeight(); y++)
    {
      PixelType* p = reinterpret_cast<PixelType*>(image.GetRow(y));

      for (unsigned int x = 0; x < width; x++, p++)
      {
        int64_t v;
        if (UseRound)
        {
          // The "round" operation is very costly
          v = boost::math::llround(static_cast<float>(*p) * factor);
        }
        else
        {
          v = static_cast<int64_t>(static_cast<float>(*p) * factor);
        }

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


  template <typename PixelType,
            bool UseRound>
  static void ShiftScaleInternal(ImageAccessor& image,
                                 float offset,
                                 float scaling)
  {
    const float minFloatValue = static_cast<float>(std::numeric_limits<PixelType>::min());
    const float maxFloatValue = static_cast<float>(std::numeric_limits<PixelType>::max());
    const PixelType minPixelValue = std::numeric_limits<PixelType>::min();
    const PixelType maxPixelValue = std::numeric_limits<PixelType>::max();

    const unsigned int height = image.GetHeight();
    const unsigned int width = image.GetWidth();
    
    for (unsigned int y = 0; y < height; y++)
    {
      PixelType* p = reinterpret_cast<PixelType*>(image.GetRow(y));

      for (unsigned int x = 0; x < width; x++, p++)
      {
        float v = (static_cast<float>(*p) + offset) * scaling;

        if (v > maxFloatValue)
        {
          *p = maxPixelValue;
        }
        else if (v < minFloatValue)
        {
          *p = minPixelValue;
        }
        else if (UseRound)
        {
          // The "round" operation is very costly
          *p = static_cast<PixelType>(boost::math::iround(v));
        }
        else
        {
          *p = static_cast<PixelType>(v);
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

    if (target.GetFormat() == PixelFormat_Float32 &&
        source.GetFormat() == PixelFormat_Grayscale8)
    {
      ConvertGrayscaleToFloat<uint8_t>(target, source);
      return;
    }

    if (target.GetFormat() == PixelFormat_Float32 &&
        source.GetFormat() == PixelFormat_Grayscale16)
    {
      ConvertGrayscaleToFloat<uint16_t>(target, source);
      return;
    }

    if (target.GetFormat() == PixelFormat_Float32 &&
        source.GetFormat() == PixelFormat_Grayscale32)
    {
      ConvertGrayscaleToFloat<uint32_t>(target, source);
      return;
    }

    if (target.GetFormat() == PixelFormat_Float32 &&
        source.GetFormat() == PixelFormat_SignedGrayscale16)
    {
      ConvertGrayscaleToFloat<int16_t>(target, source);
      return;
    }

    if (target.GetFormat() == PixelFormat_Grayscale8 &&
        source.GetFormat() == PixelFormat_RGBA32)
    {
      for (unsigned int y = 0; y < source.GetHeight(); y++)
      {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(source.GetConstRow(y));
        uint8_t* q = reinterpret_cast<uint8_t*>(target.GetRow(y));
        for (unsigned int x = 0; x < source.GetWidth(); x++, q++)
        {
          *q = static_cast<uint8_t>((2126 * static_cast<uint32_t>(p[0]) +
                                     7152 * static_cast<uint32_t>(p[1]) +
                                     0722 * static_cast<uint32_t>(p[2])) / 10000);
          p += 4;
        }
      }

      return;
    }

    if (target.GetFormat() == PixelFormat_RGB24 &&
        source.GetFormat() == PixelFormat_RGBA32)
    {
      for (unsigned int y = 0; y < source.GetHeight(); y++)
      {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(source.GetConstRow(y));
        uint8_t* q = reinterpret_cast<uint8_t*>(target.GetRow(y));
        for (unsigned int x = 0; x < source.GetWidth(); x++)
        {
          q[0] = p[0];
          q[1] = p[1];
          q[2] = p[2];
          p += 4;
          q += 3;
        }
      }

      return;
    }

    if (target.GetFormat() == PixelFormat_RGB24 &&
        source.GetFormat() == PixelFormat_BGRA32)
    {
      for (unsigned int y = 0; y < source.GetHeight(); y++)
      {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(source.GetConstRow(y));
        uint8_t* q = reinterpret_cast<uint8_t*>(target.GetRow(y));
        for (unsigned int x = 0; x < source.GetWidth(); x++)
        {
          q[0] = p[2];
          q[1] = p[1];
          q[2] = p[0];
          p += 4;
          q += 3;
        }
      }

      return;
    }

    if (target.GetFormat() == PixelFormat_RGBA32 &&
        source.GetFormat() == PixelFormat_RGB24)
    {
      for (unsigned int y = 0; y < source.GetHeight(); y++)
      {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(source.GetConstRow(y));
        uint8_t* q = reinterpret_cast<uint8_t*>(target.GetRow(y));
        for (unsigned int x = 0; x < source.GetWidth(); x++)
        {
          q[0] = p[0];
          q[1] = p[1];
          q[2] = p[2];
          q[3] = 255;   // Set the alpha channel to full opacity
          p += 3;
          q += 4;
        }
      }

      return;
    }

    if (target.GetFormat() == PixelFormat_RGB24 &&
        source.GetFormat() == PixelFormat_Grayscale8)
    {
      for (unsigned int y = 0; y < source.GetHeight(); y++)
      {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(source.GetConstRow(y));
        uint8_t* q = reinterpret_cast<uint8_t*>(target.GetRow(y));
        for (unsigned int x = 0; x < source.GetWidth(); x++)
        {
          q[0] = *p;
          q[1] = *p;
          q[2] = *p;
          p += 1;
          q += 3;
        }
      }

      return;
    }

    if (target.GetFormat() == PixelFormat_RGBA32 &&
        source.GetFormat() == PixelFormat_Grayscale8)
    {
      for (unsigned int y = 0; y < source.GetHeight(); y++)
      {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(source.GetConstRow(y));
        uint8_t* q = reinterpret_cast<uint8_t*>(target.GetRow(y));
        for (unsigned int x = 0; x < source.GetWidth(); x++)
        {
          q[0] = *p;
          q[1] = *p;
          q[2] = *p;
          q[3] = 255;
          p += 1;
          q += 4;
        }
      }

      return;
    }

    if (target.GetFormat() == PixelFormat_BGRA32 &&
        source.GetFormat() == PixelFormat_Grayscale16)
    {
      for (unsigned int y = 0; y < source.GetHeight(); y++)
      {
        const uint16_t* p = reinterpret_cast<const uint16_t*>(source.GetConstRow(y));
        uint8_t* q = reinterpret_cast<uint8_t*>(target.GetRow(y));
        for (unsigned int x = 0; x < source.GetWidth(); x++)
        {
          uint8_t value = (*p < 256 ? *p : 255);
          q[0] = value;
          q[1] = value;
          q[2] = value;
          q[3] = 255;
          p += 1;
          q += 4;
        }
      }

      return;
    }

    if (target.GetFormat() == PixelFormat_BGRA32 &&
        source.GetFormat() == PixelFormat_SignedGrayscale16)
    {
      for (unsigned int y = 0; y < source.GetHeight(); y++)
      {
        const int16_t* p = reinterpret_cast<const int16_t*>(source.GetConstRow(y));
        uint8_t* q = reinterpret_cast<uint8_t*>(target.GetRow(y));
        for (unsigned int x = 0; x < source.GetWidth(); x++)
        {
          uint8_t value;
          if (*p < 0)
          {
            value = 0;
          }
          else if (*p > 255)
          {
            value = 255;
          }
          else
          {
            value = static_cast<uint8_t>(*p);
          }

          q[0] = value;
          q[1] = value;
          q[2] = value;
          q[3] = 255;
          p += 1;
          q += 4;
        }
      }

      return;
    }

    if (target.GetFormat() == PixelFormat_BGRA32 &&
        source.GetFormat() == PixelFormat_RGB24)
    {
      for (unsigned int y = 0; y < source.GetHeight(); y++)
      {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(source.GetConstRow(y));
        uint8_t* q = reinterpret_cast<uint8_t*>(target.GetRow(y));
        for (unsigned int x = 0; x < source.GetWidth(); x++)
        {
          q[0] = p[2];
          q[1] = p[1];
          q[2] = p[0];
          q[3] = 255;
          p += 3;
          q += 4;
        }
      }

      return;
    }

    if (target.GetFormat() == PixelFormat_RGB24 &&
        source.GetFormat() == PixelFormat_RGB48)
    {
      for (unsigned int y = 0; y < source.GetHeight(); y++)
      {
        const uint16_t* p = reinterpret_cast<const uint16_t*>(source.GetConstRow(y));
        uint8_t* q = reinterpret_cast<uint8_t*>(target.GetRow(y));
        for (unsigned int x = 0; x < source.GetWidth(); x++)
        {
          q[0] = p[0] >> 8;
          q[1] = p[1] >> 8;
          q[2] = p[2] >> 8;
          p += 3;
          q += 3;
        }
      }

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
        memset(image.GetBuffer(), static_cast<uint8_t>(value), image.GetPitch() * image.GetHeight());
        return;

      case PixelFormat_Grayscale16:
        if (value == 0)
        {
          memset(image.GetBuffer(), 0, image.GetPitch() * image.GetHeight());
        }
        else
        {
          SetInternal<uint16_t>(image, value);
        }
        return;

      case PixelFormat_Grayscale32:
        if (value == 0)
        {
          memset(image.GetBuffer(), 0, image.GetPitch() * image.GetHeight());
        }
        else
        {
          SetInternal<uint32_t>(image, value);
        }
        return;

      case PixelFormat_SignedGrayscale16:
        if (value == 0)
        {
          memset(image.GetBuffer(), 0, image.GetPitch() * image.GetHeight());
        }
        else
        {
          SetInternal<int16_t>(image, value);
        }
        return;

      case PixelFormat_Float32:
        assert(sizeof(float) == 4);
        SetInternal<float>(image, value);
        return;

      default:
        throw OrthancException(ErrorCode_NotImplemented);
    }
  }


  void ImageProcessing::Set(ImageAccessor& image,
                            uint8_t red,
                            uint8_t green,
                            uint8_t blue,
                            uint8_t alpha)
  {
    uint8_t p[4];
    unsigned int size;

    switch (image.GetFormat())
    {
      case PixelFormat_RGBA32:
        p[0] = red;
        p[1] = green;
        p[2] = blue;
        p[3] = alpha;
        size = 4;
        break;

      case PixelFormat_BGRA32:
        p[0] = blue;
        p[1] = green;
        p[2] = red;
        p[3] = alpha;
        size = 4;
        break;

      case PixelFormat_RGB24:
        p[0] = red;
        p[1] = green;
        p[2] = blue;
        size = 3;
        break;

      default:
        throw OrthancException(ErrorCode_NotImplemented);
    }    

    for (unsigned int y = 0; y < image.GetHeight(); y++)
    {
      uint8_t* q = reinterpret_cast<uint8_t*>(image.GetRow(y));

      for (unsigned int x = 0; x < image.GetWidth(); x++)
      {
        for (unsigned int i = 0; i < size; i++)
        {
          q[i] = p[i];
        }

        q += size;
      }
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


  void ImageProcessing::GetMinMaxIntegerValue(int64_t& minValue,
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

      case PixelFormat_Grayscale32:
      {
        uint32_t a, b;
        GetMinMaxValueInternal<uint32_t>(a, b, image);
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


  void ImageProcessing::GetMinMaxFloatValue(float& minValue,
                                            float& maxValue,
                                            const ImageAccessor& image)
  {
    switch (image.GetFormat())
    {
      case PixelFormat_Float32:
      {
        assert(sizeof(float) == 32);
        float a, b;
        GetMinMaxValueInternal<float>(a, b, image);
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
                                         float factor,
                                         bool useRound)
  {
    switch (image.GetFormat())
    {
      case PixelFormat_Grayscale8:
        if (useRound)
        {
          MultiplyConstantInternal<uint8_t, true>(image, factor);
        }
        else
        {
          MultiplyConstantInternal<uint8_t, false>(image, factor);
        }
        return;

      case PixelFormat_Grayscale16:
        if (useRound)
        {
          MultiplyConstantInternal<uint16_t, true>(image, factor);
        }
        else
        {
          MultiplyConstantInternal<uint16_t, false>(image, factor);
        }
        return;

      case PixelFormat_SignedGrayscale16:
        if (useRound)
        {
          MultiplyConstantInternal<int16_t, true>(image, factor);
        }
        else
        {
          MultiplyConstantInternal<int16_t, false>(image, factor);
        }
        return;

      default:
        throw OrthancException(ErrorCode_NotImplemented);
    }
  }


  void ImageProcessing::ShiftScale(ImageAccessor& image,
                                   float offset,
                                   float scaling,
                                   bool useRound)
  {
    switch (image.GetFormat())
    {
      case PixelFormat_Grayscale8:
        if (useRound)
        {
          ShiftScaleInternal<uint8_t, true>(image, offset, scaling);
        }
        else
        {
          ShiftScaleInternal<uint8_t, false>(image, offset, scaling);
        }
        return;

      case PixelFormat_Grayscale16:
        if (useRound)
        {
          ShiftScaleInternal<uint16_t, true>(image, offset, scaling);
        }
        else
        {
          ShiftScaleInternal<uint16_t, false>(image, offset, scaling);
        }
        return;

      case PixelFormat_SignedGrayscale16:
        if (useRound)
        {
          ShiftScaleInternal<int16_t, true>(image, offset, scaling);
        }
        else
        {
          ShiftScaleInternal<int16_t, false>(image, offset, scaling);
        }
        return;

      default:
        throw OrthancException(ErrorCode_NotImplemented);
    }
  }


  void ImageProcessing::Invert(ImageAccessor& image)
  {
    switch (image.GetFormat())
    {
      case PixelFormat_Grayscale8:
      {
        for (unsigned int y = 0; y < image.GetHeight(); y++)
        {
          uint8_t* p = reinterpret_cast<uint8_t*>(image.GetRow(y));

          for (unsigned int x = 0; x < image.GetWidth(); x++, p++)
          {
            *p = 255 - (*p);
          }
        }
        
        return;
      }

      default:
        throw OrthancException(ErrorCode_NotImplemented);
    }   
  }



  namespace
  {
    template <Orthanc::PixelFormat Format>
    class BresenhamPixelWriter
    {
    private:
      typedef typename PixelTraits<Format>::PixelType  PixelType;
    
      Orthanc::ImageAccessor&  image_;
      PixelType                value_;

      void PlotLineLow(int x0,
                       int y0,
                       int x1,
                       int y1)
      {
        int dx = x1 - x0;
        int dy = y1 - y0;
        int yi = 1;

        if (dy < 0)
        {
          yi = -1;
          dy = -dy;
        }

        int d = 2 * dy - dx;
        int y = y0;

        for (int x = x0; x <= x1; x++)
        {
          Write(x, y);
          
          if (d > 0)
          {
            y = y + yi;
            d = d - 2 * dx;
          }
      
          d = d + 2*dy;
        }
      }
      
      void PlotLineHigh(int x0,
                        int y0,
                        int x1,
                        int y1)
      {
        int dx = x1 - x0;
        int dy = y1 - y0;
        int xi = 1;
    
        if (dx < 0)
        {
          xi = -1;
          dx = -dx;
        }
    
        int d = 2 * dx - dy;
        int x = x0;

        for (int y = y0; y <= y1; y++)
        {
          Write(x, y);
          
          if (d > 0)
          {
            x = x + xi;
            d = d - 2 * dy;
          }
      
          d = d + 2 * dx;
        }
      }

    public:
      BresenhamPixelWriter(Orthanc::ImageAccessor& image,
                           int64_t value) :
        image_(image),
        value_(PixelTraits<Format>::IntegerToPixel(value))
      {
      }

      BresenhamPixelWriter(Orthanc::ImageAccessor& image,
                           const PixelType& value) :
        image_(image),
        value_(value)
      {
      }

      void Write(int x,
                 int y)
      {
        if (x >= 0 &&
            y >= 0 &&
            static_cast<unsigned int>(x) < image_.GetWidth() &&
            static_cast<unsigned int>(y) < image_.GetHeight())
        {
          PixelType* p = reinterpret_cast<PixelType*>(image_.GetRow(y));
          p[x] = value_;
        }
      }

      void DrawSegment(int x0,
                       int y0,
                       int x1,
                       int y1)
      {
        // This is an implementation of Bresenham's line algorithm
        // https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm#All_cases
    
        if (abs(y1 - y0) < abs(x1 - x0))
        {
          if (x0 > x1)
          {
            PlotLineLow(x1, y1, x0, y0);
          }
          else
          {
            PlotLineLow(x0, y0, x1, y1);
          }
        }
        else
        {
          if (y0 > y1)
          {
            PlotLineHigh(x1, y1, x0, y0);
          }
          else
          {
            PlotLineHigh(x0, y0, x1, y1);
          }
        }
      }
    };
  }

  
  void ImageProcessing::DrawLineSegment(ImageAccessor& image,
                                        int x0,
                                        int y0,
                                        int x1,
                                        int y1,
                                        int64_t value)
  {
    switch (image.GetFormat())
    {       
      case Orthanc::PixelFormat_Grayscale8:
      {
        BresenhamPixelWriter<Orthanc::PixelFormat_Grayscale8> writer(image, value);
        writer.DrawSegment(x0, y0, x1, y1);
        break;
      }

      case Orthanc::PixelFormat_Grayscale16:
      {
        BresenhamPixelWriter<Orthanc::PixelFormat_Grayscale16> writer(image, value);
        writer.DrawSegment(x0, y0, x1, y1);
        break;
      }

      case Orthanc::PixelFormat_SignedGrayscale16:
      {
        BresenhamPixelWriter<Orthanc::PixelFormat_SignedGrayscale16> writer(image, value);
        writer.DrawSegment(x0, y0, x1, y1);
        break;
      }
        
      default:
        throw Orthanc::OrthancException(Orthanc::ErrorCode_NotImplemented);
    }
  }

  
  void ImageProcessing::DrawLineSegment(ImageAccessor& image,
                                        int x0,
                                        int y0,
                                        int x1,
                                        int y1,
                                        uint8_t red,
                                        uint8_t green,
                                        uint8_t blue,
                                        uint8_t alpha)
  {
    switch (image.GetFormat())
    {
      case Orthanc::PixelFormat_BGRA32:
      {
        PixelTraits<Orthanc::PixelFormat_BGRA32>::PixelType pixel;
        pixel.red_ = red;
        pixel.green_ = green;
        pixel.blue_ = blue;
        pixel.alpha_ = alpha;

        BresenhamPixelWriter<Orthanc::PixelFormat_BGRA32> writer(image, pixel);
        writer.DrawSegment(x0, y0, x1, y1);
        break;
      }
        
      case Orthanc::PixelFormat_RGB24:
      {
        PixelTraits<Orthanc::PixelFormat_RGB24>::PixelType pixel;
        pixel.red_ = red;
        pixel.green_ = green;
        pixel.blue_ = blue;

        BresenhamPixelWriter<Orthanc::PixelFormat_RGB24> writer(image, pixel);
        writer.DrawSegment(x0, y0, x1, y1);
        break;
      }
        
      default:
        throw Orthanc::OrthancException(Orthanc::ErrorCode_NotImplemented);
    }
  }
}
