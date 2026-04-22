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


#include "../PrecompiledHeadersServer.h"
#include "StoreJob.h"

#include <algorithm>

#include "../../../OrthancFramework/Sources/Compatibility.h"
#include "../../../OrthancFramework/Sources/Logging.h"
#include "../../../OrthancFramework/Sources/SerializationToolbox.h"
#include "../ServerContext.h"
#include "../OrthancConfiguration.h"


namespace Orthanc
{
  StoreJob::StoreJob(ServerContext& context) :
    context_(context)
  {
  }


  StoreJob::StoreJob(ServerContext& context,
                     const Json::Value& serialized) :
    SetOfInstancesJob(serialized),
    context_(context)
  {
    // we need to rebuild the instancesIds_ and filesInfo_ from the SetOfInstancesJob
    GetInstancesIds(instancesIds_);

    for (size_t i = 0; i < instancesIds_.size(); ++i)
    {
      FileInfo fileInfo;
      int64_t revisionNotUsed;
      if (!context.GetIndex().LookupAttachment(fileInfo, revisionNotUsed, ResourceType_Instance, instancesIds_[i], FileContentType_Dicom))
      {
        throw OrthancException(ErrorCode_UnknownResource, std::string("Error while unserializing a job, unable to find DICOM attachment for instance ") + instancesIds_[i]);
      }

      filesInfo_.push_back(fileInfo);
    }
  }


  void StoreJob::AddInstances(const std::vector<std::string>& instancesIds,
                              const std::vector<FileInfo>& filesInfo)
  {
    assert(instancesIds.size() == filesInfo.size());

    instancesIds_.reserve(instancesIds_.size() + instancesIds.size());
    filesInfo_.reserve(filesInfo.size() + filesInfo.size());

    std::copy(instancesIds.begin(), instancesIds.end(), std::back_inserter(instancesIds_));
    std::copy(filesInfo.begin(), filesInfo.end(), std::back_inserter(filesInfo_));

    // create the commands (they must be in the same order as the instancesIds_ so that the instancesLoader loads them in the right consume order)
    for (size_t i = 0; i < instancesIds.size(); ++i)
    {
      AddInstance(instancesIds[i]);
    }
  }


  void StoreJob::StartLoaderThreads()
  {
    size_t loaderThreads = 1;
    {
      OrthancConfiguration::ReaderLock lock;
      loaderThreads = lock.GetConfiguration().GetLoaderThreads();
    }

    instancesLoader_.reset(new ThreadedInstancesLoader(context_, loaderThreads, false, DicomTransferSyntax_LittleEndianImplicit /* dummy value not used*/, 0, GetLoaderPrefix()));

    size_t position = GetPosition(); // in case we reload a job in progress, we shall not preload the first instances
    for (size_t i = position; i < instancesIds_.size(); ++i)
    {
      instancesLoader_->PreloadDicomInstance(instancesIds_[i], filesInfo_[i]);
    }
  }

  
  void StoreJob::Stop(JobStopReason reason)
  {
    // clear the loader threads (also when simply pausing the job)
    instancesLoader_.reset(NULL);
  }
}
