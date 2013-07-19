/**
 * Laaw - Lightweight, Automated API Wrapper
 * Copyright (C) 2010-2013 Jomago - Alain Mazy, Benjamin Golinvaux,
 * Sebastien Jodogne
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

#include "laaw-exports.h"
#include <stddef.h>
#include <string>

#if (LAAW_PARSING == 1)

#define LAAW_API   __attribute__((deprecated("")))
#define LAAW_API_INTERNAL  __attribute__((deprecated("")))
#define LAAW_API_OVERLOAD(name)  __attribute__((deprecated("")))
#define LAAW_API_PROPERTY  __attribute__((deprecated("")))
#define LAAW_API_STATIC_CLASS  __attribute__((deprecated("")))
#define LAAW_API_CUSTOM(name, value)  __attribute__((deprecated("")))

#else

#define LAAW_API
#define LAAW_API_INTERNAL
#define LAAW_API_OVERLOAD(name)
#define LAAW_API_PROPERTY
#define LAAW_API_STATIC_CLASS
#define LAAW_API_CUSTOM(name, value)

#endif


namespace Laaw
{
  /**
   * This is the base class from which all the public exceptions in
   * the SDK should derive.
   **/
  class LaawException
  {
  private:
    std::string what_;

  public:
    LaawException()
    {
    }

    LaawException(const std::string& what) : what_(what)
    {
    }

    LaawException(const char* what) : what_(what)
    {
    }

    virtual const char* What() const
    {
      return what_.c_str();
    }
  };
}
