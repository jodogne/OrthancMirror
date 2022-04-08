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
#include "ImageProcessing.h"

#include "Image.h"
#include "ImageTraits.h"
#include "PixelTraits.h"
#include "../OrthancException.h"

#ifdef __EMSCRIPTEN__
/* 
   Avoid this error:
   -----------------
   .../boost/math/special_functions/round.hpp:118:12: warning: implicit conversion from 'std::__2::numeric_limits<long long>::type' (aka 'long long') to 'float' changes value from 9223372036854775807 to 9223372036854775808 [-Wimplicit-int-float-conversion]
   .../mnt/c/osi/dev/orthanc/Core/Images/ImageProcessing.cpp:333:28: note: in instantiation of function template specialization 'boost::math::llround<float>' requested here
   .../mnt/c/osi/dev/orthanc/Core/Images/ImageProcessing.cpp:1006:9: note: in instantiation of function template specialization 'Orthanc::MultiplyConstantInternal<unsigned char, true>' requested here
*/
#pragma GCC diagnostic ignored "-Wimplicit-int-float-conversion"
#endif 

#include <boost/math/special_functions/round.hpp>

#include <algorithm>
#include <cassert>
#include <limits>
#include <list>
#include <map>
#include <stdint.h>
#include <string.h>

namespace Orthanc
{
  ImageProcessing::ImagePoint::ImagePoint(int32_t x,
                                          int32_t y) :
    x_(x),
    y_(y)
  {
  }

  int32_t ImageProcessing::ImagePoint::GetX() const
  {
    return x_;
  }

  int32_t ImageProcessing::ImagePoint::GetY() const
  {
    return y_;
  }

  void ImageProcessing::ImagePoint::Set(int32_t x, int32_t y)
  {
    x_ = x;
    y_ = y;
  }

  void ImageProcessing::ImagePoint::ClipTo(int32_t minX, int32_t maxX, int32_t minY, int32_t maxY)
  {
    x_ = std::max(minX, std::min(maxX, x_));
    y_ = std::max(minY, std::min(maxY, y_));
  }

  double ImageProcessing::ImagePoint::GetDistanceTo(const ImagePoint& other) const
  {
    double dx = (double)(other.GetX() - GetX());
    double dy = (double)(other.GetY() - GetY());
    return sqrt(dx * dx + dy * dy);
  }

  double ImageProcessing::ImagePoint::GetDistanceToLine(double a, double b, double c) const // where ax + by + c = 0 is the equation of the line
  {
    return std::abs(a * static_cast<double>(GetX()) + b * static_cast<double>(GetY()) + c) / pow(a * a + b * b, 0.5);
  }

