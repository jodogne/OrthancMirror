/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
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

#include "../../../OrthancFramework/Sources/Compression/GzipCompressor.h"
#include "../../../OrthancFramework/Sources/Compression/ZipReader.h"
#include "../../../OrthancFramework/Sources/Logging.h"
#include "../../../OrthancFramework/Sources/MetricsRegistry.h"
#include "../../../OrthancFramework/Sources/SerializationToolbox.h"
#include "../OrthancConfiguration.h"
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


  static void SetupResourceAnswer(Json::Value& result,
                                  const DicomInstanceToStore& instance,
                                  StoreStatus status,
                                  const std::string& instanceId)
  {
    SetupResourceAnswer(result, instanceId, ResourceType_Instance, status);

    DicomMap summary;
    instance.GetSummary(summary);

    DicomInstanceHasher hasher(summary);
    result["ParentPatient"] = hasher.HashPatient();
    result["ParentStudy"] = hasher.HashStudy();
    result["ParentSeries"] = hasher.HashSeries();
  }


  void OrthancRestApi::AnswerStoredInstance(RestApiPostCall& call,
                                            DicomInstanceToStore& instance,
                                            StoreStatus status,
                                            const std::string& instanceId) const
  {
    Json::Value result;
    SetupResourceAnswer(result, instance, status, instanceId);
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
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("System")
        .SetSummary("Restart Orthanc");
      return;
    }

    OrthancRestApi::GetApi(call).leaveBarrier_ = true;
    OrthancRestApi::GetApi(call).resetRequestReceived_ = true;
    call.GetOutput().AnswerBuffer("{}", MimeType_Json);
  }


  void OrthancRestApi::ShutdownOrthanc(RestApiPostCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("System")
        .SetSummary("Shutdown Orthanc");
      return;
    }

    OrthancRestApi::GetApi(call).leaveBarrier_ = true;
    call.GetOutput().AnswerBuffer("{}", MimeType_Json);
    LOG(WARNING) << "Shutdown request received";
  }





  // Upload of DICOM files through HTTP ---------------------------------------

  static void UploadDicomFile(RestApiPostCall& call)
  {
    if (call.GetRequestOrigin() == RequestOrigin_Documentation)
    {
      Json::Value sample = Json::objectValue;
      sample["ID"] = "19816330-cb02e1cf-df3a8fe8-bf510623-ccefe9f5";
      sample["ParentPatient"] = "ef9d77db-eb3b2bef-9b31fd3e-bf42ae46-dbdb0cc3";
      sample["ParentSeries"] = "3774320f-ccda46d8-69ee8641-9e791cbf-3ecbbcc6";
      sample["ParentStudy"] = "66c8e41e-ac3a9029-0b85e42a-8195ee0a-92c2e62e";
      sample["Path"] = "/instances/19816330-cb02e1cf-df3a8fe8-bf510623-ccefe9f5";
      sample["Status"] = "Success";
      
      call.GetDocumentation()
        .SetTag("Instances")
        .SetSummary("Upload DICOM instances")
        .AddRequestType(MimeType_Dicom, "DICOM file to be uploaded")
        .AddRequestType(MimeType_Zip, "ZIP archive containing DICOM files (new in Orthanc 1.8.2)")
        .AddAnswerType(MimeType_Json, "Information about the uploaded instance, "
                       "or list of information for each uploaded instance in the case of ZIP archive")
        .SetAnswerField("ID", RestApiCallDocumentation::Type_String, "Orthanc identifier of the new instance")
        .SetAnswerField("Path", RestApiCallDocumentation::Type_String, "Path to the new instance in the REST API")
        .SetAnswerField("Status", RestApiCallDocumentation::Type_String, "Can be `Success`, `AlreadyStored`, `Failure`, or `FilteredOut` (removed by some `NewInstanceFilter`)")
        .SetAnswerField("ParentPatient", RestApiCallDocumentation::Type_String, "Orthanc identifier of the parent patient")
        .SetAnswerField("ParentStudy", RestApiCallDocumentation::Type_String, "Orthanc identifier of the parent study")
        .SetAnswerField("ParentSeries", RestApiCallDocumentation::Type_String, "Orthanc identifier of the parent series")
        .SetSample(sample);
      return;
    }

    ServerContext& context = OrthancRestApi::GetContext(call);

    CLOG(INFO, HTTP) << "Receiving a DICOM file of " << call.GetBodySize() << " bytes through HTTP";

    if (call.GetBodySize() == 0)
    {
      throw OrthancException(ErrorCode_BadFileFormat,
                             "Received an empty DICOM file");
    }

    if (ZipReader::IsZipMemoryBuffer(call.GetBodyData(), call.GetBodySize()))
    {
      // New in Orthanc 1.8.2
      std::unique_ptr<ZipReader> reader(ZipReader::CreateFromMemory(call.GetBodyData(), call.GetBodySize()));

      Json::Value answer = Json::arrayValue;
      
      std::string filename, content;
      while (reader->ReadNextFile(filename, content))
      {
        if (!content.empty())
        {
          LOG(INFO) << "Uploading DICOM file from ZIP archive: " << filename;

          std::unique_ptr<DicomInstanceToStore> toStore(DicomInstanceToStore::CreateFromBuffer(content));
          toStore->SetOrigin(DicomInstanceOrigin::FromRest(call));

          std::string publicId;

          try
          {
            StoreStatus status = context.Store(publicId, *toStore, StoreInstanceMode_Default);

            Json::Value info;
            SetupResourceAnswer(info, *toStore, status, publicId);
            answer.append(info);
          }
          catch (OrthancException& e)
          {
            if (e.GetErrorCode() == ErrorCode_BadFileFormat)
            {
              LOG(ERROR) << "Cannot import non-DICOM file from ZIP archive: " << filename;
            }
            else
            {
              throw;
            }
          }
        }
      }      

      call.GetOutput().AnswerJson(answer);
    }
    else
    {
      // The lifetime of "dicom" must be longer than "toStore", as the
      // latter can possibly store a reference to the former (*)
      std::string dicom;

      std::unique_ptr<DicomInstanceToStore> toStore;

      if (boost::iequals(call.GetHttpHeader("content-encoding", ""), "gzip"))
      {
        GzipCompressor compressor;
        compressor.Uncompress(dicom, call.GetBodyData(), call.GetBodySize());
        toStore.reset(DicomInstanceToStore::CreateFromBuffer(dicom));  // (*)
      }
      else
      {
        toStore.reset(DicomInstanceToStore::CreateFromBuffer(call.GetBodyData(), call.GetBodySize()));
      }    

      toStore->SetOrigin(DicomInstanceOrigin::FromRest(call));

      std::string publicId;
      StoreStatus status = context.Store(publicId, *toStore, StoreInstanceMode_Default);

      OrthancRestApi::GetApi(call).AnswerStoredInstance(call, *toStore, status, publicId);
    }
  }



  // Registration of the various REST handlers --------------------------------

  OrthancRestApi::OrthancRestApi(ServerContext& context, 
                                 bool orthancExplorerEnabled) : 
    context_(context),
    leaveBarrier_(false),
    resetRequestReceived_(false),
    activeRequests_(context.GetMetricsRegistry(), 
                    "orthanc_rest_api_active_requests", 
                    MetricsType_MaxOver10Seconds)
  {
    RegisterSystem(orthancExplorerEnabled);

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
  }


  bool OrthancRestApi::Handle(HttpOutput& output,
                              RequestOrigin origin,
                              const char* remoteIp,
                              const char* username,
                              HttpMethod method,
                              const UriComponents& uri,
                              const HttpToolbox::Arguments& headers,
                              const HttpToolbox::GetArguments& getArguments,
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

  
  void OrthancRestApi::DocumentSubmitGenericJob(RestApiPostCall& call)
  {
    call.GetDocumentation()
      .SetRequestField(KEY_SYNCHRONOUS, RestApiCallDocumentation::Type_Boolean,
                       "If `true`, run the job in synchronous mode, which means that the HTTP answer will directly "
                       "contain the result of the job. This is the default, easy behavior, but it is *not* desirable for "
                       "long jobs, as it might lead to network timeouts.", false)
      .SetRequestField(KEY_ASYNCHRONOUS, RestApiCallDocumentation::Type_Boolean,
                       "If `true`, run the job in asynchronous mode, which means that the REST API call will immediately "
                       "return, reporting the identifier of a job. Prefer this flavor wherever possible.", false)
      .SetRequestField(KEY_PRIORITY, RestApiCallDocumentation::Type_Number,
                       "In asynchronous mode, the priority of the job. The lower the value, the higher the priority.", false)
      .SetAnswerField("ID", RestApiCallDocumentation::Type_String, "In asynchronous mode, identifier of the job")
      .SetAnswerField("Path", RestApiCallDocumentation::Type_String, "In asynchronous mode, path to access the job in the REST API");
  }
    

  void OrthancRestApi::DocumentSubmitCommandsJob(RestApiPostCall& call)
  {
    DocumentSubmitGenericJob(call);
    call.GetDocumentation()
      .SetRequestField(KEY_PERMISSIVE, RestApiCallDocumentation::Type_Boolean,
                       "If `true`, ignore errors during the individual steps of the job.", false);
  }
}
