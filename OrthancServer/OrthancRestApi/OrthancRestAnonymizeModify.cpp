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

#include "../../Core/Logging.h"
#include "../../Core/DicomParsing/FromDcmtkBridge.h"
#include "../ServerContext.h"
#include "../OrthancInitialization.h"

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/predicate.hpp>

namespace Orthanc
{
  // Modification of DICOM instances ------------------------------------------

  enum TagOperation
  {
    TagOperation_Keep,
    TagOperation_Remove
  };

  static bool IsDatabaseKey(const DicomTag& tag)
  {
    return (tag == DICOM_TAG_PATIENT_ID ||
            tag == DICOM_TAG_STUDY_INSTANCE_UID ||
            tag == DICOM_TAG_SERIES_INSTANCE_UID ||
            tag == DICOM_TAG_SOP_INSTANCE_UID);
  }

  static void ParseListOfTags(DicomModification& target,
                              const Json::Value& query,
                              TagOperation operation,
                              bool force)
  {
    if (!query.isArray())
    {
      throw OrthancException(ErrorCode_BadRequest);
    }

    for (Json::Value::ArrayIndex i = 0; i < query.size(); i++)
    {
      std::string name = query[i].asString();

      DicomTag tag = FromDcmtkBridge::ParseTag(name);

      if (!force && IsDatabaseKey(tag))
      {
        LOG(ERROR) << "Marking tag \"" << name << "\" as to be "
                   << (operation == TagOperation_Keep ? "kept" : "removed")
                   << " requires the \"Force\" option to be set to true";
        throw OrthancException(ErrorCode_BadRequest);
      }

      switch (operation)
      {
        case TagOperation_Keep:
          target.Keep(tag);
          VLOG(1) << "Keep: " << name << " " << tag << std::endl;
          break;

        case TagOperation_Remove:
          target.Remove(tag);
          VLOG(1) << "Remove: " << name << " " << tag << std::endl;
          break;

        default:
          throw OrthancException(ErrorCode_InternalError);
      }
    }
  }


  static void ParseReplacements(DicomModification& target,
                                const Json::Value& replacements,
                                bool force)
  {
    if (!replacements.isObject())
    {
      throw OrthancException(ErrorCode_BadRequest);
    }

    Json::Value::Members members = replacements.getMemberNames();
    for (size_t i = 0; i < members.size(); i++)
    {
      const std::string& name = members[i];
      const Json::Value& value = replacements[name];

      DicomTag tag = FromDcmtkBridge::ParseTag(name);

      if (!force && IsDatabaseKey(tag))
      {
        LOG(ERROR) << "Marking tag \"" << name << "\" as to be replaced "
                   << "requires the \"Force\" option to be set to true";
        throw OrthancException(ErrorCode_BadRequest);
      }

      target.Replace(tag, value, false);

      VLOG(1) << "Replace: " << name << " " << tag 
              << " == " << value.toStyledString() << std::endl;
    }
  }


  static std::string GeneratePatientName(ServerContext& context)
  {
    uint64_t seq = context.GetIndex().IncrementGlobalSequence(GlobalProperty_AnonymizationSequence);
    return "Anonymized" + boost::lexical_cast<std::string>(seq);
  }


  static bool GetBooleanValue(const std::string& member,
                              const Json::Value& json,
                              bool defaultValue)
  {
    if (!json.isMember(member))
    {
      return defaultValue;
    }
    else if (json[member].type() == Json::booleanValue)
    {
      return json[member].asBool();
    }
    else
    {
      LOG(ERROR) << "Member \"" << member << "\" should be a Boolean value";
      throw OrthancException(ErrorCode_BadFileFormat);
    }
  }


  bool OrthancRestApi::ParseModifyRequest(DicomModification& target,
                                          const Json::Value& request)
  {
    if (request.isObject())
    {
      bool force = GetBooleanValue("Force", request, false);
      
      if (GetBooleanValue("RemovePrivateTags", request, false))
      {
        target.SetRemovePrivateTags(true);
      }

      if (request.isMember("Remove"))
      {
        ParseListOfTags(target, request["Remove"], TagOperation_Remove, force);
      }

      if (request.isMember("Replace"))
      {
        ParseReplacements(target, request["Replace"], force);
      }

      // The "Keep" operation only makes sense for the tags
      // StudyInstanceUID, SeriesInstanceUID and SOPInstanceUID. Avoid
      // this feature as much as possible, as this breaks the DICOM
      // model of the real world, except if you know exactly what
      // you're doing!
      if (request.isMember("Keep"))
      {
        ParseListOfTags(target, request["Keep"], TagOperation_Keep, force);
      }

      return true;
    }
    else
    {
      return false;
    }
  }


