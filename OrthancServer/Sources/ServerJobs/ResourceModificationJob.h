/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2021-2023 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

#include "../../../OrthancFramework/Sources/DicomParsing/DicomModification.h"
#include "../../../OrthancFramework/Sources/MultiThreading/RunnableWorkersPool.h"
#include "../DicomInstanceOrigin.h"
#include "ThreadedSetOfInstancesJob.h"
#include <boost/thread/recursive_mutex.hpp>

namespace Orthanc
{
  class ServerContext;
  
  class ResourceModificationJob : public ThreadedSetOfInstancesJob
  {
  private:
    class IOutput : public boost::noncopyable
    {
    public:
      virtual ~IOutput()
      {
      }

      virtual void Update(DicomInstanceHasher& hasher) = 0;

      virtual void Format(Json::Value& target) const = 0;

      virtual bool IsSingleResource() const = 0;
    };
    
    class SingleOutput;
    class MultipleOutputs;

    mutable boost::recursive_mutex      outputMutex_;
    
    std::unique_ptr<DicomModification>  modification_;
    boost::shared_ptr<IOutput>          output_;
    bool                                isAnonymization_;
    DicomInstanceOrigin                 origin_;
    bool                                transcode_;
    DicomTransferSyntax                 transferSyntax_;
    std::set<std::string>               modifiedSeries_;          // the list of new series ids of the newly generated series
    std::set<std::string>               instancesToReconstruct_;  // for each new series generated, an instance id that we can use to reconstruct the hierarchy DB model

  protected:
    virtual bool HandleInstance(const std::string& instance) ORTHANC_OVERRIDE;
    
    virtual void PostProcessInstances() ORTHANC_OVERRIDE;

  public:
    explicit ResourceModificationJob(ServerContext& context, unsigned int workersCount);

    ResourceModificationJob(ServerContext& context,
                            const Json::Value& serialized);

    // NB: The "outputLevel" only controls the output format, and
    // might *not* be the same as "modification->GetLevel()"
    void SetSingleResourceModification(DicomModification* modification,   // Takes ownership
                                       ResourceType outputLevel,
                                       bool isAnonymization);

    void SetMultipleResourcesModification(DicomModification* modification,   // Takes ownership
                                          bool isAnonymization);

    void SetOrigin(const DicomInstanceOrigin& origin);

    void SetOrigin(const RestApiCall& call);

    bool IsAnonymization() const
    {
      return isAnonymization_;
    }

    const DicomInstanceOrigin& GetOrigin() const
    {
      return origin_;
    }

    bool IsTranscode() const
    {
      return transcode_;
    }

    DicomTransferSyntax GetTransferSyntax() const;

    void SetTranscode(DicomTransferSyntax syntax);

    void SetTranscode(const std::string& transferSyntaxUid);

    void ClearTranscode();

    bool IsSingleResourceModification() const;

    // Only possible if "IsSingleResourceModification()"
    ResourceType GetOutputLevel() const;

    virtual void GetJobType(std::string& target) ORTHANC_OVERRIDE
    {
      target = "ResourceModification";
    }

    virtual void GetPublicContent(Json::Value& value) ORTHANC_OVERRIDE;
    
    virtual bool Serialize(Json::Value& value) ORTHANC_OVERRIDE;

    virtual void Reset() ORTHANC_OVERRIDE;

    void PerformSanityChecks();

#if ORTHANC_BUILD_UNIT_TESTS == 1
    const DicomModification& GetModification() const;
#endif
  };
}
