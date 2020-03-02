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

#include "../../Core/DicomParsing/FromDcmtkBridge.h"
#include "../../Core/Logging.h"
#include "../../Core/SerializationToolbox.h"
#include "../OrthancConfiguration.h"
#include "../ServerContext.h"
#include "../ServerJobs/MergeStudyJob.h"
#include "../ServerJobs/ResourceModificationJob.h"
#include "../ServerJobs/SplitStudyJob.h"

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/predicate.hpp>

namespace Orthanc
{
  // Modification of DICOM instances ------------------------------------------

  
  static std::string GeneratePatientName(ServerContext& context)
  {
    uint64_t seq = context.GetIndex().IncrementGlobalSequence(GlobalProperty_AnonymizationSequence);
    return "Anonymized" + boost::lexical_cast<std::string>(seq);
  }


  static void ParseModifyRequest(Json::Value& request,
                                 DicomModification& target,
                                 const RestApiPostCall& call)
  {
    // curl http://localhost:8042/series/95a6e2bf-9296e2cc-bf614e2f-22b391ee-16e010e0/modify -X POST -d '{"Replace":{"InstitutionName":"My own clinic"},"Priority":9}'

    {
      OrthancConfiguration::ReaderLock lock;
      target.SetPrivateCreator(lock.GetConfiguration().GetDefaultPrivateCreator());
    }
    
    if (call.ParseJsonRequest(request))
    {
      target.ParseModifyRequest(request);
    }
    else
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }
  }


  static void ParseAnonymizationRequest(Json::Value& request,
                                        DicomModification& target,
                                        RestApiPostCall& call)
  {
    // curl http://localhost:8042/instances/6e67da51-d119d6ae-c5667437-87b9a8a5-0f07c49f/anonymize -X POST -d '{"Replace":{"PatientName":"hello","0010-0020":"world"},"Keep":["StudyDescription", "SeriesDescription"],"KeepPrivateTags": true,"Remove":["Modality"]}' > Anonymized.dcm

    {
      OrthancConfiguration::ReaderLock lock;
      target.SetPrivateCreator(lock.GetConfiguration().GetDefaultPrivateCreator());
    }
    
    if (call.ParseJsonRequest(request) &&
        request.isObject())
    {
      bool patientNameReplaced;
      target.ParseAnonymizationRequest(patientNameReplaced, request);

      if (patientNameReplaced)
      {
        // Overwrite the random Patient's Name by one that is more
        // user-friendly (provided none was specified by the user)
        target.Replace(DICOM_TAG_PATIENT_NAME, GeneratePatientName(OrthancRestApi::GetContext(call)), true);
      }
    }
    else
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }
  }


  static void AnonymizeOrModifyInstance(DicomModification& modification,
                                        RestApiPostCall& call)
  {
    std::string id = call.GetUriComponent("id", "");

    std::unique_ptr<ParsedDicomFile> modified;

    {
      ServerContext::DicomCacheLocker locker(OrthancRestApi::GetContext(call), id);
      modified.reset(locker.GetDicom().Clone(true));
    }
    
    modification.Apply(*modified);
    modified->Answer(call.GetOutput());
  }


  static void ModifyInstance(RestApiPostCall& call)
  {
    DicomModification modification;
    modification.SetAllowManualIdentifiers(true);

    Json::Value request;
    ParseModifyRequest(request, modification, call);

    if (modification.IsReplaced(DICOM_TAG_PATIENT_ID))
    {
      modification.SetLevel(ResourceType_Patient);
    }
    else if (modification.IsReplaced(DICOM_TAG_STUDY_INSTANCE_UID))
    {
      modification.SetLevel(ResourceType_Study);
    }
    else if (modification.IsReplaced(DICOM_TAG_SERIES_INSTANCE_UID))
    {
      modification.SetLevel(ResourceType_Series);
    }
    else
    {
      modification.SetLevel(ResourceType_Instance);
    }

    AnonymizeOrModifyInstance(modification, call);
  }


  static void AnonymizeInstance(RestApiPostCall& call)
  {
    DicomModification modification;
    modification.SetAllowManualIdentifiers(true);

    Json::Value request;
    ParseAnonymizationRequest(request, modification, call);

    AnonymizeOrModifyInstance(modification, call);
  }


  static void SubmitModificationJob(std::unique_ptr<DicomModification>& modification,
                                    bool isAnonymization,
                                    RestApiPostCall& call,
                                    const Json::Value& body,
                                    ResourceType level)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    std::unique_ptr<ResourceModificationJob> job(new ResourceModificationJob(context));
    
    job->SetModification(modification.release(), level, isAnonymization);
    job->SetOrigin(call);
    
    context.AddChildInstances(*job, call.GetUriComponent("id", ""));

    OrthancRestApi::GetApi(call).SubmitCommandsJob
      (call, job.release(), true /* synchronous by default */, body);
  }


  template <enum ResourceType resourceType>
  static void ModifyResource(RestApiPostCall& call)
  {
    std::unique_ptr<DicomModification> modification(new DicomModification);

    Json::Value body;
    ParseModifyRequest(body, *modification, call);

    modification->SetLevel(resourceType);

    SubmitModificationJob(modification, false /* not an anonymization */,
                          call, body, resourceType);
  }


  template <enum ResourceType resourceType>
  static void AnonymizeResource(RestApiPostCall& call)
  {
    std::unique_ptr<DicomModification> modification(new DicomModification);

    Json::Value body;
    ParseAnonymizationRequest(body, *modification, call);

    SubmitModificationJob(modification, true /* anonymization */,
                          call, body, resourceType);
  }


  static void StoreCreatedInstance(std::string& id /* out */,
                                   RestApiPostCall& call,
                                   ParsedDicomFile& dicom,
                                   bool sendAnswer)
  {
    DicomInstanceToStore toStore;
    toStore.SetOrigin(DicomInstanceOrigin::FromRest(call));
    toStore.SetParsedDicomFile(dicom);

    ServerContext& context = OrthancRestApi::GetContext(call);
    StoreStatus status = context.Store(id, toStore);

    if (status == StoreStatus_Failure)
    {
      throw OrthancException(ErrorCode_CannotStoreInstance);
    }

    if (sendAnswer)
    {
      OrthancRestApi::GetApi(call).AnswerStoredInstance(call, toStore, status);
    }
  }


  static void CreateDicomV1(ParsedDicomFile& dicom,
                            RestApiPostCall& call,
                            const Json::Value& request)
  {
    // curl http://localhost:8042/tools/create-dicom -X POST -d '{"PatientName":"Hello^World"}'
    // curl http://localhost:8042/tools/create-dicom -X POST -d '{"PatientName":"Hello^World","PixelData":"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAAAAAA6mKC9AAAACXBIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH3gUGDDcB53FulQAAAElJREFUGNNtj0sSAEEEQ1+U+185s1CtmRkblQ9CZldsKHJDk6DLGLJa6chjh0ooQmpjXMM86zPwydGEj6Ed/UGykkEM8X+p3u8/8LcOJIWLGeMAAAAASUVORK5CYII="}'

    assert(request.isObject());
    LOG(WARNING) << "Using a deprecated call to /tools/create-dicom";

    Json::Value::Members members = request.getMemberNames();
    for (size_t i = 0; i < members.size(); i++)
    {
      const std::string& name = members[i];
      if (request[name].type() != Json::stringValue)
      {
        throw OrthancException(ErrorCode_CreateDicomNotString);
      }

      std::string value = request[name].asString();

      DicomTag tag = FromDcmtkBridge::ParseTag(name);
      if (tag == DICOM_TAG_PIXEL_DATA)
      {
        dicom.EmbedContent(value);
      }
      else
      {
        // This is V1, don't try and decode data URI scheme
        dicom.ReplacePlainString(tag, value);
      }
    }
  }


  static void InjectTags(ParsedDicomFile& dicom,
                         const Json::Value& tags,
                         bool decodeBinaryTags,
                         const std::string& privateCreator)
  {
    if (tags.type() != Json::objectValue)
    {
      throw OrthancException(ErrorCode_BadRequest, "Tags field is not an array");
    }

    // Inject the user-specified tags
    Json::Value::Members members = tags.getMemberNames();
    for (size_t i = 0; i < members.size(); i++)
    {
      const std::string& name = members[i];
      DicomTag tag = FromDcmtkBridge::ParseTag(name);

      if (tag != DICOM_TAG_SPECIFIC_CHARACTER_SET)
      {
        if (tag != DICOM_TAG_PATIENT_ID &&
            tag != DICOM_TAG_ACQUISITION_DATE &&
            tag != DICOM_TAG_ACQUISITION_TIME &&
            tag != DICOM_TAG_CONTENT_DATE &&
            tag != DICOM_TAG_CONTENT_TIME &&
            tag != DICOM_TAG_INSTANCE_CREATION_DATE &&
            tag != DICOM_TAG_INSTANCE_CREATION_TIME &&
            tag != DICOM_TAG_SERIES_DATE &&
            tag != DICOM_TAG_SERIES_TIME &&
            tag != DICOM_TAG_STUDY_DATE &&
            tag != DICOM_TAG_STUDY_TIME &&
            dicom.HasTag(tag))
        {
          throw OrthancException(ErrorCode_CreateDicomOverrideTag, name);
        }

        if (tag == DICOM_TAG_PIXEL_DATA)
        {
          throw OrthancException(ErrorCode_CreateDicomUseContent);
        }
        else
        {
          dicom.Replace(tag, tags[name], decodeBinaryTags, DicomReplaceMode_InsertIfAbsent, privateCreator);
        }
      }
    }
  }


  static void CreateSeries(RestApiPostCall& call,
                           ParsedDicomFile& base /* in */,
                           const Json::Value& content,
                           bool decodeBinaryTags,
                           const std::string& privateCreator)
  {
    assert(content.isArray());
    assert(content.size() > 0);
    ServerContext& context = OrthancRestApi::GetContext(call);

    base.ReplacePlainString(DICOM_TAG_IMAGES_IN_ACQUISITION, boost::lexical_cast<std::string>(content.size()));
    base.ReplacePlainString(DICOM_TAG_NUMBER_OF_TEMPORAL_POSITIONS, "1");

    std::string someInstance;

    try
    {
      for (Json::ArrayIndex i = 0; i < content.size(); i++)
      {
        std::unique_ptr<ParsedDicomFile> dicom(base.Clone(false));
        const Json::Value* payload = NULL;

        if (content[i].type() == Json::stringValue)
        {
          payload = &content[i];
        }
        else if (content[i].type() == Json::objectValue)
        {
          if (!content[i].isMember("Content"))
          {
            throw OrthancException(ErrorCode_CreateDicomNoPayload);
          }

          payload = &content[i]["Content"];

          if (content[i].isMember("Tags"))
          {
            InjectTags(*dicom, content[i]["Tags"], decodeBinaryTags, privateCreator);
          }
        }

        if (payload == NULL ||
            payload->type() != Json::stringValue)
        {
          throw OrthancException(ErrorCode_CreateDicomUseDataUriScheme);
        }

        dicom->EmbedContent(payload->asString());
        dicom->ReplacePlainString(DICOM_TAG_INSTANCE_NUMBER, boost::lexical_cast<std::string>(i + 1));
        dicom->ReplacePlainString(DICOM_TAG_IMAGE_INDEX, boost::lexical_cast<std::string>(i + 1));

        StoreCreatedInstance(someInstance, call, *dicom, false);
      }
    }
    catch (OrthancException&)
    {
      // Error: Remove the newly-created series
      
      std::string series;
      if (context.GetIndex().LookupParent(series, someInstance))
      {
        Json::Value dummy;
        context.GetIndex().DeleteResource(dummy, series, ResourceType_Series);
      }

      throw;
    }

    std::string series;
    if (context.GetIndex().LookupParent(series, someInstance))
    {
      OrthancRestApi::GetApi(call).AnswerStoredResource(call, series, ResourceType_Series, StoreStatus_Success);
    }
  }


  static void CreateDicomV2(RestApiPostCall& call,
                            const Json::Value& request)
  {
    assert(request.isObject());
    ServerContext& context = OrthancRestApi::GetContext(call);

    if (!request.isMember("Tags") ||
        request["Tags"].type() != Json::objectValue)
    {
      throw OrthancException(ErrorCode_BadRequest);
    }

    ParsedDicomFile dicom(true);

    {
      Encoding encoding;

      if (request["Tags"].isMember("SpecificCharacterSet"))
      {
        const char* tmp = request["Tags"]["SpecificCharacterSet"].asCString();
        if (!GetDicomEncoding(encoding, tmp))
        {
          throw OrthancException(ErrorCode_ParameterOutOfRange,
                                 "Unknown specific character set: " + std::string(tmp));
        }
      }
      else
      {
        encoding = GetDefaultDicomEncoding();
      }

      dicom.SetEncoding(encoding);
    }

    ResourceType parentType = ResourceType_Instance;

    if (request.isMember("Parent"))
    {
      // Locate the parent tags
      std::string parent = request["Parent"].asString();
      if (!context.GetIndex().LookupResourceType(parentType, parent))
      {
        throw OrthancException(ErrorCode_CreateDicomBadParent);
      }

      if (parentType == ResourceType_Instance)
      {
        throw OrthancException(ErrorCode_CreateDicomParentIsInstance);
      }

      // Select one existing child instance of the parent resource, to
      // retrieve all its tags
      Json::Value siblingTags;
      std::string siblingInstanceId;

      {
        // Retrieve all the instances of the parent resource
        std::list<std::string>  siblingInstances;
        context.GetIndex().GetChildInstances(siblingInstances, parent);

        if (siblingInstances.empty())
	{
	  // Error: No instance (should never happen)
          throw OrthancException(ErrorCode_InternalError);
        }

        siblingInstanceId = siblingInstances.front();
        context.ReadDicomAsJson(siblingTags, siblingInstanceId);
      }


      // Choose the same encoding as the parent resource
      {
        static const char* SPECIFIC_CHARACTER_SET = "0008,0005";

        if (siblingTags.isMember(SPECIFIC_CHARACTER_SET))
        {
          Encoding encoding;

          if (!siblingTags[SPECIFIC_CHARACTER_SET].isMember("Value") ||
              siblingTags[SPECIFIC_CHARACTER_SET]["Value"].type() != Json::stringValue ||
              !GetDicomEncoding(encoding, siblingTags[SPECIFIC_CHARACTER_SET]["Value"].asCString()))
          {
            LOG(WARNING) << "Instance with an incorrect Specific Character Set, "
                         << "using the default Orthanc encoding: " << siblingInstanceId;
            encoding = GetDefaultDicomEncoding();
          }

          dicom.SetEncoding(encoding);
        }
      }


      // Retrieve the tags for all the parent modules
      typedef std::set<DicomTag> ModuleTags;
      ModuleTags moduleTags;

      ResourceType type = parentType;
      for (;;)
      {
        DicomTag::AddTagsForModule(moduleTags, GetModule(type));
      
        if (type == ResourceType_Patient)
        {
          break;   // We're done
        }

        // Go up
        std::string tmp;
        if (!context.GetIndex().LookupParent(tmp, parent))
        {
          throw OrthancException(ErrorCode_InternalError);
        }

        parent = tmp;
        type = GetParentResourceType(type);
      }

      for (ModuleTags::const_iterator it = moduleTags.begin();
           it != moduleTags.end(); ++it)
      {
        std::string t = it->Format();
        if (siblingTags.isMember(t))
        {
          const Json::Value& tag = siblingTags[t];
          if (tag["Type"] == "Null")
          {
            dicom.ReplacePlainString(*it, "");
          }
          else if (tag["Type"] == "String")
          {
            std::string value = tag["Value"].asString();  // This is an UTF-8 value (as it comes from JSON)
            dicom.ReplacePlainString(*it, value);
          }
        }
      }
    }


    bool decodeBinaryTags = true;
    if (request.isMember("InterpretBinaryTags"))
    {
      const Json::Value& v = request["InterpretBinaryTags"];
      if (v.type() != Json::booleanValue)
      {
        throw OrthancException(ErrorCode_BadRequest);
      }

      decodeBinaryTags = v.asBool();
    }


    // New argument in Orthanc 1.6.0
    std::string privateCreator;
    if (request.isMember("PrivateCreator"))
    {
      const Json::Value& v = request["PrivateCreator"];
      if (v.type() != Json::stringValue)
      {
        throw OrthancException(ErrorCode_BadRequest);
      }

      privateCreator = v.asString();
    }
    else
    {
      OrthancConfiguration::ReaderLock lock;
      privateCreator = lock.GetConfiguration().GetDefaultPrivateCreator();
    }

    
    // Inject time-related information
    std::string date, time;
    SystemToolbox::GetNowDicom(date, time, true /* use UTC time (not local time) */);
    dicom.ReplacePlainString(DICOM_TAG_ACQUISITION_DATE, date);
    dicom.ReplacePlainString(DICOM_TAG_ACQUISITION_TIME, time);
    dicom.ReplacePlainString(DICOM_TAG_CONTENT_DATE, date);
    dicom.ReplacePlainString(DICOM_TAG_CONTENT_TIME, time);
    dicom.ReplacePlainString(DICOM_TAG_INSTANCE_CREATION_DATE, date);
    dicom.ReplacePlainString(DICOM_TAG_INSTANCE_CREATION_TIME, time);

    if (parentType == ResourceType_Patient ||
        parentType == ResourceType_Study ||
        parentType == ResourceType_Instance /* no parent */)
    {
      dicom.ReplacePlainString(DICOM_TAG_SERIES_DATE, date);
      dicom.ReplacePlainString(DICOM_TAG_SERIES_TIME, time);
    }

    if (parentType == ResourceType_Patient ||
        parentType == ResourceType_Instance /* no parent */)
    {
      dicom.ReplacePlainString(DICOM_TAG_STUDY_DATE, date);
      dicom.ReplacePlainString(DICOM_TAG_STUDY_TIME, time);
    }


    InjectTags(dicom, request["Tags"], decodeBinaryTags, privateCreator);


    // Inject the content (either an image, or a PDF file)
    if (request.isMember("Content"))
    {
      const Json::Value& content = request["Content"];

      if (content.type() == Json::stringValue)
      {
        dicom.EmbedContent(request["Content"].asString());

      }
      else if (content.type() == Json::arrayValue)
      {
        if (content.size() > 0)
        {
          // Let's create a series instead of a single instance
          CreateSeries(call, dicom, content, decodeBinaryTags, privateCreator);
          return;
        }
      }
      else
      {
        throw OrthancException(ErrorCode_CreateDicomUseDataUriScheme);
      }
    }

    std::string id;
    StoreCreatedInstance(id, call, dicom, true);
  }


  static void CreateDicom(RestApiPostCall& call)
  {
    Json::Value request;
    if (!call.ParseJsonRequest(request) ||
        !request.isObject())
    {
      throw OrthancException(ErrorCode_BadRequest);
    }

    if (request.isMember("Tags"))
    {
      CreateDicomV2(call, request);
    }
    else
    {
      // Compatibility with Orthanc <= 0.9.3
      ParsedDicomFile dicom(true);
      CreateDicomV1(dicom, call, request);

      std::string id;
      StoreCreatedInstance(id, call, dicom, true);
    }
  }


  static void SplitStudy(RestApiPostCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    Json::Value request;
    if (!call.ParseJsonRequest(request))
    {
      // Bad JSON request
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    const std::string study = call.GetUriComponent("id", "");

    std::unique_ptr<SplitStudyJob> job(new SplitStudyJob(context, study));    
    job->SetOrigin(call);

    std::vector<std::string> series;
    SerializationToolbox::ReadArrayOfStrings(series, request, "Series");

    for (size_t i = 0; i < series.size(); i++)
    {
      job->AddSourceSeries(series[i]);
    }

    job->AddTrailingStep();

    static const char* KEEP_SOURCE = "KeepSource";
    if (request.isMember(KEEP_SOURCE))
    {
      job->SetKeepSource(SerializationToolbox::ReadBoolean(request, KEEP_SOURCE));
    }

    static const char* REMOVE = "Remove";
    if (request.isMember(REMOVE))
    {
      if (request[REMOVE].type() != Json::arrayValue)
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }

      for (Json::Value::ArrayIndex i = 0; i < request[REMOVE].size(); i++)
      {
        if (request[REMOVE][i].type() != Json::stringValue)
        {
          throw OrthancException(ErrorCode_BadFileFormat);
        }
        else
        {
          job->Remove(FromDcmtkBridge::ParseTag(request[REMOVE][i].asCString()));
        }
      }
    }

    static const char* REPLACE = "Replace";
    if (request.isMember(REPLACE))
    {
      if (request[REPLACE].type() != Json::objectValue)
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }

      Json::Value::Members tags = request[REPLACE].getMemberNames();

      for (size_t i = 0; i < tags.size(); i++)
      {
        const Json::Value& value = request[REPLACE][tags[i]];
        
        if (value.type() != Json::stringValue)
        {
          throw OrthancException(ErrorCode_BadFileFormat);
        }
        else
        {
          job->Replace(FromDcmtkBridge::ParseTag(tags[i]), value.asString());
        }
      }
    }

    OrthancRestApi::GetApi(call).SubmitCommandsJob
      (call, job.release(), true /* synchronous by default */, request);
  }


  static void MergeStudy(RestApiPostCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    Json::Value request;
    if (!call.ParseJsonRequest(request))
    {
      // Bad JSON request
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    const std::string study = call.GetUriComponent("id", "");

    std::unique_ptr<MergeStudyJob> job(new MergeStudyJob(context, study));    
    job->SetOrigin(call);

    std::vector<std::string> resources;
    SerializationToolbox::ReadArrayOfStrings(resources, request, "Resources");

    for (size_t i = 0; i < resources.size(); i++)
    {
      job->AddSource(resources[i]);
    }

    job->AddTrailingStep();

    static const char* KEEP_SOURCE = "KeepSource";
    if (request.isMember(KEEP_SOURCE))
    {
      job->SetKeepSource(SerializationToolbox::ReadBoolean(request, KEEP_SOURCE));
    }

    OrthancRestApi::GetApi(call).SubmitCommandsJob
      (call, job.release(), true /* synchronous by default */, request);
  }
  

  void OrthancRestApi::RegisterAnonymizeModify()
  {
    Register("/instances/{id}/modify", ModifyInstance);
    Register("/series/{id}/modify", ModifyResource<ResourceType_Series>);
    Register("/studies/{id}/modify", ModifyResource<ResourceType_Study>);
    Register("/patients/{id}/modify", ModifyResource<ResourceType_Patient>);

    Register("/instances/{id}/anonymize", AnonymizeInstance);
    Register("/series/{id}/anonymize", AnonymizeResource<ResourceType_Series>);
    Register("/studies/{id}/anonymize", AnonymizeResource<ResourceType_Study>);
    Register("/patients/{id}/anonymize", AnonymizeResource<ResourceType_Patient>);

    Register("/tools/create-dicom", CreateDicom);

    Register("/studies/{id}/split", SplitStudy);
    Register("/studies/{id}/merge", MergeStudy);
  }
}