  static bool ParseModifyRequest(DicomModification& target,
                                 const RestApiPostCall& call)
  {
    // curl http://localhost:8042/series/95a6e2bf-9296e2cc-bf614e2f-22b391ee-16e010e0/modify -X POST -d '{"Replace":{"InstitutionName":"My own clinic"}}'

    Json::Value request;
    if (call.ParseJsonRequest(request))
    {
      return OrthancRestApi::ParseModifyRequest(target, request);
    }
    else
    {
      return false;
    }
  }


  static bool ParseAnonymizationRequest(DicomModification& target,
                                        RestApiPostCall& call)
  {
    // curl http://localhost:8042/instances/6e67da51-d119d6ae-c5667437-87b9a8a5-0f07c49f/anonymize -X POST -d '{"Replace":{"PatientName":"hello","0010-0020":"world"},"Keep":["StudyDescription", "SeriesDescription"],"KeepPrivateTags": true,"Remove":["Modality"]}' > Anonymized.dcm

    Json::Value request;
    if (!call.ParseJsonRequest(request) ||
        !request.isObject())
    {
      return false;
    }

    bool force = GetBooleanValue("Force", request, false);
      
    // As of Orthanc 1.3.0, the default anonymization is done
    // according to PS 3.15-2017c Table E.1-1 (basic profile)
    DicomVersion version = DicomVersion_2017c;
    if (request.isMember("DicomVersion"))
    {
      if (request["DicomVersion"].type() != Json::stringValue)
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }
      else
      {
        version = StringToDicomVersion(request["DicomVersion"].asString());
      }
    }
        
    target.SetupAnonymization(version);
    std::string patientName = target.GetReplacementAsString(DICOM_TAG_PATIENT_NAME);

    if (GetBooleanValue("KeepPrivateTags", request, false))
    {
      target.SetRemovePrivateTags(false);
    }

    if (request.isMember("Remove"))
    {
      ParseListOfTags(target, request["Remove"], TagOperation_Remove, force);
    }

    if (request.isMember("Replace"))
    {
      ParseReplacements(target, request["Replace"], force);
    }

    if (request.isMember("Keep"))
    {
      ParseListOfTags(target, request["Keep"], TagOperation_Keep, force);
    }

    if (target.IsReplaced(DICOM_TAG_PATIENT_NAME) &&
        target.GetReplacement(DICOM_TAG_PATIENT_NAME) == patientName)
    {
      // Overwrite the random Patient's Name by one that is more
      // user-friendly (provided none was specified by the user)
      target.Replace(DICOM_TAG_PATIENT_NAME, GeneratePatientName(OrthancRestApi::GetContext(call)), true);
    }

