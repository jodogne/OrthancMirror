/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
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


// Macro "ORTHANC_FORCE_INLINE" forces a function/method to be inlined
#if defined(_MSC_VER)
#  define ORTHANC_FORCE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__) || defined(__EMSCRIPTEN__)
#  define ORTHANC_FORCE_INLINE inline __attribute((always_inline))
#else
#  error Please support your compiler here
#endif


// Macro "ORTHANC_DEPRECATED" tags a function as having been deprecated
#if (__cplusplus >= 201402L)  // C++14
#  define ORTHANC_DEPRECATED(f) [[deprecated]] f
#elif defined(__GNUC__) || defined(__clang__)
#  define ORTHANC_DEPRECATED(f) f __attribute__((deprecated))
#elif defined(_MSC_VER)
#  define ORTHANC_DEPRECATED(f) __declspec(deprecated) f
#else
#  define ORTHANC_DEPRECATED
#endif


// Macros "ORTHANC_OVERRIDE" and "ORTHANC_FINAL" wrap the "override"
// and "final" keywords introduced in C++11, to do compile-time
// checking of virtual methods
// The __cplusplus macro is broken in Visual Studio up to 15.6 and, in
// later versions, require the usage of the /Zc:__cplusplus flag
// We thus use an alternate way of checking for 'override' support
#ifdef ORTHANC_OVERRIDE_SUPPORTED
#  error ORTHANC_OVERRIDE_SUPPORTED cannot be defined at this point
#endif 

#if __cplusplus >= 201103L   // C++11
#  define ORTHANC_OVERRIDE_SUPPORTED 1
#else
#  ifdef _MSC_VER
#    if _MSC_VER >= 1600  // Visual Studio 2010 (10.0)
#      define ORTHANC_OVERRIDE_SUPPORTED 1
#    endif
#  endif
#endif


#if ORTHANC_OVERRIDE_SUPPORTED
// The override keyword (C++11) is enabled
#  define ORTHANC_OVERRIDE  override 
#  define ORTHANC_FINAL     final
#else
// The override keyword (C++11) is not available
#  define ORTHANC_OVERRIDE
#  define ORTHANC_FINAL
#endif



//#define Orthanc_Compatibility_h_STR2(x) #x
//#define Orthanc_Compatibility_h_STR1(x) Orthanc_Compatibility_h_STR2(x)

//#pragma message("__cplusplus = " Orthanc_Compatibility_h_STR1(__cplusplus))

#if (defined _MSC_VER)
//#  pragma message("_MSC_VER = " Orthanc_Compatibility_h_STR1(_MSC_VER))
//#  pragma message("_MSVC_LANG = " Orthanc_Compatibility_h_STR1(_MSVC_LANG))
// The __cplusplus macro cannot be used in Visual C++ < 1914 (VC++ 15.7)
// However, even in recent versions, __cplusplus will only be correct (that is,
// correctly defines the supported C++ version) if a special flag is passed to
// the compiler ("/Zc:__cplusplus")
// To make this header more robust, we use the _MSVC_LANG equivalent macro.

// please note that not all C++11 features are supported when _MSC_VER == 1600
// (or higher). This header file can be made for fine-grained, if required, 
// based on specific _MSC_VER values

#  if _MSC_VER >= 1600  // Visual Studio 2010 (10.0)
#    define ORTHANC_Cxx03_DETECTED 0
#  else
#    define ORTHANC_Cxx03_DETECTED 1
#  endif

#else
// of _MSC_VER is not defined, we assume __cplusplus is correctly defined
// if __cplusplus is not defined (very old compilers??), then the following
// test will compare 0 < 201103L and will be true --> safe.
#  if __cplusplus < 201103L  // C++11
#    define ORTHANC_Cxx03_DETECTED 1
#  else
#    define ORTHANC_Cxx03_DETECTED 0
#  endif
#endif

#if ORTHANC_Cxx03_DETECTED == 1
//#pragma message("C++ 11 support is not present.")

/**
 * "std::unique_ptr" was introduced in C++11, and "std::auto_ptr" was
 * removed in C++17. We emulate "std::auto_ptr" using boost: "The
 * smart pointer unique_ptr [is] a drop-in replacement for
 * std::unique_ptr, usable also from C++03 compilers." This is only
 * available if Boost >= 1.57.0 (from November 2014).
 * https://www.boost.org/doc/libs/1_57_0/doc/html/move/reference.html#header.boost.move.unique_ptr_hpp
 **/

#include <boost/move/unique_ptr.hpp>

namespace std
{
  template <typename T>
  class unique_ptr : public boost::movelib::unique_ptr<T>
  {
  public:
    explicit unique_ptr() :
      boost::movelib::unique_ptr<T>()
    {
    }      

    explicit unique_ptr(T* p) :
      boost::movelib::unique_ptr<T>(p)
    {
    }      
  };
}
#else
//# pragma message("C++ 11 support is present.")
# include <memory>
#endif
