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
#include "JobOperationValues.h"

#include "../IJobUnserializer.h"
#include "../../OrthancException.h"

#include <cassert>
#include <memory>

namespace Orthanc
{
#if ORTHANC_BUILDING_FRAMEWORK_LIBRARY == 1
  void JobOperationValues::Append(JobOperationValue* value)
  {
    throw OrthancException(ErrorCode_DiscontinuedAbi, "Removed in 1.8.1");
  }
#endif
    

  void JobOperationValues::Append(JobOperationValues& target,
                                  bool clear)
  {
    target.Reserve(target.GetSize() + GetSize());

    for (size_t i = 0; i < values_.size(); i++)
    {
      if (clear)
      {
        target.Append(values_[i]);
        values_[i] = NULL;
      }
      else
      {
        target.Append(GetValue(i).Clone());
      }
    }

    if (clear)
    {
      Clear();
    }
  }

  JobOperationValues::~JobOperationValues()
  {
    Clear();
  }

  void JobOperationValues::Move(JobOperationValues &target)
  {
    return Append(target, true);
  }

  void JobOperationValues::Copy(JobOperationValues &target)
  {
    return Append(target, false);
  }


  void JobOperationValues::Clear()
  {
    for (size_t i = 0; i < values_.size(); i++)
    {
      if (values_[i] != NULL)
      {
        delete values_[i];
      }
    }

    values_.clear();
  }

  void JobOperationValues::Reserve(size_t count)
  {
    values_.reserve(count);
  }


  void JobOperationValues::Append(IJobOperationValue* value)  // Takes ownership
  {
    if (value == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
    else
    {
      values_.push_back(value);
    }
  }

  size_t JobOperationValues::GetSize() const
  {
    return values_.size();
  }


  IJobOperationValue& JobOperationValues::GetValue(size_t index) const
  {
    if (index >= values_.size())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
    else
    {
      assert(values_[index] != NULL);
      return *values_[index];
    }
  }


  void JobOperationValues::Serialize(Json::Value& target) const
  {
    target = Json::arrayValue;

    for (size_t i = 0; i < values_.size(); i++)
    {
      Json::Value tmp;
      values_[i]->Serialize(tmp);
      target.append(tmp);
    }
  }


  JobOperationValues* JobOperationValues::Unserialize(IJobUnserializer& unserializer,
                                                      const Json::Value& source)
  {
    if (source.type() != Json::arrayValue)
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    std::unique_ptr<JobOperationValues> result(new JobOperationValues);

    result->Reserve(source.size());
    
    for (Json::Value::ArrayIndex i = 0; i < source.size(); i++)
    {
      result->Append(unserializer.UnserializeValue(source[i]));
    }
    
    return result.release();
  }
}
