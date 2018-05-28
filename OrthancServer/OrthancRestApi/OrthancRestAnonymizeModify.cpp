/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2018 Osimis S.A., Belgium
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
#include "../ServerContext.h"
#include "../ServerJobs/ResourceModificationJob.h"

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


  static int GetPriority(const Json::Value& request)
  {
    static const char* PRIORITY = "Priority";
    
    if (request.isMember(PRIORITY))
    {
      if (request[PRIORITY].type() == Json::intValue)
      {
        return request[PRIORITY].asInt();
      }
      else
      {
        LOG(ERROR) << "Field \"" << PRIORITY << "\" of a modification request should be an integer";
        throw OrthancException(ErrorCode_BadFileFormat);        
      }
    }
    else
    {
      return 0;   // Default priority
    }
  }


  static void ParseModifyRequest(DicomModification& target,
                                 int& priority,
                                 const RestApiPostCall& call)
  {
    // curl http://localhost:8042/series/95a6e2bf-9296e2cc-bf614e2f-22b391ee-16e010e0/modify -X POST -d '{"Replace":{"InstitutionName":"My own clinic"},"Priority":9}'

    Json::Value request;
    if (call.ParseJsonRequest(request))
    {
      priority = GetPriority(request);
      target.ParseModifyRequest(request);
    }
    else
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }
  }


  static void ParseAnonymizationRequest(DicomModification& target,
                                        int& priority,
                                        RestApiPostCall& call)
  {
    // curl http://localhost:8042/instances/6e67da51-d119d6ae-c5667437-87b9a8a5-0f07c49f/anonymize -X POST -d '{"Replace":{"PatientName":"hello","0010-0020":"world"},"Keep":["StudyDescription", "SeriesDescription"],"KeepPrivateTags": true,"Remove":["Modality"]}' > Anonymized.dcm

    Json::Value request;
    if (call.ParseJsonRequest(request) &&
        request.isObject())
    {
      priority = GetPriority(request);

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

    ServerContext::DicomCacheLocker locker(OrthancRestApi::GetContext(call), id);

    std::auto_ptr<ParsedDicomFile> modified(locker.GetDicom().Clone(true));
    modification.Apply(*modified);
    modified->Answer(call.GetOutput());
  }



  static void SubmitJob(std::auto_ptr<DicomModification>& modification,
                        bool isAnonymization,
                        ResourceType level,
                        int priority,
                        RestApiPostCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    std::auto_ptr<ResourceModificationJob> job(new ResourceModificationJob(context));
    
    boost::shared_ptr<ResourceModificationJob::Output> output(new ResourceModificationJob::Output(level));
    job->SetModification(modification.release(), isAnonymization);
    job->SetOutput(output);
    job->SetOrigin(call);
    job->SetDescription("REST API");

    context.AddChildInstances(*job, call.GetUriComponent("id", ""));
    
    if (context.GetJobsEngine().GetRegistry().SubmitAndWait(job.release(), priority))
    {
      Json::Value json;
      if (output->Format(json))
      {
        call.GetOutput().AnswerJson(json);
        return;
      }
    }

    call.GetOutput().SignalError(HttpStatus_500_InternalServerError);
  }



  static void ModifyInstance(RestApiPostCall& call)
  {
    DicomModification modification;
    modification.SetAllowManualIdentifiers(true);

    int priority;
    ParseModifyRequest(modification, priority, call);

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

    int priority;
    ParseAnonymizationRequest(modification, priority, call);

    AnonymizeOrModifyInstance(modification, call);
  }


  template <enum ResourceType resourceType>
  static void ModifyResource(RestApiPostCall& call)
  {
    std::auto_ptr<DicomModification> modification(new DicomModification);

    int priority;
    ParseModifyRequest(*modification, priority, call);

    modification->SetLevel(resourceType);
    SubmitJob(modification, false, resourceType, priority, call);
  }


  template <enum ResourceType resourceType>
  static void AnonymizeResource(RestApiPostCall& call)
  {
    std::auto_ptr<DicomModification> modification(new DicomModification);

    int priority;
    ParseAnonymizationRequest(*modification, priority, call);

    SubmitJob(modification, true, resourceType, priority, call);
  }


  static void StoreCreatedInstance(std::string& id /* out */,
                                   RestApiPostCall& call,
                                   ParsedDicomFile& dicom)
  {
    DicomInstanceToStore toStore;
    toStore.GetOrigin().SetRestOrigin(call);
    toStore.SetParsedDicomFile(dicom);

    ServerContext& context = OrthancRestApi::GetContext(call);
    StoreStatus status = context.Store(id, toStore);

    if (status == StoreStatus_Failure)
    {
      throw OrthancException(ErrorCode_CannotStoreInstance);
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
                         bool decodeBinaryTags)
  {
    if (tags.type() != Json::objectValue)
    {
      throw OrthancException(ErrorCode_BadRequest);
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
          throw OrthancException(ErrorCode_CreateDicomOverrideTag);
        }

        if (tag == DICOM_TAG_PIXEL_DATA)
        {
          throw OrthancException(ErrorCode_CreateDicomUseContent);
        }
        else
        {
          dicom.Replace(tag, tags[name], decodeBinaryTags, DicomReplaceMode_InsertIfAbsent);
        }
      }
    }
  }


  static void CreateSeries(RestApiPostCall& call,
                           ParsedDicomFile& base /* in */,
                           const Json::Value& content,
                           bool decodeBinaryTags)
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
        std::auto_ptr<ParsedDicomFile> dicom(base.Clone(false));
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
            InjectTags(*dicom, content[i]["Tags"], decodeBinaryTags);
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

        StoreCreatedInstance(someInstance, call, *dicom);
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
          LOG(ERROR) << "Unknown specific character set: " << std::string(tmp);
          throw OrthancException(ErrorCode_ParameterOutOfRange);
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

      {
        // Retrieve all the instances of the parent resource
        std::list<std::string>  siblingInstances;
        context.GetIndex().GetChildInstances(siblingInstances, parent);

        if (siblingInstances.empty())
	{
	  // Error: No instance (should never happen)
          throw OrthancException(ErrorCode_InternalError);
        }

        context.ReadDicomAsJson(siblingTags, siblingInstances.front());
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
            throw OrthancException(ErrorCode_CreateDicomParentEncoding);
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
            std::string value = tag["Value"].asString();
            dicom.ReplacePlainString(*it, Toolbox::ConvertFromUtf8(value, dicom.GetEncoding()));
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


    InjectTags(dicom, request["Tags"], decodeBinaryTags);


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
          CreateSeries(call, dicom, content, decodeBinaryTags);
          return;
        }
      }
      else
      {
        throw OrthancException(ErrorCode_CreateDicomUseDataUriScheme);
      }
    }

    std::string id;
    StoreCreatedInstance(id, call, dicom);
    OrthancRestApi::GetApi(call).AnswerStoredResource(call, id, ResourceType_Instance, StoreStatus_Success);

    return;
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
      StoreCreatedInstance(id, call, dicom);
      OrthancRestApi::GetApi(call).AnswerStoredResource(call, id, ResourceType_Instance, StoreStatus_Success);
    }
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
  }
}
