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


#pragma once

#include "../Compatibility.h"  // For ORTHANC_FORCE_INLINE
#include "../Enumerations.h"

#include <limits>

namespace Orthanc
{
  template <PixelFormat format,
            typename _PixelType>
  struct IntegerPixelTraits
  {
    typedef _PixelType  PixelType;

    ORTHANC_FORCE_INLINE
    static PixelFormat GetPixelFormat()
    {
      return format;
    }

    ORTHANC_FORCE_INLINE
    static PixelType IntegerToPixel(int64_t value)
    {
      if (value < static_cast<int64_t>(std::numeric_limits<PixelType>::min()))
      {
        return std::numeric_limits<PixelType>::min();
      }
      else if (value > static_cast<int64_t>(std::numeric_limits<PixelType>::max()))
      {
        return std::numeric_limits<PixelType>::max();        
      }
      else
      {
        return static_cast<PixelType>(value);
      }
    }

    ORTHANC_FORCE_INLINE
    static void SetZero(PixelType& target)
    {
      target = 0;
    }

    ORTHANC_FORCE_INLINE
    static void SetMinValue(PixelType& target)
    {
      target = std::numeric_limits<PixelType>::min();
    }

    ORTHANC_FORCE_INLINE
    static void SetMaxValue(PixelType& target)
    {
      target = std::numeric_limits<PixelType>::max();
    }

    ORTHANC_FORCE_INLINE
    static void Copy(PixelType& target,
                     const PixelType& source)
    {
      target = source;
    }

    ORTHANC_FORCE_INLINE
    static float PixelToFloat(const PixelType& source)
    {
      return static_cast<float>(source);
    }

    ORTHANC_FORCE_INLINE
    static void FloatToPixel(PixelType& target,
                             float value)
    {
      value += 0.5f;
      if (value < static_cast<float>(std::numeric_limits<PixelType>::min()))
      {
        target = std::numeric_limits<PixelType>::min();
      }
      else if (value > static_cast<float>(std::numeric_limits<PixelType>::max()))
      {
        target = std::numeric_limits<PixelType>::max();
      }
      else
      {
        target = static_cast<PixelType>(value);
      }
    }

    ORTHANC_FORCE_INLINE
    static bool IsEqual(const PixelType& a,
                        const PixelType& b)
    {
      return a == b;
    }
  };


  template <PixelFormat Format>
  struct PixelTraits;


  template <>
  struct PixelTraits<PixelFormat_Grayscale8> :
    public IntegerPixelTraits<PixelFormat_Grayscale8, uint8_t>
  {
  };

  
  template <>
  struct PixelTraits<PixelFormat_Grayscale16> :
    public IntegerPixelTraits<PixelFormat_Grayscale16, uint16_t>
  {
  };

  
  template <>
  struct PixelTraits<PixelFormat_SignedGrayscale16> :
    public IntegerPixelTraits<PixelFormat_SignedGrayscale16, int16_t>
  {
  };


  template <>
  struct PixelTraits<PixelFormat_Grayscale32> :
    public IntegerPixelTraits<PixelFormat_Grayscale32, uint32_t>
  {
  };


  template <>
  struct PixelTraits<PixelFormat_Grayscale64> :
    public IntegerPixelTraits<PixelFormat_Grayscale64, uint64_t>
  {
  };


  template <>
  struct PixelTraits<PixelFormat_RGB24>
  {
    struct PixelType
    {
      uint8_t  red_;
      uint8_t  green_;
      uint8_t  blue_;
    };

    ORTHANC_FORCE_INLINE
    static PixelFormat GetPixelFormat()
    {
      return PixelFormat_RGB24;
    }

    ORTHANC_FORCE_INLINE
    static void SetZero(PixelType& target)
    {
      target.red_ = 0;
      target.green_ = 0;
      target.blue_ = 0;
    }

    ORTHANC_FORCE_INLINE
    static void Copy(PixelType& target,
                     const PixelType& source)
    {
      target.red_ = source.red_;
      target.green_ = source.green_;
      target.blue_ = source.blue_;
    }

    ORTHANC_FORCE_INLINE
    static bool IsEqual(const PixelType& a,
                        const PixelType& b)
    {
      return (a.red_ == b.red_ &&
              a.green_ == b.green_ &&
              a.blue_ == b.blue_);
    }

    ORTHANC_FORCE_INLINE
    static void FloatToPixel(PixelType& target,
                             float value)
    {
      uint8_t v;
      PixelTraits<PixelFormat_Grayscale8>::FloatToPixel(v, value);

      target.red_ = v;
      target.green_ = v;
      target.blue_ = v;
    }
  };


