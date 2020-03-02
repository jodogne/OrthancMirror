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


#include "../PrecompiledHeadersServer.h"
#include "OrthancRestApi.h"

#include "../../Core/Compression/GzipCompressor.h"
#include "../../Core/Logging.h"
#include "../../Core/MetricsRegistry.h"
#include "../../Core/SerializationToolbox.h"
#include "../ServerContext.h"

#include <boost/algorithm/string/predicate.hpp>

namespace Orthanc
{
  static void SetupResourceAnswer(Json::Value& result,
                                  const std::string& publicId,
                                  ResourceType resourceType,
                                  StoreStatus status)
  {
    result = Json::objectValue;

    if (status != StoreStatus_Failure)
    {
      result["ID"] = publicId;
      result["Path"] = GetBasePath(resourceType, publicId);
    }
    
    result["Status"] = EnumerationToString(status);
  }


  void OrthancRestApi::AnswerStoredInstance(RestApiPostCall& call,
                                            DicomInstanceToStore& instance,
                                            StoreStatus status) const
  {
    Json::Value result;
    SetupResourceAnswer(result, instance.GetHasher().HashInstance(), 
                        ResourceType_Instance, status);

    result["ParentPatient"] = instance.GetHasher().HashPatient();
    result["ParentStudy"] = instance.GetHasher().HashStudy();
    result["ParentSeries"] = instance.GetHasher().HashSeries();

    call.GetOutput().AnswerJson(result);
  }


  void OrthancRestApi::AnswerStoredResource(RestApiPostCall& call,
                                            const std::string& publicId,
                                            ResourceType resourceType,
                                            StoreStatus status) const
  {
    Json::Value result;
    SetupResourceAnswer(result, publicId, resourceType, status);
    call.GetOutput().AnswerJson(result);
  }


  void OrthancRestApi::ResetOrthanc(RestApiPostCall& call)
  {
    OrthancRestApi::GetApi(call).leaveBarrier_ = true;
    OrthancRestApi::GetApi(call).resetRequestReceived_ = true;
    call.GetOutput().AnswerBuffer("{}", MimeType_Json);
  }


  void OrthancRestApi::ShutdownOrthanc(RestApiPostCall& call)
  {
    OrthancRestApi::GetApi(call).leaveBarrier_ = true;
    call.GetOutput().AnswerBuffer("{}", MimeType_Json);
    LOG(WARNING) << "Shutdown request received";
  }





  // Upload of DICOM files through HTTP ---------------------------------------

  static void UploadDicomFile(RestApiPostCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    LOG(INFO) << "Receiving a DICOM file of " << call.GetBodySize() << " bytes through HTTP";

    if (call.GetBodySize() == 0)
    {
      throw OrthancException(ErrorCode_BadFileFormat,
                             "Received an empty DICOM file");
    }

    std::string dicom;

    if (boost::iequals(call.GetHttpHeader("content-encoding", ""), "gzip"))
    {
      GzipCompressor compressor;
      compressor.Uncompress(dicom, call.GetBodyData(), call.GetBodySize());
    }
    else
    {
      // TODO Remove unneccessary memcpy
      call.BodyToString(dicom);
    }

    DicomInstanceToStore toStore;
    toStore.SetOrigin(DicomInstanceOrigin::FromRest(call));
    toStore.SetBuffer(dicom);

    std::string publicId;
    StoreStatus status = context.Store(publicId, toStore);

    OrthancRestApi::GetApi(call).AnswerStoredInstance(call, toStore, status);
  }



  // Registration of the various REST handlers --------------------------------

  OrthancRestApi::OrthancRestApi(ServerContext& context) : 
    context_(context),
    leaveBarrier_(false),
    resetRequestReceived_(false),
    activeRequests_(context.GetMetricsRegistry(), 
                    "orthanc_rest_api_active_requests", 
                    MetricsType_MaxOver10Seconds)
  {
    RegisterSystem();

    RegisterChanges();
    RegisterResources();
    RegisterModalities();
    RegisterAnonymizeModify();
    RegisterArchive();

    Register("/instances", UploadDicomFile);

    // Auto-generated directories
    Register("/tools", RestApi::AutoListChildren);
    Register("/tools/reset", ResetOrthanc);
    Register("/tools/shutdown", ShutdownOrthanc);
    Register("/instances/{id}/frames/{frame}", RestApi::AutoListChildren);
  }


