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

#if __cplusplus < 201103L
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
    unique_ptr() :
      boost::movelib::unique_ptr<T>()
    {
    }      

    unique_ptr(T* p) :
      boost::movelib::unique_ptr<T>(p)
    {
    }      
  };
}

#endif