  template <typename TargetType, typename SourceType>
  static void ConvertInternal(ImageAccessor& target,
                              const ImageAccessor& source)
  {
    // WARNING - "::min()" should be replaced by "::lowest()" if
    // dealing with float or double (which is not the case so far)
    assert(sizeof(TargetType) <= 2);  // Safeguard to remember about "float/double"
    const TargetType minValue = std::numeric_limits<TargetType>::min();
    const TargetType maxValue = std::numeric_limits<TargetType>::max();

    const unsigned int width = source.GetWidth();
    const unsigned int height = source.GetHeight();
    
    for (unsigned int y = 0; y < height; y++)
    {
      TargetType* t = reinterpret_cast<TargetType*>(target.GetRow(y));
      const SourceType* s = reinterpret_cast<const SourceType*>(source.GetConstRow(y));

      for (unsigned int x = 0; x < width; x++, t++, s++)
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

    const unsigned int width = source.GetWidth();
    const unsigned int height = source.GetHeight();
    
    for (unsigned int y = 0; y < height; y++)
    {
      float* t = reinterpret_cast<float*>(target.GetRow(y));
      const SourceType* s = reinterpret_cast<const SourceType*>(source.GetConstRow(y));

      for (unsigned int x = 0; x < width; x++, t++, s++)
      {
        *t = static_cast<float>(*s);
      }
    }
  }


  template <PixelFormat TargetFormat>
  static void ConvertFloatToGrayscale(ImageAccessor& target,
                                      const ImageAccessor& source)
  {
    typedef typename PixelTraits<TargetFormat>::PixelType  TargetType;
    
    assert(sizeof(float) == 4);

    const unsigned int width = source.GetWidth();
    const unsigned int height = source.GetHeight();

    for (unsigned int y = 0; y < height; y++)
    {
      TargetType* q = reinterpret_cast<TargetType*>(target.GetRow(y));
      const float* p = reinterpret_cast<const float*>(source.GetConstRow(y));

      for (unsigned int x = 0; x < width; x++, p++, q++)
      {
        PixelTraits<TargetFormat>::FloatToPixel(*q, *p);
      }
    }
  }


  template <typename TargetType>
  static void ConvertColorToGrayscale(ImageAccessor& target,
                                      const ImageAccessor& source)
  {
    assert(source.GetFormat() == PixelFormat_RGB24);

    // WARNING - "::min()" should be replaced by "::lowest()" if
    // dealing with float or double (which is not the case so far)
    assert(sizeof(TargetType) <= 2);  // Safeguard to remember about "float/double"
    const TargetType minValue = std::numeric_limits<TargetType>::min();
    const TargetType maxValue = std::numeric_limits<TargetType>::max();

    const unsigned int width = source.GetWidth();
    const unsigned int height = source.GetHeight();
    
    for (unsigned int y = 0; y < height; y++)
    {
      TargetType* t = reinterpret_cast<TargetType*>(target.GetRow(y));
      const uint8_t* s = reinterpret_cast<const uint8_t*>(source.GetConstRow(y));

      for (unsigned int x = 0; x < width; x++, t++, s += 3)
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


  static void MemsetZeroInternal(ImageAccessor& image)
  {
    const unsigned int height = image.GetHeight();
    const size_t lineSize = image.GetBytesPerPixel() * image.GetWidth();
    const size_t pitch = image.GetPitch();

    uint8_t *p = reinterpret_cast<uint8_t*>(image.GetBuffer());
    
    for (unsigned int y = 0; y < height; y++)
    {
      memset(p, 0, lineSize);
      p += pitch;
    }
  }


  template <typename PixelType>
  static void SetInternal(ImageAccessor& image,
                          int64_t constant)
  {
    if (constant == 0 &&
        (image.GetFormat() == PixelFormat_Grayscale8 ||
         image.GetFormat() == PixelFormat_Grayscale16 ||
         image.GetFormat() == PixelFormat_Grayscale32 ||
         image.GetFormat() == PixelFormat_Grayscale64 ||
         image.GetFormat() == PixelFormat_SignedGrayscale16))
    {
      MemsetZeroInternal(image);
    }
    else
    {
      const unsigned int width = image.GetWidth();
      const unsigned int height = image.GetHeight();

      for (unsigned int y = 0; y < height; y++)
      {
        PixelType* p = reinterpret_cast<PixelType*>(image.GetRow(y));

        for (unsigned int x = 0; x < width; x++, p++)
        {
          *p = static_cast<PixelType>(constant);
        }
      }
    }
  }


  template <typename PixelType>
  static void GetMinMaxValueInternal(PixelType& minValue,
                                     PixelType& maxValue,
                                     const ImageAccessor& source,
                                     const PixelType LowestValue = std::numeric_limits<PixelType>::min())
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
    maxValue = LowestValue;

    const unsigned int height = source.GetHeight();
    const unsigned int width = source.GetWidth();

    for (unsigned int y = 0; y < height; y++)
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

    // WARNING - "::min()" should be replaced by "::lowest()" if
    // dealing with float or double (which is not the case so far)
    assert(sizeof(PixelType) <= 2);  // Safeguard to remember about "float/double"
    const int64_t minValue = std::numeric_limits<PixelType>::min();
    const int64_t maxValue = std::numeric_limits<PixelType>::max();

    const unsigned int width = image.GetWidth();
    const unsigned int height = image.GetHeight();
    
    for (unsigned int y = 0; y < height; y++)
    {
      PixelType* p = reinterpret_cast<PixelType*>(image.GetRow(y));

      for (unsigned int x = 0; x < width; x++, p++)
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

    // WARNING - "::min()" should be replaced by "::lowest()" if
    // dealing with float or double (which is not the case so far)
    assert(sizeof(PixelType) <= 2);  // Safeguard to remember about "float/double"
    const int64_t minValue = std::numeric_limits<PixelType>::min();
    const int64_t maxValue = std::numeric_limits<PixelType>::max();

    const unsigned int width = image.GetWidth();
    const unsigned int height = image.GetHeight();

    for (unsigned int y = 0; y < height; y++)
    {
      PixelType* p = reinterpret_cast<PixelType*>(image.GetRow(y));

      for (unsigned int x = 0; x < width; x++, p++)
      {
        int64_t v;
        if (UseRound)
        {
          assert(sizeof(long long) == sizeof(int64_t));
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


  // Computes "a * x + b" at each pixel => Note that this is not the
  // same convention as in "ShiftScale()", but it is the convention of
  // "ShiftScale2()"
  template <typename TargetType,
            typename SourceType,
            bool UseRound,
            bool Invert>
  static void ShiftScaleIntegerInternal(ImageAccessor& target,
                                        const ImageAccessor& source,
                                        float a,
                                        float b)
  // This function can be applied inplace (source == target)
  {
    assert(target.GetFormat() != PixelFormat_Float32);
    
    if (source.GetWidth() != target.GetWidth() ||
        source.GetHeight() != target.GetHeight())
    {
      throw OrthancException(ErrorCode_IncompatibleImageSize);
    }

    if (&source == &target &&
        source.GetFormat() != target.GetFormat())
    {
      throw OrthancException(ErrorCode_IncompatibleImageFormat);
    }
    
    const TargetType minPixelValue = std::numeric_limits<TargetType>::min();
    const TargetType maxPixelValue = std::numeric_limits<TargetType>::max();
    const float minFloatValue = static_cast<float>(minPixelValue);
    const float maxFloatValue = static_cast<float>(maxPixelValue);

    const unsigned int height = target.GetHeight();
    const unsigned int width = target.GetWidth();
    
    for (unsigned int y = 0; y < height; y++)
    {
      TargetType* p = reinterpret_cast<TargetType*>(target.GetRow(y));
      const SourceType* q = reinterpret_cast<const SourceType*>(source.GetConstRow(y));

      for (unsigned int x = 0; x < width; x++, p++, q++)
      {
        float v = a * static_cast<float>(*q) + b;

        if (v >= maxFloatValue)
        {
          *p = maxPixelValue;
        }
        else if (v <= minFloatValue)
        {
          *p = minPixelValue;
        }
        else if (UseRound)
        {         
          // The "round" operation is very costly
          assert(sizeof(TargetType) < sizeof(int));
          *p = static_cast<TargetType>(boost::math::iround(v));
        }
        else
        {
          *p = static_cast<TargetType>(std::floor(v));
        }

        if (Invert)
        {
          *p = maxPixelValue - *p;
        }
      }
    }
  }


  template <typename SourceType>
  static void ShiftScaleFloatInternal(ImageAccessor& target,
                                      const ImageAccessor& source,
                                      float a,
                                      float b)
  // This function can be applied inplace (source == target)
  {
    assert(target.GetFormat() == PixelFormat_Float32);
    
    if (source.GetWidth() != target.GetWidth() ||
        source.GetHeight() != target.GetHeight())
    {
      throw OrthancException(ErrorCode_IncompatibleImageSize);
    }

    if (&source == &target &&
        source.GetFormat() != target.GetFormat())
    {
      throw OrthancException(ErrorCode_IncompatibleImageFormat);
    }
    
    const unsigned int height = target.GetHeight();
    const unsigned int width = target.GetWidth();
    
    for (unsigned int y = 0; y < height; y++)
    {
      float* p = reinterpret_cast<float*>(target.GetRow(y));
      const SourceType* q = reinterpret_cast<const SourceType*>(source.GetConstRow(y));

      for (unsigned int x = 0; x < width; x++, p++, q++)
      {
        *p = a * static_cast<float>(*q) + b;
      }
    }
  }
  

  template <typename PixelType>
  static void ShiftRightInternal(ImageAccessor& image,
                                 unsigned int shift)
  {
    const unsigned int height = image.GetHeight();
    const unsigned int width = image.GetWidth();

    for (unsigned int y = 0; y < height; y++)
    {
      PixelType* p = reinterpret_cast<PixelType*>(image.GetRow(y));

      for (unsigned int x = 0; x < width; x++, p++)
      {
        *p = *p >> shift;
      }
    }
  }

  template <typename PixelType>
  static void ShiftLeftInternal(ImageAccessor& image,
                                unsigned int shift)
  {
    const unsigned int height = image.GetHeight();
    const unsigned int width = image.GetWidth();

    for (unsigned int y = 0; y < height; y++)
    {
      PixelType* p = reinterpret_cast<PixelType*>(image.GetRow(y));

      for (unsigned int x = 0; x < width; x++, p++)
      {
        *p = *p << shift;
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

    const unsigned int lineSize = source.GetBytesPerPixel() * source.GetWidth();
    assert(source.GetPitch() >= lineSize && target.GetPitch() >= lineSize);

    const unsigned int height = source.GetHeight();
    for (unsigned int y = 0; y < height; y++)
    {
      memcpy(target.GetRow(y), source.GetConstRow(y), lineSize);
    }
  }

  template <typename TargetType, typename SourceType>
  static void ApplyWindowingInternal(ImageAccessor& target,
                                     const ImageAccessor& source,
                                     float windowCenter,
                                     float windowWidth,
                                     float rescaleSlope,
                                     float rescaleIntercept,
                                     bool invert)
  {
    assert(sizeof(SourceType) == source.GetBytesPerPixel() &&
           sizeof(TargetType) == target.GetBytesPerPixel());
    
    const TargetType maxTargetValue = std::numeric_limits<TargetType>::max();
    const float maxFloatValue = static_cast<float>(maxTargetValue);
    
    const float windowIntercept = windowCenter - windowWidth / 2.0f;
    const float windowSlope = (maxFloatValue + 1.0f) / windowWidth;

    const float a = rescaleSlope * windowSlope;
    const float b = (rescaleIntercept - windowIntercept) * windowSlope;

    if (invert)
    {
      ShiftScaleIntegerInternal<TargetType, SourceType, false, true>(target, source, a, b);
    }
    else
    {
      ShiftScaleIntegerInternal<TargetType, SourceType, false, false>(target, source, a, b);
    }
  }

  void ImageProcessing::ApplyWindowing_Deprecated(ImageAccessor& target,
                                                  const ImageAccessor& source,
                                                  float windowCenter,
                                                  float windowWidth,
                                                  float rescaleSlope,
                                                  float rescaleIntercept,
                                                  bool invert)
  {
    if (target.GetWidth() != source.GetWidth() ||
        target.GetHeight() != source.GetHeight())
    {
      throw OrthancException(ErrorCode_IncompatibleImageSize);
    }

    switch (source.GetFormat())
    {
      case PixelFormat_Float32:
      {
        switch (target.GetFormat())
        {
          case PixelFormat_Grayscale8:
            ApplyWindowingInternal<uint8_t, float>(target, source, windowCenter, windowWidth, rescaleSlope, rescaleIntercept, invert);
            break;
          case PixelFormat_Grayscale16:
            ApplyWindowingInternal<uint16_t, float>(target, source, windowCenter, windowWidth, rescaleSlope, rescaleIntercept, invert);
            break;
          default:
            throw OrthancException(ErrorCode_NotImplemented);
        }
      };break;
      case PixelFormat_Grayscale8:
      {
        switch (target.GetFormat())
        {
          case PixelFormat_Grayscale8:
            ApplyWindowingInternal<uint8_t, uint8_t>(target, source, windowCenter, windowWidth, rescaleSlope, rescaleIntercept, invert);
            break;
          case PixelFormat_Grayscale16:
            ApplyWindowingInternal<uint16_t, uint8_t>(target, source, windowCenter, windowWidth, rescaleSlope, rescaleIntercept, invert);
            break;
          default:
            throw OrthancException(ErrorCode_NotImplemented);
        }
      };break;
      case PixelFormat_Grayscale16:
      {
        switch (target.GetFormat())
        {
          case PixelFormat_Grayscale8:
            ApplyWindowingInternal<uint8_t, uint16_t>(target, source, windowCenter, windowWidth, rescaleSlope, rescaleIntercept, invert);
            break;
          case PixelFormat_Grayscale16:
            ApplyWindowingInternal<uint16_t, uint16_t>(target, source, windowCenter, windowWidth, rescaleSlope, rescaleIntercept, invert);
            break;
          default:
            throw OrthancException(ErrorCode_NotImplemented);
        }
      };break;
      default:
        throw OrthancException(ErrorCode_NotImplemented);
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

    const unsigned int width = source.GetWidth();
    const unsigned int height = source.GetHeight();

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
      for (unsigned int y = 0; y < height; y++)
      {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(source.GetConstRow(y));
        uint8_t* q = reinterpret_cast<uint8_t*>(target.GetRow(y));
        for (unsigned int x = 0; x < width; x++, q++)
        {
          *q = static_cast<uint8_t>((2126 * static_cast<uint32_t>(p[0]) +
                                     7152 * static_cast<uint32_t>(p[1]) +
                                     0722 * static_cast<uint32_t>(p[2])) / 10000);
          p += 4;
        }
      }

      return;
    }

    if (target.GetFormat() == PixelFormat_Grayscale8 &&
        source.GetFormat() == PixelFormat_BGRA32)
    {
      for (unsigned int y = 0; y < height; y++)
      {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(source.GetConstRow(y));
        uint8_t* q = reinterpret_cast<uint8_t*>(target.GetRow(y));
        for (unsigned int x = 0; x < width; x++, q++)
        {
          *q = static_cast<uint8_t>((2126 * static_cast<uint32_t>(p[2]) +
                                     7152 * static_cast<uint32_t>(p[1]) +
                                     0722 * static_cast<uint32_t>(p[0])) / 10000);
          p += 4;
        }
      }

      return;
    }

    if (target.GetFormat() == PixelFormat_RGB24 &&
        source.GetFormat() == PixelFormat_RGBA32)
    {
      for (unsigned int y = 0; y < height; y++)
      {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(source.GetConstRow(y));
        uint8_t* q = reinterpret_cast<uint8_t*>(target.GetRow(y));
        for (unsigned int x = 0; x < width; x++)
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
      for (unsigned int y = 0; y < height; y++)
      {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(source.GetConstRow(y));
        uint8_t* q = reinterpret_cast<uint8_t*>(target.GetRow(y));
        for (unsigned int x = 0; x < width; x++)
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
      for (unsigned int y = 0; y < height; y++)
      {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(source.GetConstRow(y));
        uint8_t* q = reinterpret_cast<uint8_t*>(target.GetRow(y));
        for (unsigned int x = 0; x < width; x++)
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
      for (unsigned int y = 0; y < height; y++)
      {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(source.GetConstRow(y));
        uint8_t* q = reinterpret_cast<uint8_t*>(target.GetRow(y));
        for (unsigned int x = 0; x < width; x++)
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

    if ((target.GetFormat() == PixelFormat_RGBA32 ||
         target.GetFormat() == PixelFormat_BGRA32) &&
        source.GetFormat() == PixelFormat_Grayscale8)
    {
      for (unsigned int y = 0; y < height; y++)
      {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(source.GetConstRow(y));
        uint8_t* q = reinterpret_cast<uint8_t*>(target.GetRow(y));
        for (unsigned int x = 0; x < width; x++)
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
      for (unsigned int y = 0; y < height; y++)
      {
        const uint16_t* p = reinterpret_cast<const uint16_t*>(source.GetConstRow(y));
        uint8_t* q = reinterpret_cast<uint8_t*>(target.GetRow(y));
        for (unsigned int x = 0; x < width; x++)
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
      for (unsigned int y = 0; y < height; y++)
      {
        const int16_t* p = reinterpret_cast<const int16_t*>(source.GetConstRow(y));
        uint8_t* q = reinterpret_cast<uint8_t*>(target.GetRow(y));
        for (unsigned int x = 0; x < width; x++)
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
      for (unsigned int y = 0; y < height; y++)
      {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(source.GetConstRow(y));
        uint8_t* q = reinterpret_cast<uint8_t*>(target.GetRow(y));
        for (unsigned int x = 0; x < width; x++)
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

    if ((target.GetFormat() == PixelFormat_BGRA32 &&
         source.GetFormat() == PixelFormat_RGBA32)
        || (target.GetFormat() == PixelFormat_RGBA32 &&
            source.GetFormat() == PixelFormat_BGRA32))
    {
      for (unsigned int y = 0; y < height; y++)
      {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(source.GetConstRow(y));
        uint8_t* q = reinterpret_cast<uint8_t*>(target.GetRow(y));
        for (unsigned int x = 0; x < width; x++)
        {
          q[0] = p[2];
          q[1] = p[1];
          q[2] = p[0];
          q[3] = p[3];
          p += 4;
          q += 4;
        }
      }

      return;
    }

    if (target.GetFormat() == PixelFormat_RGB24 &&
        source.GetFormat() == PixelFormat_RGB48)
    {
      for (unsigned int y = 0; y < height; y++)
      {
        const uint16_t* p = reinterpret_cast<const uint16_t*>(source.GetConstRow(y));
        uint8_t* q = reinterpret_cast<uint8_t*>(target.GetRow(y));
        for (unsigned int x = 0; x < width; x++)
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

    if (target.GetFormat() == PixelFormat_Grayscale16 &&
        source.GetFormat() == PixelFormat_Float32)
    {
      ConvertFloatToGrayscale<PixelFormat_Grayscale16>(target, source);
      return;
    }

    if (target.GetFormat() == PixelFormat_Grayscale8 &&
        source.GetFormat() == PixelFormat_Float32)
    {
      ConvertFloatToGrayscale<PixelFormat_Grayscale8>(target, source);
      return;
    }

    if (target.GetFormat() == PixelFormat_RGB24 &&
        source.GetFormat() == PixelFormat_Float32)
    {
      ConvertFloatToGrayscale<PixelFormat_RGB24>(target, source);
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

      case PixelFormat_Grayscale32:
        SetInternal<uint32_t>(image, value);
        return;

      case PixelFormat_Grayscale64:
        SetInternal<uint64_t>(image, value);
        return;

      case PixelFormat_SignedGrayscale16:
        SetInternal<int16_t>(image, value);
        return;

      case PixelFormat_Float32:
        assert(sizeof(float) == 4);
        SetInternal<float>(image, value);
        return;

      case PixelFormat_RGBA32:
      case PixelFormat_BGRA32:
      case PixelFormat_RGB24:
      {
        uint8_t v = static_cast<uint8_t>(value);
        Set(image, v, v, v, v);  // Use the color version
        return;
      }

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
      case PixelFormat_Grayscale8:
      {
        // New in Orthanc 1.9.0
        uint8_t grayscale = (2126 * static_cast<uint16_t>(red) + 
                             7152 * static_cast<uint16_t>(green) +
                             0722 * static_cast<uint16_t>(blue)) / 10000;
        Orthanc::ImageProcessing::Set(image, grayscale);
        return;
      }
      
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

    const unsigned int width = image.GetWidth();
    const unsigned int height = image.GetHeight();

    for (unsigned int y = 0; y < height; y++)
    {
      uint8_t* q = reinterpret_cast<uint8_t*>(image.GetRow(y));

      for (unsigned int x = 0; x < width; x++)
      {
        for (unsigned int i = 0; i < size; i++)
        {
          q[i] = p[i];
        }

        q += size;
      }
    }
  }

  void ImageProcessing::Set(ImageAccessor& image,
                            uint8_t red,
                            uint8_t green,
                            uint8_t blue,
                            ImageAccessor& alpha)
  {
    uint8_t p[4];

    if (alpha.GetWidth() != image.GetWidth() || alpha.GetHeight() != image.GetHeight())
    {
      throw OrthancException(ErrorCode_IncompatibleImageSize);
    }

    if (alpha.GetFormat() != PixelFormat_Grayscale8)
    {
      throw OrthancException(ErrorCode_NotImplemented);
    }

    switch (image.GetFormat())
    {
      case PixelFormat_RGBA32:
        p[0] = red;
        p[1] = green;
        p[2] = blue;
        break;

      case PixelFormat_BGRA32:
        p[0] = blue;
        p[1] = green;
        p[2] = red;
        break;

      default:
        throw OrthancException(ErrorCode_NotImplemented);
    }

    const unsigned int width = image.GetWidth();
    const unsigned int height = image.GetHeight();

    for (unsigned int y = 0; y < height; y++)
    {
      uint8_t* q = reinterpret_cast<uint8_t*>(image.GetRow(y));
      uint8_t* a = reinterpret_cast<uint8_t*>(alpha.GetRow(y));

      for (unsigned int x = 0; x < width; x++)
      {
        for (unsigned int i = 0; i < 3; i++)
        {
          q[i] = p[i];
        }
        q[3] = *a;
        q += 4;
        ++a;
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

    switch (image.GetFormat())
    {
      case PixelFormat_Grayscale8:
      {
        ShiftRightInternal<uint8_t>(image, shift);
        break;
      }

      case PixelFormat_Grayscale16:
      {
        ShiftRightInternal<uint16_t>(image, shift);
        break;
      }
      default:
        throw OrthancException(ErrorCode_NotImplemented);
    }
  }

  void ImageProcessing::ShiftLeft(ImageAccessor& image,
                                  unsigned int shift)
  {
    if (image.GetWidth() == 0 ||
        image.GetHeight() == 0 ||
        shift == 0)
    {
      // Nothing to do
      return;
    }

    switch (image.GetFormat())
    {
      case PixelFormat_Grayscale8:
      {
        ShiftLeftInternal<uint8_t>(image, shift);
        break;
      }

      case PixelFormat_Grayscale16:
      {
        ShiftLeftInternal<uint16_t>(image, shift);
        break;
      }
      default:
        throw OrthancException(ErrorCode_NotImplemented);
    }
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
        assert(sizeof(float) == 4);
        float a, b;

        /**
         * WARNING - On floating-point types, the minimal value is
         * "-FLT_MAX" (as implemented by "::lowest()"), not "FLT_MIN"
         * (as implemented by "::min()")
         * https://en.cppreference.com/w/cpp/types/numeric_limits
         **/
        GetMinMaxValueInternal<float>(a, b, image, -std::numeric_limits<float>::max());
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


  static bool IsIdentityRescaling(float offset,
                                  float scaling)
  {
    return (std::abs(offset) <= 10.0f * std::numeric_limits<float>::epsilon() &&
            std::abs(scaling - 1.0f) <= 10.0f * std::numeric_limits<float>::epsilon());
  }
  

  void ImageProcessing::ShiftScale2(ImageAccessor& image,
                                    float offset,
                                    float scaling,
                                    bool useRound)
  {
    // We compute "a * x + b"
    const float a = scaling;
    const float b = offset;

    if (IsIdentityRescaling(offset, scaling))
    {
      return;
    }
    
    switch (image.GetFormat())
    {
      case PixelFormat_Grayscale8:
        if (useRound)
        {
          ShiftScaleIntegerInternal<uint8_t, uint8_t, true, false>(image, image, a, b);
        }
        else
        {
          ShiftScaleIntegerInternal<uint8_t, uint8_t, false, false>(image, image, a, b);
        }
        return;

      case PixelFormat_Grayscale16:
        if (useRound)
        {
          ShiftScaleIntegerInternal<uint16_t, uint16_t, true, false>(image, image, a, b);
        }
        else
        {
          ShiftScaleIntegerInternal<uint16_t, uint16_t, false, false>(image, image, a, b);
        }
        return;

      case PixelFormat_SignedGrayscale16:
        if (useRound)
        {
          ShiftScaleIntegerInternal<int16_t, int16_t, true, false>(image, image, a, b);
        }
        else
        {
          ShiftScaleIntegerInternal<int16_t, int16_t, false, false>(image, image, a, b);
        }
        return;

      case PixelFormat_Float32:
        if (useRound)
        {
          ShiftScaleFloatInternal<float>(image, image, a, b);
        }
        else
        {
          ShiftScaleFloatInternal<float>(image, image, a, b);
        }
        return;

      default:
        throw OrthancException(ErrorCode_NotImplemented);
    }
  }


  void ImageProcessing::ShiftScale2(ImageAccessor& target,
                                    const ImageAccessor& source,
                                    float offset,
                                    float scaling,
                                    bool useRound)
  {
    // We compute "a * x + b"
    const float a = scaling;
    const float b = offset;
    
    if (target.GetFormat() == source.GetFormat() &&
        IsIdentityRescaling(offset, scaling))
    {
      Copy(target, source);
      return;
    }
    
    switch (target.GetFormat())
    {
      case PixelFormat_Grayscale8:

        switch (source.GetFormat())
        {
          case PixelFormat_Float32:
            if (useRound)
            {
              ShiftScaleIntegerInternal<uint8_t, float, true, false>(target, source, a, b);
            }
            else
            {
              ShiftScaleIntegerInternal<uint8_t, float, false, false>(target, source, a, b);
            }
            return;

          default:
            throw OrthancException(ErrorCode_NotImplemented);
        }
        
      default:
        throw OrthancException(ErrorCode_NotImplemented);
    }
  }


  void ImageProcessing::ShiftScale(ImageAccessor& image,
                                   float offset,
                                   float scaling,
                                   bool useRound)
  {
    // Rewrite "(x + offset) * scaling" as "a * x + b"

    const float a = scaling;
    const float b = offset * scaling;
    ShiftScale2(image, b, a, useRound);
  }


  void ImageProcessing::ShiftScale(ImageAccessor& target,
                                   const ImageAccessor& source,
                                   float offset,
                                   float scaling,
                                   bool useRound)
  {
    // Rewrite "(x + offset) * scaling" as "a * x + b"

    const float a = scaling;
    const float b = offset * scaling;
    ShiftScale2(target, source, b, a, useRound);
  }



  void ImageProcessing::Invert(ImageAccessor& image, int64_t maxValue)
  {
    const unsigned int width = image.GetWidth();
    const unsigned int height = image.GetHeight();

    switch (image.GetFormat())
    {
      case PixelFormat_Grayscale16:
      {
        uint16_t maxValueUint16 = (uint16_t)(std::min(maxValue, static_cast<int64_t>(std::numeric_limits<uint16_t>::max())));

        for (unsigned int y = 0; y < height; y++)
        {
          uint16_t* p = reinterpret_cast<uint16_t*>(image.GetRow(y));

          for (unsigned int x = 0; x < width; x++, p++)
          {
            *p = maxValueUint16 - (*p);
          }
        }

        return;
      }
      case PixelFormat_Grayscale8:
      {
        uint8_t maxValueUint8 = (uint8_t)(std::min(maxValue, static_cast<int64_t>(std::numeric_limits<uint8_t>::max())));

        for (unsigned int y = 0; y < height; y++)
        {
          uint8_t* p = reinterpret_cast<uint8_t*>(image.GetRow(y));

          for (unsigned int x = 0; x < width; x++, p++)
          {
            *p = maxValueUint8 - (*p);
          }
        }

        return;
      }

      default:
        throw OrthancException(ErrorCode_NotImplemented);
    }

  }

  void ImageProcessing::Invert(ImageAccessor& image)
  {
    switch (image.GetFormat())
    {
      case PixelFormat_Grayscale8:
        return Invert(image, 255);
      default:
        throw OrthancException(ErrorCode_NotImplemented); // you should use the Invert(image, maxValue) overload
    }
  }



  namespace
  {
    template <PixelFormat Format>
    class BresenhamPixelWriter
    {
    private:
      typedef typename PixelTraits<Format>::PixelType  PixelType;

      ImageAccessor&  image_;
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
      BresenhamPixelWriter(ImageAccessor& image,
                           int64_t value) :
        image_(image),
        value_(PixelTraits<Format>::IntegerToPixel(value))
      {
      }

      BresenhamPixelWriter(ImageAccessor& image,
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
      case PixelFormat_Grayscale8:
      {
        BresenhamPixelWriter<PixelFormat_Grayscale8> writer(image, value);
        writer.DrawSegment(x0, y0, x1, y1);
        break;
      }

      case PixelFormat_Grayscale16:
      {
        BresenhamPixelWriter<PixelFormat_Grayscale16> writer(image, value);
        writer.DrawSegment(x0, y0, x1, y1);
        break;
      }

      case PixelFormat_SignedGrayscale16:
      {
        BresenhamPixelWriter<PixelFormat_SignedGrayscale16> writer(image, value);
        writer.DrawSegment(x0, y0, x1, y1);
        break;
      }

      default:
        throw OrthancException(ErrorCode_NotImplemented);
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
      case PixelFormat_BGRA32:
      {
        PixelTraits<PixelFormat_BGRA32>::PixelType pixel;
        pixel.red_ = red;
        pixel.green_ = green;
        pixel.blue_ = blue;
        pixel.alpha_ = alpha;

        BresenhamPixelWriter<PixelFormat_BGRA32> writer(image, pixel);
        writer.DrawSegment(x0, y0, x1, y1);
        break;
      }

      case PixelFormat_RGBA32:
      {
        PixelTraits<PixelFormat_RGBA32>::PixelType pixel;
        pixel.red_ = red;
        pixel.green_ = green;
        pixel.blue_ = blue;
        pixel.alpha_ = alpha;

        BresenhamPixelWriter<PixelFormat_RGBA32> writer(image, pixel);
        writer.DrawSegment(x0, y0, x1, y1);
        break;
      }

      case PixelFormat_RGB24:
      {
        PixelTraits<PixelFormat_RGB24>::PixelType pixel;
        pixel.red_ = red;
        pixel.green_ = green;
        pixel.blue_ = blue;

        BresenhamPixelWriter<PixelFormat_RGB24> writer(image, pixel);
        writer.DrawSegment(x0, y0, x1, y1);
        break;
      }

      default:
        throw OrthancException(ErrorCode_NotImplemented);
    }
  }

  void ComputePolygonExtent(int32_t& left, int32_t& right, int32_t& top, int32_t& bottom, const std::vector<ImageProcessing::ImagePoint>& points)
  {
    left = std::numeric_limits<int32_t>::max();
    right = std::numeric_limits<int32_t>::min();
    top = std::numeric_limits<int32_t>::max();
    bottom = std::numeric_limits<int32_t>::min();

    for (size_t i = 0; i < points.size(); i++)
    {
      const ImageProcessing::ImagePoint& p = points[i];
      left = std::min(p.GetX(), left);
      right = std::max(p.GetX(), right);
      bottom = std::max(p.GetY(), bottom);
      top = std::min(p.GetY(), top);
    }
  }


  namespace
  {
#define USE_POLYGON_FRACTIONS 1

    class PolygonEdge
    {
    private:
      int   yUpper;

#if USE_POLYGON_FRACTIONS == 1
      int  x;
      int  xOffset;
      int  dxPerScanNumerator;
      int  dxPerScanDenominator;
#else
      float xIntersect;
      float dxPerScan;
#endif

    public:
      PolygonEdge(const ImageProcessing::ImagePoint& lower,
                  const ImageProcessing::ImagePoint& upper,
                  int yComp)
      {
        // cf. "makeEdgeRec()" in textbook

        assert(upper.GetY() != lower.GetY());

#if USE_POLYGON_FRACTIONS == 1
        x = lower.GetX();
        xOffset = 0;
        dxPerScanNumerator = upper.GetX() - lower.GetX();
        dxPerScanDenominator = upper.GetY() - lower.GetY();
#else
        dxPerScan = (static_cast<float>(upper.GetX() - lower.GetX()) /
                     static_cast<float>(upper.GetY() - lower.GetY()));
        xIntersect = lower.GetX();
#endif
    
        if (upper.GetY() < yComp)
        {
          yUpper = upper.GetY() - 1;
        }
        else
        {
          yUpper = upper.GetY();
        }
      }

      void NextScanLine()
      {
#if USE_POLYGON_FRACTIONS == 1
        xOffset += dxPerScanNumerator;

        while (xOffset >= dxPerScanDenominator)
        {
          x++;
          xOffset -= dxPerScanDenominator;
        }

        while (xOffset < 0)
        {
          x--;
          xOffset += dxPerScanDenominator;
        }

#else
        xIntersect += dxPerScan;
#endif
      }


      int GetEnterX() const
      {
#if USE_POLYGON_FRACTIONS == 1
        assert(xOffset >= 0 && xOffset < dxPerScanDenominator);
        if (xOffset == 0)
        {
          return x;
        }
        else
        {
          return x + 1;
        }
#else
        return static_cast<int>(std::ceil(xIntersect));
#endif
      }
  
      int GetExitX() const
      {
#if USE_POLYGON_FRACTIONS == 1
        assert(xOffset >= 0 && xOffset < dxPerScanDenominator);
        return x;
#else
        return static_cast<int>(std::floor(xIntersect));
#endif
      }
  
      int GetUpperY() const
      {
        return yUpper;
      }

      bool operator< (const PolygonEdge& other) const
      {
#if USE_POLYGON_FRACTIONS == 1
        assert(xOffset >= 0 && xOffset < dxPerScanDenominator);
        assert(other.xOffset >= 0 && other.xOffset < other.dxPerScanDenominator);
        return x < other.x;
#else
        // cf. "insertEdge()" in textbook
        return (xIntersect < other.xIntersect);
#endif
      }
    };
  }
  

  // For an index, return y-coordinate of next nonhorizontal line
  static int GetPolygonNextY(const std::vector<ImageProcessing::ImagePoint>& points,
                             size_t k)
  {
    // cf. "yNext()" in textbook
    size_t j = k;
  
    for (;;)
    {
      j++;
      if (j == points.size())
      {
        j = 0;
      }

      if (points[k].GetY() != points[j].GetY())
      {
        return points[j].GetY();
      }
    }
  }


  static int GetPolygonPreviousY(const std::vector<ImageProcessing::ImagePoint>& points,
                                 size_t k)
  {
    size_t j = k;

    for (;;)
    {
      if (j > 0)
      {
        j --;
      }
      else
      {
        j = points.size() - 1;
      }

      if (points[k].GetY() != points[j].GetY())
      {
        return points[j].GetY();
      }
    }
  }



  void ImageProcessing::FillPolygon(IPolygonFiller& filler,
                                    const std::vector<ImagePoint>& points)
  {
    /**
     * This implementation is a C++ adaption of Section 3.11 (pages
     * 117-124) of textbook "Computer Graphics - C Version (2nd
     * Edition)" by Hearn and Baker, 1997.
     **/
  
    typedef std::map<int, std::list<PolygonEdge> > EdgeTable;

    if (points.size() < 2)
    {
      return;
    }

    bool onlyHorizontalSegments = true;
    for (size_t i = 1; i < points.size(); i++)
    {
      if (points[0].GetY() != points[i].GetY())
      {
        onlyHorizontalSegments = false;
        break;
      }
    }

    if (onlyHorizontalSegments)
    {
      // Degenerate case: There are only horizontal lines. If this is
      // the case, "GetPolygonPreviousY()" would be an infinite loop
      int x1 = points[0].GetX();
      int x2 = x1;
      for (size_t i = 1; i < points.size(); i++)
      {
        assert(points[i].GetY() == points[0].GetY());

        const int x = points[i].GetX();
        x1 = std::min(x1, x);
        x2 = std::max(x2, x);
      }
      filler.Fill(points[0].GetY(), x1, x2);
      return;
    }

    EdgeTable globalEdgeTable;

    // cf. "buildEdgeList()" in textbook

    // Error in the textbook: we use "GetPolygonPreviousY()" instead of "points.size() - 2"
    int yPrev = GetPolygonPreviousY(points, points.size() - 1);
    ImagePoint v1(points[points.size() - 1]);

    for (size_t i = 0; i < points.size(); i++)
    {
      ImagePoint v2(points[i]);

      if (v1.GetY() != v2.GetY())
      {
        // Non-horizontal line
        if (v1.GetY() < v2.GetY())
        {
          // Up-going edge
          PolygonEdge edge(v1, v2, GetPolygonNextY(points, i));
          globalEdgeTable[v1.GetY()].push_back(edge);
        }
        else if (v1.GetY() > v2.GetY())
        {
          // Down-going edge
          PolygonEdge edge(v2, v1, yPrev);
          globalEdgeTable[v2.GetY()].push_back(edge);
        }

        // Error in the textbook: "yPrev" must NOT be updated on horizontal lines
        yPrev = v1.GetY();
      }
      
      v1 = v2;
    }
    
    assert(!globalEdgeTable.empty());

    std::vector<PolygonEdge> activeEdges;

    for (EdgeTable::const_iterator it = globalEdgeTable.begin(); it != globalEdgeTable.end(); ++it)
    {
      // cf. "buildActiveList()" in textbook
      activeEdges.reserve(activeEdges.size() + it->second.size());
      for (std::list<PolygonEdge>::const_iterator it2 = it->second.begin(); it2 != it->second.end(); ++it2)
      {
        activeEdges.push_back(*it2);
      }

      assert(!activeEdges.empty());

      EdgeTable::const_iterator next = it;
      ++next;

      int rampEnd;
      if (next == globalEdgeTable.end())
      {
        rampEnd = activeEdges[0].GetUpperY() + 1;

        for (size_t i = 1; i < activeEdges.size(); i++)
        {
          rampEnd = std::max(rampEnd, activeEdges[i].GetUpperY() + 1);
        }
      }
      else
      {
        rampEnd = next->first;
      }

      for (int y = it->first; y < rampEnd; y++)
      {
        // cf. "updateActiveList()" in textbook
        std::vector<PolygonEdge> stillActive;
        stillActive.reserve(activeEdges.size());

        for (size_t i = 0; i < activeEdges.size(); i++)
        {
          if (y <= activeEdges[i].GetUpperY())
          {
            stillActive.push_back(activeEdges[i]);
          }
        }

        activeEdges.swap(stillActive);

        assert(activeEdges.size() % 2 == 0);
        std::sort(activeEdges.begin(), activeEdges.end());

        // cf. "fillScan()" in textbook
        for (size_t k = 0; k + 1 < activeEdges.size(); )
        {
          int a = activeEdges[k].GetExitX();
          int b = activeEdges[k + 1].GetEnterX();

          // Fix wrt. the textbook: merge overlapping segments
          k += 2;
          while (k + 1 < activeEdges.size() &&
                 activeEdges[k].GetExitX() == b)
          {
            assert(a <= b);
            b = activeEdges[k + 1].GetEnterX();
            k += 2;
          }

          assert(a <= b);
          filler.Fill(y, a, b);
        }

        // cf. "updateActiveList()" in textbook
        for (size_t k = 0; k < activeEdges.size(); k++)
        {
          activeEdges[k].NextScanLine();
        }
      }
    }
  }

  
  void ImageProcessing::FillPolygon(ImageAccessor& image,
                                    const std::vector<ImagePoint>& points,
                                    int64_t value)
  {
    class Filler : public IPolygonFiller
    {
    private:
      ImageAccessor& image_;
      int64_t        value_;

    public:
      Filler(ImageAccessor& image,
             int64_t value) :
        image_(image),
        value_(value)
      {
      }
      
      virtual void Fill(int y,
                        int x1,
                        int x2) ORTHANC_OVERRIDE
      {
        assert(x1 <= x2);
    
        if (x1 < static_cast<int>(image_.GetWidth()) &&
            x2 >= 0 &&
            y >= 0 &&
            y < static_cast<int>(image_.GetHeight()))
        {
          unsigned int yy = static_cast<unsigned int>(y);
          unsigned int a = static_cast<unsigned int>(std::max(0, x1));
          unsigned int b = static_cast<unsigned int>(std::min(x2, static_cast<int>(image_.GetWidth()) - 1));

          assert(a <= b);

          ImageAccessor region;
          image_.GetRegion(region, a, yy, b - a + 1, 1);
          Set(region, value_);
        }
      }
    };

    
    if (image.GetFormat() == PixelFormat_Grayscale8 ||
        image.GetFormat() == PixelFormat_Grayscale16 ||
        image.GetFormat() == PixelFormat_SignedGrayscale16)
    {
      Filler filler(image, value);
      FillPolygon(filler, points);
    }
    else
    {
      throw OrthancException(ErrorCode_NotImplemented);
    }
  }


  template <PixelFormat Format>
  static void ResizeInternal(ImageAccessor& target,
                             const ImageAccessor& source)
  {
    assert(target.GetFormat() == source.GetFormat() &&
           target.GetFormat() == Format);
      
    const unsigned int sourceWidth = source.GetWidth();
    const unsigned int sourceHeight = source.GetHeight();
    const unsigned int targetWidth = target.GetWidth();
    const unsigned int targetHeight = target.GetHeight();

    if (targetWidth == 0 || targetHeight == 0)
    {
      return;
    }

    if (sourceWidth == 0 || sourceHeight == 0)
    {
      // Avoids division by zero below
      ImageProcessing::Set(target, 0);
      return;
    }
      
    const float scaleX = static_cast<float>(sourceWidth) / static_cast<float>(targetWidth);
    const float scaleY = static_cast<float>(sourceHeight) / static_cast<float>(targetHeight);


    /**
     * Create two lookup tables to quickly know the (x,y) position
     * in the source image, given the (x,y) position in the target
     * image.
     **/
      
    std::vector<unsigned int>  lookupX(targetWidth);
      
    for (unsigned int x = 0; x < targetWidth; x++)
    {
      int sourceX = static_cast<int>(std::floor((static_cast<float>(x) + 0.5f) * scaleX));
      if (sourceX < 0)
      {
        sourceX = 0;  // Should never happen
      }
      else if (sourceX >= static_cast<int>(sourceWidth))
      {
        sourceX = sourceWidth - 1;
      }

      lookupX[x] = static_cast<unsigned int>(sourceX);
    }
      
    std::vector<unsigned int>  lookupY(targetHeight);
      
    for (unsigned int y = 0; y < targetHeight; y++)
    {
      int sourceY = static_cast<int>(std::floor((static_cast<float>(y) + 0.5f) * scaleY));
      if (sourceY < 0)
      {
        sourceY = 0;  // Should never happen
      }
      else if (sourceY >= static_cast<int>(sourceHeight))
      {
        sourceY = sourceHeight - 1;
      }

      lookupY[y] = static_cast<unsigned int>(sourceY);
    }


    /**
     * Actual resizing
     **/
      
    for (unsigned int targetY = 0; targetY < targetHeight; targetY++)
    {
      unsigned int sourceY = lookupY[targetY];

      for (unsigned int targetX = 0; targetX < targetWidth; targetX++)
      {
        unsigned int sourceX = lookupX[targetX];

        typename ImageTraits<Format>::PixelType pixel;
        ImageTraits<Format>::GetPixel(pixel, source, sourceX, sourceY);
        ImageTraits<Format>::SetPixel(target, pixel, targetX, targetY);
      }
    }            
  }



  void ImageProcessing::Resize(ImageAccessor& target,
                               const ImageAccessor& source)
  {
    if (source.GetFormat() != target.GetFormat())
    {
      throw OrthancException(ErrorCode_IncompatibleImageFormat);
    }

    if (source.GetWidth() == target.GetWidth() &&
        source.GetHeight() == target.GetHeight())
    {
      Copy(target, source);
      return;
    }

    switch (source.GetFormat())
    {
      case PixelFormat_Grayscale8:
        ResizeInternal<PixelFormat_Grayscale8>(target, source);
        break;

      case PixelFormat_Float32:
        ResizeInternal<PixelFormat_Float32>(target, source);
        break;

      case PixelFormat_RGB24:
        ResizeInternal<PixelFormat_RGB24>(target, source);
        break;

      default:
        throw OrthancException(ErrorCode_NotImplemented);
    }
  }


  ImageAccessor* ImageProcessing::Halve(const ImageAccessor& source,
                                        bool forceMinimalPitch)
  {
    std::unique_ptr<Image> target(new Image(source.GetFormat(), source.GetWidth() / 2,
                                            source.GetHeight() / 2, forceMinimalPitch));
    Resize(*target, source);
    return target.release();
  }

    
  template <PixelFormat Format>
  static void FlipXInternal(ImageAccessor& image)
  {     
    const unsigned int height = image.GetHeight();
    const unsigned int width = image.GetWidth();

    for (unsigned int y = 0; y < height; y++)
    {
      for (unsigned int x1 = 0; x1 < width / 2; x1++)
      {
        unsigned int x2 = width - 1 - x1;
          
        typename ImageTraits<Format>::PixelType a, b;
        ImageTraits<Format>::GetPixel(a, image, x1, y);
        ImageTraits<Format>::GetPixel(b, image, x2, y);
        ImageTraits<Format>::SetPixel(image, a, x2, y);
        ImageTraits<Format>::SetPixel(image, b, x1, y);
      }
    }        
  }

    
  void ImageProcessing::FlipX(ImageAccessor& image)
  {
    switch (image.GetFormat())
    {
      case PixelFormat_Grayscale8:
        FlipXInternal<PixelFormat_Grayscale8>(image);
        break;

      case PixelFormat_RGB24:
        FlipXInternal<PixelFormat_RGB24>(image);
        break;

      default:
        throw OrthancException(ErrorCode_NotImplemented);
    }
  }

    
  template <PixelFormat Format>
  static void FlipYInternal(ImageAccessor& image)
  {     
    const unsigned int height = image.GetHeight();
    const unsigned int width = image.GetWidth();

    for (unsigned int y1 = 0; y1 < height / 2; y1++)
    {
      unsigned int y2 = height - 1 - y1;
        
      for (unsigned int x = 0; x < width; x++)
      {
        typename ImageTraits<Format>::PixelType a, b;
        ImageTraits<Format>::GetPixel(a, image, x, y1);
        ImageTraits<Format>::GetPixel(b, image, x, y2);
        ImageTraits<Format>::SetPixel(image, a, x, y2);
        ImageTraits<Format>::SetPixel(image, b, x, y1);
      }
    }        
  }

    
  void ImageProcessing::FlipY(ImageAccessor& image)
  {
    switch (image.GetFormat())
    {
      case PixelFormat_Grayscale8:
        FlipYInternal<PixelFormat_Grayscale8>(image);
        break;

      case PixelFormat_RGB24:
        FlipYInternal<PixelFormat_RGB24>(image);
        break;

      default:
        throw OrthancException(ErrorCode_NotImplemented);
    }
  }


  // This is a slow implementation of horizontal convolution on one
  // individual channel, that checks for out-of-image values
  template <typename RawPixel, unsigned int ChannelsCount>
  static float GetHorizontalConvolutionFloatSecure(const ImageAccessor& source,
                                                   const std::vector<float>& horizontal,
                                                   size_t horizontalAnchor,
                                                   unsigned int x,
                                                   unsigned int y,
                                                   float leftBorder,
                                                   float rightBorder,
                                                   unsigned int channel)
  {
    const RawPixel* row = reinterpret_cast<const RawPixel*>(source.GetConstRow(y)) + channel;

    float p = 0;

    for (unsigned int k = 0; k < horizontal.size(); k++)
    {
      float value;

      if (x + k < horizontalAnchor)   // Negation of "x - horizontalAnchor + k >= 0"
      {
        value = leftBorder;
      }
      else if (x + k >= source.GetWidth() + horizontalAnchor)   // Negation of "x - horizontalAnchor + k < width"
      {
        value = rightBorder;
      }
      else
      {
        // The value lies within the image
        value = row[(x - horizontalAnchor + k) * ChannelsCount];
      }

      p += value * horizontal[k];
    }

    return p;
  }
  

  
  // This is an implementation of separable convolution that uses
  // floating-point arithmetics, and an intermediate Float32
  // image. The out-of-image values are taken as the border
  // value. Further optimization is possible.
  template <typename RawPixel, unsigned int ChannelsCount, bool UseRound>
  static void SeparableConvolutionFloat(ImageAccessor& image /* inplace */,
                                        const std::vector<float>& horizontal,
                                        size_t horizontalAnchor,
                                        const std::vector<float>& vertical,
                                        size_t verticalAnchor,
                                        float normalization)
  {
    // WARNING - "::min()" should be replaced by "::lowest()" if
    // dealing with float or double (which is not the case so far)
    assert(sizeof(RawPixel) <= 2);  // Safeguard to remember about "float/double"

    const unsigned int width = image.GetWidth();
    const unsigned int height = image.GetHeight();
    

    /**
     * Horizontal convolution
     **/

    Image tmp(PixelFormat_Float32, ChannelsCount * width, height, false);

    for (unsigned int y = 0; y < height; y++)
    {
      const RawPixel* row = reinterpret_cast<const RawPixel*>(image.GetConstRow(y));

      float leftBorder[ChannelsCount], rightBorder[ChannelsCount];
      
      for (unsigned int c = 0; c < ChannelsCount; c++)
      {
        leftBorder[c] = row[c];
        rightBorder[c] = row[ChannelsCount * (width - 1) + c];
      }

      float* p = static_cast<float*>(tmp.GetRow(y));

      if (width < horizontal.size())
      {
        // It is not possible to have the full kernel within the image, use the direct implementation
        for (unsigned int x = 0; x < width; x++)
        {
          for (unsigned int c = 0; c < ChannelsCount; c++, p++)
          {
            *p = GetHorizontalConvolutionFloatSecure<RawPixel, ChannelsCount>
              (image, horizontal, horizontalAnchor, x, y, leftBorder[c], rightBorder[c], c);
          }
        }
      }
      else
      {
        // Deal with the left border
        for (unsigned int x = 0; x < horizontalAnchor; x++)
        {
          for (unsigned int c = 0; c < ChannelsCount; c++, p++)
          {
            *p = GetHorizontalConvolutionFloatSecure<RawPixel, ChannelsCount>
              (image, horizontal, horizontalAnchor, x, y, leftBorder[c], rightBorder[c], c);
          }
        }

        // Deal with the central portion of the image (all pixel values
        // scanned by the kernel lie inside the image)

        for (unsigned int x = 0; x < width - horizontal.size() + 1; x++)
        {
          for (unsigned int c = 0; c < ChannelsCount; c++, p++)
          {
            *p = 0;
            for (unsigned int k = 0; k < horizontal.size(); k++)
            {
              *p += static_cast<float>(row[(x + k) * ChannelsCount + c]) * horizontal[k];
            }
          }
        }

        // Deal with the right border
        for (unsigned int x = static_cast<unsigned int>(
               horizontalAnchor + width - horizontal.size() + 1); x < width; x++)
        {
          for (unsigned int c = 0; c < ChannelsCount; c++, p++)
          {
            *p = GetHorizontalConvolutionFloatSecure<RawPixel, ChannelsCount>
              (image, horizontal, horizontalAnchor, x, y, leftBorder[c], rightBorder[c], c);
          }
        }
      }
    }


    /**
     * Vertical convolution
     **/

    std::vector<const float*> rows(vertical.size());

    for (unsigned int y = 0; y < height; y++)
    {
      for (unsigned int k = 0; k < vertical.size(); k++)
      {
        if (y + k < verticalAnchor)
        {
          rows[k] = reinterpret_cast<const float*>(tmp.GetConstRow(0));   // Use top border
        }
        else if (y + k >= height + verticalAnchor)
        {
          rows[k] = reinterpret_cast<const float*>(tmp.GetConstRow(height - 1));  // Use bottom border
        }
        else
        {
          rows[k] = reinterpret_cast<const float*>(tmp.GetConstRow(static_cast<unsigned int>(y + k - verticalAnchor)));
        }
      }

      RawPixel* p = reinterpret_cast<RawPixel*>(image.GetRow(y));
        
      for (unsigned int x = 0; x < width; x++)
      {
        for (unsigned int c = 0; c < ChannelsCount; c++, p++)
        {
          float accumulator = 0;
        
          for (unsigned int k = 0; k < vertical.size(); k++)
          {
            accumulator += rows[k][ChannelsCount * x + c] * vertical[k];
          }

          accumulator *= normalization;

          if (accumulator <= static_cast<float>(std::numeric_limits<RawPixel>::min()))
          {
            *p = std::numeric_limits<RawPixel>::min();
          }
          else if (accumulator >= static_cast<float>(std::numeric_limits<RawPixel>::max()))
          {
            *p = std::numeric_limits<RawPixel>::max();
          }
          else
          {
            if (UseRound)
            {
              assert(sizeof(RawPixel) < sizeof(int));
              *p = static_cast<RawPixel>(boost::math::iround(accumulator));
            }
            else
            {
              *p = static_cast<RawPixel>(accumulator);
            }
          }
        }
      }
    }
  }


  void ImageProcessing::SeparableConvolution(ImageAccessor& image /* inplace */,
                                             const std::vector<float>& horizontal,
                                             size_t horizontalAnchor,
                                             const std::vector<float>& vertical,
                                             size_t verticalAnchor,
                                             bool useRound)
  {
    if (horizontal.size() == 0 ||
        vertical.size() == 0 ||
        horizontalAnchor >= horizontal.size() ||
        verticalAnchor >= vertical.size())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
    
    if (image.GetWidth() == 0 ||
        image.GetHeight() == 0)
    {
      return;
    }
    
    /**
     * Compute normalization
     **/
    
    float sumHorizontal = 0;
    for (size_t i = 0; i < horizontal.size(); i++)
    {
      sumHorizontal += horizontal[i];
    }
    
    float sumVertical = 0;
    for (size_t i = 0; i < vertical.size(); i++)
    {
      sumVertical += vertical[i];
    }

    if (fabsf(sumHorizontal) <= std::numeric_limits<float>::epsilon() ||
        fabsf(sumVertical) <= std::numeric_limits<float>::epsilon())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange, "Singular convolution kernel");
    }      

    const float normalization = 1.0f / (sumHorizontal * sumVertical);

    switch (image.GetFormat())
    {
      case PixelFormat_Grayscale8:
        if (useRound)
        {
          SeparableConvolutionFloat<uint8_t, 1u, true>
            (image, horizontal, horizontalAnchor, vertical, verticalAnchor, normalization);
        }
        else
        {
          SeparableConvolutionFloat<uint8_t, 1u, false>
            (image, horizontal, horizontalAnchor, vertical, verticalAnchor, normalization);
        }
        break;

      case PixelFormat_RGB24:
        if (useRound)
        {
          SeparableConvolutionFloat<uint8_t, 3u, true>
            (image, horizontal, horizontalAnchor, vertical, verticalAnchor, normalization);
        }
        else
        {
          SeparableConvolutionFloat<uint8_t, 3u, false>
            (image, horizontal, horizontalAnchor, vertical, verticalAnchor, normalization);
        }
        break;

      default:
        throw OrthancException(ErrorCode_NotImplemented);
    }
  }


  void ImageProcessing::SmoothGaussian5x5(ImageAccessor& image,
                                          bool useRound)
  {
    std::vector<float> kernel(5);
    kernel[0] = 1;
    kernel[1] = 4;
    kernel[2] = 6;
    kernel[3] = 4;
    kernel[4] = 1;

    SeparableConvolution(image, kernel, 2, kernel, 2, useRound);
  }


  void ImageProcessing::FitSize(ImageAccessor& target,
                                const ImageAccessor& source)
  {
    if (target.GetWidth() == 0 ||
        target.GetHeight() == 0)
    {
      return;
    }

    if (source.GetWidth() == target.GetWidth() &&
        source.GetHeight() == target.GetHeight())
    {
      Copy(target, source);
      return;
    }

    Set(target, 0);

    // Preserve the aspect ratio
    float cw = static_cast<float>(source.GetWidth());
    float ch = static_cast<float>(source.GetHeight());
    float r = std::min(
      static_cast<float>(target.GetWidth()) / cw,
      static_cast<float>(target.GetHeight()) / ch);

    unsigned int sw = std::min(static_cast<unsigned int>(boost::math::iround(cw * r)), target.GetWidth());  
    unsigned int sh = std::min(static_cast<unsigned int>(boost::math::iround(ch * r)), target.GetHeight());

    Image resized(target.GetFormat(), sw, sh, false);
  
    //ImageProcessing::SmoothGaussian5x5(source);
    ImageProcessing::Resize(resized, source);

    assert(target.GetWidth() >= resized.GetWidth() &&
           target.GetHeight() >= resized.GetHeight());
    unsigned int offsetX = (target.GetWidth() - resized.GetWidth()) / 2;
    unsigned int offsetY = (target.GetHeight() - resized.GetHeight()) / 2;

    ImageAccessor region;
    target.GetRegion(region, offsetX, offsetY, resized.GetWidth(), resized.GetHeight());
    ImageProcessing::Copy(region, resized);
  }


  ImageAccessor* ImageProcessing::FitSize(const ImageAccessor& source,
                                          unsigned int width,
                                          unsigned int height)
  {
    std::unique_ptr<ImageAccessor> target(new Image(source.GetFormat(), width, height, false));
    FitSize(*target, source);
    return target.release();
  }

    
  ImageAccessor* ImageProcessing::FitSizeKeepAspectRatio(const ImageAccessor& source,
                                                         unsigned int width,
                                                         unsigned int height)
  {
    std::unique_ptr<ImageAccessor> target(new Image(source.GetFormat(), width, height, false));
    Set(*target, 0);

    if (width != 0 &&
        height != 0 &&
        source.GetWidth() != 0 &&
        source.GetHeight() != 0)
    {
      float ratio = std::min(static_cast<float>(width) / static_cast<float>(source.GetWidth()),
                             static_cast<float>(height) / static_cast<float>(source.GetHeight()));

      unsigned int resizedWidth = static_cast<unsigned int>(
        boost::math::iround(ratio * static_cast<float>(source.GetWidth())));

      unsigned int resizedHeight = static_cast<unsigned int>(
        boost::math::iround(ratio * static_cast<float>(source.GetHeight())));

      std::unique_ptr<ImageAccessor> resized(FitSize(source, resizedWidth, resizedHeight));

      ImageAccessor region;
      target->GetRegion(region, (width - resizedWidth) / 2,
                        (height - resizedHeight) / 2, resizedWidth, resizedHeight);
      Copy(region, *resized);
    }

    return target.release();
  }


  void ImageProcessing::ConvertJpegYCbCrToRgb(ImageAccessor& image)
  {
    // http://dicom.nema.org/medical/dicom/current/output/chtml/part03/sect_C.7.6.3.html#sect_C.7.6.3.1.2
    // https://en.wikipedia.org/wiki/YCbCr#JPEG_conversion
    
    // TODO - Check out the outcome of Mathieu's discussion about
    // truncation of YCbCr-to-RGB conversion:
    // https://groups.google.com/forum/#!msg/comp.protocols.dicom/JHuGeyWbTz8/ARoTWrJzAQAJ

    const unsigned int width = image.GetWidth();
    const unsigned int height = image.GetHeight();
    const unsigned int pitch = image.GetPitch();
    uint8_t* buffer = reinterpret_cast<uint8_t*>(image.GetBuffer());
        
    if (image.GetFormat() != PixelFormat_RGB24 ||
        pitch < 3 * width)
    {
      throw OrthancException(ErrorCode_IncompatibleImageFormat);
    }

    for (unsigned int y = 0; y < height; y++)
    {
      uint8_t* p = buffer + y * pitch;
          
      for (unsigned int x = 0; x < width; x++, p += 3)
      {
        const float Y  = p[0];
        const float Cb = p[1];
        const float Cr = p[2];

        const float result[3] = {
          Y                             + 1.402f    * (Cr - 128.0f),
          Y - 0.344136f * (Cb - 128.0f) - 0.714136f * (Cr - 128.0f),
          Y + 1.772f    * (Cb - 128.0f)
        };

        for (uint8_t i = 0; i < 3 ; i++)
        {
          if (result[i] < 0)
          {
            p[i] = 0;
          }
          else if (result[i] > 255)
          {
            p[i] = 255;
          }
          else
          {
            p[i] = static_cast<uint8_t>(result[i]);
          }
        }    
      }
    }
  }


  void ImageProcessing::SwapEndianness(ImageAccessor& image /* inplace */)
  {
    const unsigned int width = image.GetWidth();
    const unsigned int height = image.GetHeight();
    
    switch (image.GetFormat())
    {
      case PixelFormat_Grayscale8:
      case PixelFormat_RGB24:
      case PixelFormat_RGBA32:
      case PixelFormat_BGRA32:
        // No swapping required
        break;

      case PixelFormat_Grayscale16:
      case PixelFormat_SignedGrayscale16:
        for (unsigned int y = 0; y < height; y++)
        {
          uint8_t* t = reinterpret_cast<uint8_t*>(image.GetRow(y));
          for (unsigned int x = 0; x < width; x++)
          {
            uint8_t a = t[0];
            t[0] = t[1];
            t[1] = a;
            t += 2;
          }
        }    
        break;

      case PixelFormat_Grayscale32:
      case PixelFormat_Float32:
        for (unsigned int y = 0; y < height; y++)
        {
          uint8_t* t = reinterpret_cast<uint8_t*>(image.GetRow(y));
          for (unsigned int x = 0; x < width; x++)
          {
            uint8_t a = t[0];
            uint8_t b = t[1];
            t[0] = t[3];
            t[1] = t[2];
            t[2] = b;
            t[3] = a;
            t += 4;
          }
        }    
        break;

      case PixelFormat_RGB48:  // uint16_t per channel
        for (unsigned int y = 0; y < height; y++)
        {
          uint8_t* t = reinterpret_cast<uint8_t*>(image.GetRow(y));
          for (unsigned int x = 0; x < 3 * width; x++)
          {
            uint8_t a = t[0];
            t[0] = t[1];
            t[1] = a;
            t += 2;
          }
        }    
        break;

      default:
        throw OrthancException(ErrorCode_NotImplemented);
    }          
  }


  template <typename PixelType,
            typename Functor>
  static void ApplyImageOntoImage(Functor f,
                                  ImageAccessor& image /* inout */,
                                  const ImageAccessor& other)
  {
    const unsigned int width = image.GetWidth();
    const unsigned int height = image.GetHeight();
    
    if (width != other.GetWidth() ||
        height != other.GetHeight())
    {
      throw OrthancException(ErrorCode_IncompatibleImageSize);
    }
    else if (image.GetFormat() != other.GetFormat() ||
             GetBytesPerPixel(image.GetFormat()) != sizeof(PixelType))
    {
      throw OrthancException(ErrorCode_IncompatibleImageFormat);
    }
    else
    {
      for (unsigned int y = 0; y < height; y++)
      {
        PixelType* p = reinterpret_cast<PixelType*>(image.GetRow(y));
        const PixelType* q = reinterpret_cast<const PixelType*>(other.GetConstRow(y));
        
        for (unsigned int x = 0; x < width; x++, p++, q++)
        {
          f(*p, *q);
        }
      }
    }
  }


  namespace
  {
    // For older version of gcc, templated functors cannot be defined
    // as types internal to functions, hence the anonymous namespace
    
    struct MaximumFunctor
    {
      void operator() (uint8_t& a, const uint8_t& b)
      {
        a = std::max(a, b);
      }

      void operator() (uint16_t& a, const uint16_t& b)
      {
        a = std::max(a, b);
      }
    };
  }
  

  void ImageProcessing::Maximum(ImageAccessor& image,
                                const ImageAccessor& other)
  {
    switch (image.GetFormat())
    {
      case PixelFormat_Grayscale8:
        ApplyImageOntoImage<uint8_t, MaximumFunctor>(MaximumFunctor(), image, other);
        return;

      case PixelFormat_Grayscale16:
        ApplyImageOntoImage<uint16_t, MaximumFunctor>(MaximumFunctor(), image, other);
        return;

      default:
        throw OrthancException(ErrorCode_NotImplemented);
    }
  }
}