  bool OrthancRestApi::Handle(HttpOutput& output,
                              RequestOrigin origin,
                              const char* remoteIp,
                              const char* username,
                              HttpMethod method,
                              const UriComponents& uri,
                              const Arguments& headers,
                              const GetArguments& getArguments,
                              const void* bodyData,
                              size_t bodySize)
  {
    MetricsRegistry::Timer timer(context_.GetMetricsRegistry(), "orthanc_rest_api_duration_ms");
    MetricsRegistry::ActiveCounter counter(activeRequests_);

    return RestApi::Handle(output, origin, remoteIp, username, method,
                           uri, headers, getArguments, bodyData, bodySize);
  }


  ServerContext& OrthancRestApi::GetContext(RestApiCall& call)
  {
    return GetApi(call).context_;
  }


  ServerIndex& OrthancRestApi::GetIndex(RestApiCall& call)
  {
    return GetContext(call).GetIndex();
  }



  static const char* KEY_PERMISSIVE = "Permissive";
  static const char* KEY_PRIORITY = "Priority";
  static const char* KEY_SYNCHRONOUS = "Synchronous";
  static const char* KEY_ASYNCHRONOUS = "Asynchronous";

  
  bool OrthancRestApi::IsSynchronousJobRequest(bool isDefaultSynchronous,
                                               const Json::Value& body)
  {
    if (body.type() != Json::objectValue)
    {
      return isDefaultSynchronous;
    }
    else if (body.isMember(KEY_SYNCHRONOUS))
    {
      return SerializationToolbox::ReadBoolean(body, KEY_SYNCHRONOUS);
    }
    else if (body.isMember(KEY_ASYNCHRONOUS))
    {
      return !SerializationToolbox::ReadBoolean(body, KEY_ASYNCHRONOUS);
    }
    else
    {
      return isDefaultSynchronous;
    }
  }

  
  unsigned int OrthancRestApi::GetJobRequestPriority(const Json::Value& body)
  {
    if (body.type() != Json::objectValue ||
        !body.isMember(KEY_PRIORITY))
    {
      return 0;   // Default priority
    }
    else 
    {
      return SerializationToolbox::ReadInteger(body, KEY_PRIORITY);
    }
  }
  

  void OrthancRestApi::SubmitGenericJob(RestApiOutput& output,
                                        ServerContext& context,
                                        IJob* job,
                                        bool synchronous,
                                        int priority)
  {
    std::unique_ptr<IJob> raii(job);
    
    if (job == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }

    if (synchronous)
    {
      Json::Value successContent;
      context.GetJobsEngine().GetRegistry().SubmitAndWait
        (successContent, raii.release(), priority);

      // Success in synchronous execution
      output.AnswerJson(successContent);
    }
    else
    {
      // Asynchronous mode: Submit the job, but don't wait for its completion
      std::string id;
      context.GetJobsEngine().GetRegistry().Submit
        (id, raii.release(), priority);

      Json::Value v;
      v["ID"] = id;
      v["Path"] = "/jobs/" + id;
      output.AnswerJson(v);
    }
  }

  
  void OrthancRestApi::SubmitGenericJob(RestApiPostCall& call,
                                        IJob* job,
                                        bool isDefaultSynchronous,
                                        const Json::Value& body) const
  {
    std::unique_ptr<IJob> raii(job);

    if (body.type() != Json::objectValue)
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    bool synchronous = IsSynchronousJobRequest(isDefaultSynchronous, body);
    int priority = GetJobRequestPriority(body);

    SubmitGenericJob(call.GetOutput(), context_, raii.release(), synchronous, priority);
  }

  
  void OrthancRestApi::SubmitCommandsJob(RestApiPostCall& call,
                                         SetOfCommandsJob* job,
                                         bool isDefaultSynchronous,
                                         const Json::Value& body) const
  {
    std::unique_ptr<SetOfCommandsJob> raii(job);
    
    if (body.type() != Json::objectValue)
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    job->SetDescription("REST API");
    
    if (body.isMember(KEY_PERMISSIVE))
    {
      job->SetPermissive(SerializationToolbox::ReadBoolean(body, KEY_PERMISSIVE));
    }
    else
    {
      job->SetPermissive(false);
    }

    SubmitGenericJob(call, raii.release(), isDefaultSynchronous, body);
  }
}
