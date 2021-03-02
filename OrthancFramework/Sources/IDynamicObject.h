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

#include "OrthancFramework.h"

#include <boost/noncopyable.hpp>

namespace Orthanc
{
  /**
   * This class should be the ancestor to any class whose type is
   * determined at the runtime, and that can be dynamically allocated.
   * Being a child of IDynamicObject only implies the existence of a
   * virtual destructor.
   **/
  class ORTHANC_PUBLIC IDynamicObject : public boost::noncopyable
  {
  public:
    virtual ~IDynamicObject()
    {
    }
  };
  

  /**
   * This class is a simple implementation of a IDynamicObject that
   * stores a single typed value.
   */
  template <typename T>
  class SingleValueObject : public IDynamicObject
  {
  private:
    T  value_;
    
  public:
    explicit SingleValueObject(const T& value) :
      value_(value)
    {
    }

    const T& GetValue() const
    {
      return value_;
    }
  };
}