  template <>
  struct PixelTraits<PixelFormat_BGRA32>
  {
    struct PixelType
    {
      uint8_t  blue_;
      uint8_t  green_;
      uint8_t  red_;
      uint8_t  alpha_;
    };

    ORTHANC_FORCE_INLINE
    static PixelFormat GetPixelFormat()
    {
      return PixelFormat_BGRA32;
    }

    ORTHANC_FORCE_INLINE
    static void SetZero(PixelType& target)
    {
      target.blue_ = 0;
      target.green_ = 0;
      target.red_ = 0;
      target.alpha_ = 0;
    }

    ORTHANC_FORCE_INLINE
    static void Copy(PixelType& target,
                     const PixelType& source)
    {
      target.blue_ = source.blue_;
      target.green_ = source.green_;
      target.red_ = source.red_;
      target.alpha_ = source.alpha_;
    }

    ORTHANC_FORCE_INLINE
    static bool IsEqual(const PixelType& a,
                        const PixelType& b)
    {
      return (a.blue_ == b.blue_ &&
              a.green_ == b.green_ &&
              a.red_ == b.red_ &&
              a.alpha_ == b.alpha_);
    }

    ORTHANC_FORCE_INLINE
    static void FloatToPixel(PixelType& target,
                             float value)
    {
      uint8_t v;
      PixelTraits<PixelFormat_Grayscale8>::FloatToPixel(v, value);

      target.blue_ = v;
      target.green_ = v;
      target.red_ = v;
      target.alpha_ = 255;      
    }
  };


  template <>
  struct PixelTraits<PixelFormat_RGBA32>
  {
    struct PixelType
    {
      uint8_t  red_;
      uint8_t  green_;
      uint8_t  blue_;
      uint8_t  alpha_;
    };

    ORTHANC_FORCE_INLINE
    static PixelFormat GetPixelFormat()
    {
      return PixelFormat_RGBA32;
    }

    ORTHANC_FORCE_INLINE
    static void SetZero(PixelType& target)
    {
      target.red_ = 0;
      target.green_ = 0;
      target.blue_ = 0;
      target.alpha_ = 0;
    }

    ORTHANC_FORCE_INLINE
    static void Copy(PixelType& target,
                     const PixelType& source)
    {
      target.red_ = source.red_;
      target.green_ = source.green_;
      target.blue_ = source.blue_;
      target.alpha_ = source.alpha_;
    }

    ORTHANC_FORCE_INLINE
    static bool IsEqual(const PixelType& a,
                        const PixelType& b)
    {
      return (a.red_ == b.red_ &&
              a.green_ == b.green_ &&
              a.blue_ == b.blue_ &&
              a.alpha_ == b.alpha_);
    }

    ORTHANC_FORCE_INLINE
    static void FloatToPixel(PixelType& target,
                             float value)
    {
      uint8_t v;
      PixelTraits<PixelFormat_Grayscale8>::FloatToPixel(v, value);

      target.red_ = v;
      target.green_ = v;
      target.blue_ = v;
      target.alpha_ = 255;      
    }
  };


  template <>
  struct PixelTraits<PixelFormat_Float32>
  {
    typedef float  PixelType;

    ORTHANC_FORCE_INLINE
    static PixelFormat GetPixelFormat()
    {
      return PixelFormat_Float32;
    }

    ORTHANC_FORCE_INLINE
    static void SetZero(PixelType& target)
    {
      target = 0.0f;
    }

    ORTHANC_FORCE_INLINE
    static void Copy(PixelType& target,
                     const PixelType& source)
    {
      target = source;
    }

    ORTHANC_FORCE_INLINE
    static bool IsEqual(const PixelType& a,
                        const PixelType& b)
    {
      float tmp = (a - b);

      if (tmp < 0)
      {
        tmp = -tmp;
      }

      return tmp <= std::numeric_limits<float>::epsilon();
    }
    
    ORTHANC_FORCE_INLINE
    static void SetMinValue(PixelType& target)
    {
      // std::numeric_limits<float>::lowest is not supported on
      // all compilers (for instance, Visual Studio 9.0 2008)
      target = -std::numeric_limits<float>::max();
    }

    ORTHANC_FORCE_INLINE
    static void SetMaxValue(PixelType& target)
    {
      target = std::numeric_limits<float>::max();
    }

    ORTHANC_FORCE_INLINE
    static void FloatToPixel(PixelType& target,
                             float value)
    {
      target = value;
    }

    ORTHANC_FORCE_INLINE
    static float PixelToFloat(const PixelType& source)
    {
      return source;
    }
  };
}