    return true;
  }


  static void AnonymizeOrModifyInstance(DicomModification& modification,
                                        RestApiPostCall& call)
  {
    std::string id = call.GetUriComponent("id", "");

    ServerContext::DicomCacheLocker locker(OrthancRestApi::GetContext(call), id);

    std::auto_ptr<ParsedDicomFile> modified(locker.GetDicom().Clone());
    modification.Apply(*modified);
    modified->Answer(call.GetOutput());
  }


  static void AnonymizeOrModifyResource(DicomModification& modification,
                                        MetadataType metadataType,
                                        ChangeType changeType,
                                        ResourceType resourceType,
                                        RestApiPostCall& call)
  {
    bool isFirst = true;
    Json::Value result(Json::objectValue);

    ServerContext& context = OrthancRestApi::GetContext(call);

    typedef std::list<std::string> Instances;
    Instances instances;
    std::string id = call.GetUriComponent("id", "");
    context.GetIndex().GetChildInstances(instances, id);

    if (instances.empty())
    {
      return;
    }


    /**
     * Loop over all the instances of the resource.
     **/

    for (Instances::const_iterator it = instances.begin(); 
         it != instances.end(); ++it)
    {
      LOG(INFO) << "Modifying instance " << *it;

      std::auto_ptr<ServerContext::DicomCacheLocker> locker;

      try
      {
        locker.reset(new ServerContext::DicomCacheLocker(OrthancRestApi::GetContext(call), *it));
      }
      catch (OrthancException&)
      {
        // This child instance has been removed in between
        continue;
      }


      ParsedDicomFile& original = locker->GetDicom();
      DicomInstanceHasher originalHasher = original.GetHasher();


      /**
       * Compute the resulting DICOM instance.
       **/

      std::auto_ptr<ParsedDicomFile> modified(original.Clone());
      modification.Apply(*modified);

      DicomInstanceToStore toStore;
      toStore.SetRestOrigin(call);
      toStore.SetParsedDicomFile(*modified);


      /**
       * Prepare the metadata information to associate with the
       * resulting DICOM instance (AnonymizedFrom/ModifiedFrom).
       **/

      DicomInstanceHasher modifiedHasher = modified->GetHasher();

      if (originalHasher.HashSeries() != modifiedHasher.HashSeries())
      {
        toStore.AddMetadata(ResourceType_Series, metadataType, originalHasher.HashSeries());
      }

      if (originalHasher.HashStudy() != modifiedHasher.HashStudy())
      {
        toStore.AddMetadata(ResourceType_Study, metadataType, originalHasher.HashStudy());
      }

      if (originalHasher.HashPatient() != modifiedHasher.HashPatient())
      {
        toStore.AddMetadata(ResourceType_Patient, metadataType, originalHasher.HashPatient());
      }

      assert(*it == originalHasher.HashInstance());
      toStore.AddMetadata(ResourceType_Instance, metadataType, *it);


      /**
       * Store the resulting DICOM instance into the Orthanc store.
       **/

      std::string modifiedInstance;
      if (context.Store(modifiedInstance, toStore) != StoreStatus_Success)
      {
        LOG(ERROR) << "Error while storing a modified instance " << *it;
        throw OrthancException(ErrorCode_CannotStoreInstance);
      }

      // Sanity checks in debug mode
      assert(modifiedInstance == modifiedHasher.HashInstance());


      /**
       * Compute the JSON object that is returned by the REST call.
       **/

      if (isFirst)
      {
        std::string newId;

        switch (resourceType)
        {
          case ResourceType_Series:
            newId = modifiedHasher.HashSeries();
            break;

          case ResourceType_Study:
            newId = modifiedHasher.HashStudy();
            break;

          case ResourceType_Patient:
            newId = modifiedHasher.HashPatient();
            break;

          default:
            throw OrthancException(ErrorCode_InternalError);
        }

        result["Type"] = EnumerationToString(resourceType);
        result["ID"] = newId;
        result["Path"] = GetBasePath(resourceType, newId);
        result["PatientID"] = modifiedHasher.HashPatient();
        isFirst = false;
      }
    }

    call.GetOutput().AnswerJson(result);
  }



  static void ModifyInstance(RestApiPostCall& call)
  {
    DicomModification modification;
    modification.SetAllowManualIdentifiers(true);

    if (ParseModifyRequest(modification, call))
    {
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
  }


  static void AnonymizeInstance(RestApiPostCall& call)
  {
    DicomModification modification;
    modification.SetAllowManualIdentifiers(true);

    if (ParseAnonymizationRequest(modification, call))
    {
      AnonymizeOrModifyInstance(modification, call);
    }
  }


  template <enum ChangeType changeType,
            enum ResourceType resourceType>
  static void ModifyResource(RestApiPostCall& call)
  {
    DicomModification modification;

    if (ParseModifyRequest(modification, call))
    {
      modification.SetLevel(resourceType);
      AnonymizeOrModifyResource(modification, MetadataType_ModifiedFrom, 
                                changeType, resourceType, call);
    }
  }


  template <enum ChangeType changeType,
            enum ResourceType resourceType>
  static void AnonymizeResource(RestApiPostCall& call)
  {
    DicomModification modification;

    if (ParseAnonymizationRequest(modification, call))
    {
      AnonymizeOrModifyResource(modification, MetadataType_AnonymizedFrom, 
                                changeType, resourceType, call);
    }
  }


  static void StoreCreatedInstance(std::string& id /* out */,
                                   RestApiPostCall& call,
                                   ParsedDicomFile& dicom)
  {
    DicomInstanceToStore toStore;
    toStore.SetRestOrigin(call);
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
        std::auto_ptr<ParsedDicomFile> dicom(base.Clone());
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
    Register("/series/{id}/modify", ModifyResource<ChangeType_ModifiedSeries, ResourceType_Series>);
    Register("/studies/{id}/modify", ModifyResource<ChangeType_ModifiedStudy, ResourceType_Study>);
    Register("/patients/{id}/modify", ModifyResource<ChangeType_ModifiedPatient, ResourceType_Patient>);

    Register("/instances/{id}/anonymize", AnonymizeInstance);
    Register("/series/{id}/anonymize", AnonymizeResource<ChangeType_AnonymizedSeries, ResourceType_Series>);
    Register("/studies/{id}/anonymize", AnonymizeResource<ChangeType_AnonymizedStudy, ResourceType_Study>);
    Register("/patients/{id}/anonymize", AnonymizeResource<ChangeType_AnonymizedPatient, ResourceType_Patient>);

    Register("/tools/create-dicom", CreateDicom);
  }
}
