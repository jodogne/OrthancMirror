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


#pragma once

#include "../../OrthancFramework.h"

#include <json/value.h>
#include <boost/noncopyable.hpp>

namespace Orthanc
{
  class ORTHANC_PUBLIC IJobOperationValue : public boost::noncopyable
  {
  public:
    enum Type
    {
      Type_DicomInstance,
      Type_Null,
      Type_String
    };

    virtual ~IJobOperationValue()
    {
    }

    virtual Type GetType() const = 0;

    virtual IJobOperationValue* Clone() const = 0;

    virtual void Serialize(Json::Value& target) const = 0;
  };
}
