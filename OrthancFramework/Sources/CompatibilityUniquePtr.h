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

#include "Compatibility.h"

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
#  include <memory>
#endif
