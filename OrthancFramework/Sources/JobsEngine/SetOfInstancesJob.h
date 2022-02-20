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

#include "IJob.h"
#include "SetOfCommandsJob.h"

#include <set>

namespace Orthanc
{
  class ORTHANC_PUBLIC SetOfInstancesJob : public SetOfCommandsJob
  {
  private:
    class InstanceCommand;
    class TrailingStepCommand;
    class InstanceUnserializer;
    
    bool                   hasTrailingStep_;
    std::set<std::string>  failedInstances_;
    std::set<std::string>  parentResources_;

  protected:
    virtual bool HandleInstance(const std::string& instance) = 0;

    virtual bool HandleTrailingStep() = 0;

    // Hiding this method, use AddInstance() instead
    using SetOfCommandsJob::AddCommand;

  public:
    SetOfInstancesJob();

    explicit SetOfInstancesJob(const Json::Value& source);  // Unserialization

    // Only used for reporting in the public content
    // https://groups.google.com/d/msg/orthanc-users/9GCV88GLEzw/6wAgP_PRAgAJ
    void AddParentResource(const std::string& resource);
    
    void AddInstance(const std::string& instance);

    void AddTrailingStep(); 

    size_t GetInstancesCount() const;
    
    const std::string& GetInstance(size_t index) const;

    bool HasTrailingStep() const;

    const std::set<std::string>& GetFailedInstances() const;

    bool IsFailedInstance(const std::string& instance) const;

    virtual void Start() ORTHANC_OVERRIDE;

    virtual void Reset() ORTHANC_OVERRIDE;

    virtual void GetPublicContent(Json::Value& target) ORTHANC_OVERRIDE;

    virtual bool Serialize(Json::Value& target) ORTHANC_OVERRIDE;
  };
}
