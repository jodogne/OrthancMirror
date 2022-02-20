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

#include "IJobOperationValue.h"
#include "../../Compatibility.h"

#include <vector>

namespace Orthanc
{
  class IJobUnserializer;

#if ORTHANC_BUILDING_FRAMEWORK_LIBRARY == 1
  class JobOperationValue
  {
    /**
     * This is for ABI compatibility with Orthanc framework <= 1.8.0,
     * only to be able to run unit tests from Orthanc 1.7.2 to
     * 1.8.0. The class was moved to "IJobOperationValue" in 1.8.1,
     * and its memory layout has changed. Don't use this anymore.
     **/
  };
#endif

  class ORTHANC_PUBLIC JobOperationValues : public boost::noncopyable
  {
  private:
    std::vector<IJobOperationValue*>   values_;

#if ORTHANC_BUILDING_FRAMEWORK_LIBRARY == 1
    // For binary compatibility with Orthanc <= 1.8.0
    ORTHANC_DEPRECATED(void Append(JobOperationValue* value));
#endif
    
    void Append(JobOperationValues& target,
                bool clear);

  public:
    ~JobOperationValues();

    void Move(JobOperationValues& target);

    void Copy(JobOperationValues& target);

    void Clear();

    void Reserve(size_t count);

    void Append(IJobOperationValue* value);  // Takes ownership

    size_t GetSize() const;

    IJobOperationValue& GetValue(size_t index) const;

    void Serialize(Json::Value& target) const;

    static JobOperationValues* Unserialize(IJobUnserializer& unserializer,
                                           const Json::Value& source);
  };
}
