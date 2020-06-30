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


#include "../../PrecompiledHeaders.h"
#include "JobOperationValues.h"

#include "../IJobUnserializer.h"
#include "../../OrthancException.h"

#include <cassert>
#include <memory>

namespace Orthanc
{
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


  void JobOperationValues::Append(JobOperationValue* value)  // Takes ownership
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


  JobOperationValue& JobOperationValues::GetValue(size_t index) const
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
