/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2026 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2026 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

#if defined(BOOST_MATH_ROUND_HPP)
#  error You should not manually include <boost/math/special_functions/round.hpp>
#endif

#include "Compatibility.h"


#ifdef __EMSCRIPTEN__
/*
   Avoid such errors:
   ------------------

   .../boost/math/special_functions/round.hpp:118:12: warning: implicit conversion from 'std::__2::numeric_limits<long long>::type' (aka 'long long') to 'float' changes value from 9223372036854775807 to 9223372036854775808 [-Wimplicit-int-float-conversion]
   .../boost/math/special_functions/round.hpp:125:11: note: in instantiation of function template specialization 'boost::math::llround<float, boost::math::policies::policy<boost::math::policies::default_policy, boost::math::policies::default_policy, boost::math::policies::default_policy, boost::math::policies::default_policy, boost::math::policies::default_policy, boost::math::policies::default_policy, boost::math::policies::default_policy, boost::math::policies::default_policy, boost::math::policies::default_policy, boost::math::policies::default_policy, boost::math::policies::default_policy> >' requested here

   .../boost/math/special_functions/round.hpp:86:12: warning: implicit conversion from 'std::__2::numeric_limits<int>::type' (aka 'int') to 'float' changes value from 2147483647 to 2147483648 [-Wimplicit-int-float-conversion]
   .../boost/math/special_functions/round.hpp:93:11: note: in instantiation of function template specialization 'boost::math::iround<float, boost::math::policies::policy<boost::math::policies::default_policy, boost::math::policies::default_policy, boost::math::policies::default_policy, boost::math::policies::default_policy, boost::math::policies::default_policy, boost::math::policies::default_policy, boost::math::policies::default_policy, boost::math::policies::default_policy, boost::math::policies::default_policy, boost::math::policies::default_policy, boost::math::policies::default_policy> >' requested here
*/
#  pragma GCC diagnostic ignored "-Wimplicit-int-float-conversion"
#endif


#if 1 || defined(_MSC_VER)

/**
 * This is for compatibility with Microsoft Visual Studio <= 2015.
 * Indeed, Boost 1.89.0 does not support MSVC 14.0, as can be seen in
 * this file:
 * https://www.boost.org/doc/libs/1_89_0/boost/math/tools/config.hpp
 **/

#include <cmath>
#include <limits>

namespace Orthanc
{
  namespace Math
  {
    namespace Internals
    {
      ORTHANC_FORCE_INLINE
      float RoundFloat(float v)
      {
        if (v >= 0.0f)
        {
          return std::floor(v + 0.5f);
        }
        else
        {
          return std::ceil(v - 0.5f);
        }
      }

      ORTHANC_FORCE_INLINE
      double RoundDouble(double v)
      {
        if (v >= 0.0)
        {
          return std::floor(v + 0.5);
        }
        else
        {
          return std::ceil(v - 0.5);
        }
      }

      template <typename T>
      ORTHANC_FORCE_INLINE
      bool RoundFloatWithChecks(T& target,
                                float source)
      {
        if (std::isnan(source) ||
            !std::isfinite(source))
        {
          return false;
        }
        else
        {
          float rounded = RoundFloat(source);
          if (rounded < static_cast<float>(std::numeric_limits<T>::min()) ||
              rounded > static_cast<float>(std::numeric_limits<T>::max()))
          {
            return false;
          }
          else
          {
            target = static_cast<T>(rounded);
            return true;
          }
        }
      }

      template <typename T>
      ORTHANC_FORCE_INLINE
      bool RoundDoubleWithChecks(T& target,
                                 double source)
      {
        if (std::isnan(source) ||
            !std::isfinite(source))
        {
          return false;
        }
        else
        {
          double rounded = RoundDouble(source);
          if (rounded < static_cast<double>(std::numeric_limits<T>::min()) ||
              rounded > static_cast<double>(std::numeric_limits<T>::max()))
          {
            return false;
          }
          else
          {
            target = static_cast<T>(rounded);
            return true;
          }
        }
      }
    }

    ORTHANC_FORCE_INLINE
    int iround(float v)
    {
      return static_cast<int>(Internals::RoundFloat(v));
    }

    ORTHANC_FORCE_INLINE
    int iround(double v)
    {
      return static_cast<int>(Internals::RoundDouble(v));
    }

    ORTHANC_FORCE_INLINE
    long long llround(float v)
    {
      return static_cast<long long>(Internals::RoundFloat(v));
    }

    ORTHANC_FORCE_INLINE
    long long llround(double v)
    {
      return static_cast<long long>(Internals::RoundDouble(v));
    }

    ORTHANC_FORCE_INLINE
    bool llround(long long& target,
                 float source)
    {
      return Internals::RoundFloatWithChecks<long long>(target, source);
    }

    ORTHANC_FORCE_INLINE
    long long llround(long long& target,
                      double source)
    {
      return Internals::RoundDoubleWithChecks<long long>(target, source);
    }
  }
}

#else

#include <boost/math/special_functions/round.hpp>

namespace Orthanc
{
  namespace Math
  {
    ORTHANC_FORCE_INLINE
    int iround(float v)
    {
      return boost::math::iround<float>(v);
    }

    ORTHANC_FORCE_INLINE
    int iround(double v)
    {
      return boost::math::iround<double>(v);
    }

    ORTHANC_FORCE_INLINE
    long long llround(float v)
    {
      return boost::math::llround<float>(v);
    }

    ORTHANC_FORCE_INLINE
    long long llround(double v)
    {
      return boost::math::llround<double>(v);
    }

    ORTHANC_FORCE_INLINE
    bool llround(long long& target,
                 float source)
    {
      try
      {
        target = boost::math::llround<float>(source);
        return true;
      }
      catch (boost::math::rounding_error&)
      {
        return false;
      }
    }

    ORTHANC_FORCE_INLINE
    long long llround(long long& target,
                      double source)
    {
      try
      {
        target = boost::math::llround<double>(source);
        return true;
      }
      catch (boost::math::rounding_error&)
      {
        return false;
      }
    }
  }
}

#endif
