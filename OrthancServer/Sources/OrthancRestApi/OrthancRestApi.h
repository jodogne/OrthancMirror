/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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
#include "../../../OrthancFramework/Sources/JobsEngine/SetOfCommandsJob.h"
#include "../../../OrthancFramework/Sources/MetricsRegistry.h"
#include "../../../OrthancFramework/Sources/RestApi/RestApi.h"
#include "../ServerEnumerations.h"

#include <set>

namespace Orthanc
{
  class ServerContext;
  class ServerIndex;
  class DicomInstanceToStore;

  class OrthancRestApi : public RestApi
  {
  public:
    typedef std::set<std::string> SetOfStrings;

  private:
    ServerContext&                  context_;
    bool                            leaveBarrier_;
    bool                            resetRequestReceived_;
    MetricsRegistry::SharedMetrics  activeRequests_;

    void RegisterSystem(bool orthancExplorerEnabled);

    void RegisterChanges();

    void RegisterResources();

    void RegisterModalities();

    void RegisterAnonymizeModify();

    void RegisterArchive();

    static void ResetOrthanc(RestApiPostCall& call);

    static void ShutdownOrthanc(RestApiPostCall& call);

  public:
    explicit OrthancRestApi(ServerContext& context,
                            bool orthancExplorerEnabled);

    virtual bool Handle(HttpOutput& output,
                        RequestOrigin origin,
                        const char* remoteIp,
                        const char* username,
                        HttpMethod method,
                        const UriComponents& uri,
                        const HttpToolbox::Arguments& headers,
                        const HttpToolbox::GetArguments& getArguments,
                        const void* bodyData,
                        size_t bodySize) ORTHANC_OVERRIDE;

    const bool& LeaveBarrierFlag() const
    {
      return leaveBarrier_;
    }

    bool IsResetRequestReceived() const
    {
      return resetRequestReceived_;
    }

    static OrthancRestApi& GetApi(RestApiCall& call)
    {
      return dynamic_cast<OrthancRestApi&>(call.GetContext());
    }

    static ServerContext& GetContext(RestApiCall& call);

    static ServerIndex& GetIndex(RestApiCall& call);

    // WARNING: "instanceId" can be different from
    // "instance.GetHasher().HashInstance()" if transcoding is enabled
    void AnswerStoredInstance(RestApiPostCall& call,
                              DicomInstanceToStore& instance,
                              StoreStatus status,
                              const std::string& instanceId) const;

    void AnswerStoredResource(RestApiPostCall& call,
                              const std::string& publicId,
                              ResourceType resourceType,
                              StoreStatus status) const;

    static bool IsSynchronousJobRequest(bool isDefaultSynchronous,
                                        const Json::Value& body);
    
    static unsigned int GetJobRequestPriority(const Json::Value& body);
    
    static void SubmitGenericJob(RestApiOutput& output,
                                 ServerContext& context,
                                 IJob* job,
                                 bool synchronous,
                                 int priority);
    
    void SubmitGenericJob(RestApiPostCall& call,
                          IJob* job,
                          bool isDefaultSynchronous,
                          const Json::Value& body) const;

    void SubmitCommandsJob(RestApiPostCall& call,
                           SetOfCommandsJob* job,
                           bool isDefaultSynchronous,
                           const Json::Value& body) const;

    static void DocumentSubmitGenericJob(RestApiPostCall& call);

    static void DocumentSubmitCommandsJob(RestApiPostCall& call);

    static DicomToJsonFormat GetDicomFormat(const RestApiGetCall& call,
                                            DicomToJsonFormat defaultFormat);

    static DicomToJsonFormat GetDicomFormat(const Json::Value& body,
                                            DicomToJsonFormat defaultFormat);

    static void DocumentDicomFormat(RestApiGetCall& call,
                                    DicomToJsonFormat defaultFormat);

    static void DocumentDicomFormat(RestApiPostCall& call,
                                    DicomToJsonFormat defaultFormat);

    static void GetRequestedTags(std::set<DicomTag>& requestedTags,
                                 const RestApiGetCall& call);

    static void DocumentRequestedTags(RestApiGetCall& call);
  };
}
