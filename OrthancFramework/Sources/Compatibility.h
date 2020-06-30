/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
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


#pragma once

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

#  if _MSC_VER >= 1600
#    define ORTHANC_Cxx03_DETECTED 0
#  else
#    define ORTHANC_Cxx03_DETECTED 1
#  endif

#else
// of _MSC_VER is not defined, we assume __cplusplus is correctly defined
// if __cplusplus is not defined (very old compilers??), then the following
// test will compare 0 < 201103L and will be true --> safe.
#  if __cplusplus < 201103L
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
