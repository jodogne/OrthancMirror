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


#include "../../PrecompiledHeaders.h"
#include "NullOperationValue.h"


namespace Orthanc
{
  IJobOperationValue::Type NullOperationValue::GetType() const
  {
    return Type_Null;
  }
    
  IJobOperationValue* NullOperationValue::Clone() const 
  {
    return new NullOperationValue;
  }

  void NullOperationValue::Serialize(Json::Value& target) const 
  {
    target = Json::objectValue;
    target["Type"] = "Null";
  }
}
