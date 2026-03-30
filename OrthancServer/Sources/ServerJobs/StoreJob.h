/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2026 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2026 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
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

#include "../../../OrthancFramework/Sources/Compatibility.h"
#include "../../../OrthancFramework/Sources/JobsEngine/SetOfInstancesJob.h"
#include "../../../OrthancFramework/Sources/FileStorage/FileInfo.h"
#include "ThreadedInstancesLoader.h"

#include <vector>

namespace Orthanc
{
  class ServerContext;
  
  class ORTHANC_PUBLIC StoreJob : public SetOfInstancesJob
  {
  protected:
    ServerContext&                             context_;
    std::unique_ptr<ThreadedInstancesLoader>   instancesLoader_;
    // TODO: re-create when unserializing (in AddParentResources ?)
    std::vector<std::string>                   instancesIds_;
    std::vector<FileInfo>                      filesInfo_;

  public:
    explicit StoreJob(ServerContext& context);

    StoreJob(ServerContext& context,
             const Json::Value& serialized);

    virtual void Start() ORTHANC_OVERRIDE;

    virtual void Stop(JobStopReason reason) ORTHANC_OVERRIDE;

    void AddInstances(const std::vector<std::string>& instancesIds,
                      const std::vector<FileInfo>& filesInfo);

    virtual const char* GetLoaderPrefix() const = 0;
  };
}
