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
#include "LogJobOperation.h"

#include "../../Logging.h"
#include "StringOperationValue.h"

namespace Orthanc
{
  void LogJobOperation::Apply(JobOperationValues& outputs,
                              const IJobOperationValue& input)
  {
    switch (input.GetType())
    {
      case IJobOperationValue::Type_String:
        LOG(INFO) << "Job value: "
                  << dynamic_cast<const StringOperationValue&>(input).GetContent();
        break;

      case IJobOperationValue::Type_Null:
        LOG(INFO) << "Job value: (null)";
        break;

      default:
        LOG(INFO) << "Job value: (unsupport)";
        break;
    }

    outputs.Append(input.Clone());
  }

  void LogJobOperation::Serialize(Json::Value &result) const
  {
    result = Json::objectValue;
    result["Type"] = "Log";
  }
}
