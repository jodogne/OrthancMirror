/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2015 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
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
#include "../../Core/Uuid.h"
#include "../FromDcmtkBridge.h"
#include "../ServerContext.h"

namespace Orthanc
{
  // Modification of DICOM instances ------------------------------------------

  enum TagOperation
  {
    TagOperation_Keep,
    TagOperation_Remove
  };

  static void ParseListOfTags(DicomModification& target,
                              const Json::Value& query,
                              TagOperation operation)
  {
    if (!query.isArray())
    {
      throw OrthancException(ErrorCode_BadRequest);
    }

    for (Json::Value::ArrayIndex i = 0; i < query.size(); i++)
    {
      std::string name = query[i].asString();

      DicomTag tag = FromDcmtkBridge::ParseTag(name);

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
                                const Json::Value& replacements)
  {
    if (!replacements.isObject())
    {
      throw OrthancException(ErrorCode_BadRequest);
    }

    Json::Value::Members members = replacements.getMemberNames();
    for (size_t i = 0; i < members.size(); i++)
    {
      const std::string& name = members[i];
      std::string value = replacements[name].asString();

      DicomTag tag = FromDcmtkBridge::ParseTag(name);
      target.Replace(tag, value);

      VLOG(1) << "Replace: " << name << " " << tag << " == " << value << std::endl;
    }
  }


  static std::string GeneratePatientName(ServerContext& context)
  {
    uint64_t seq = context.GetIndex().IncrementGlobalSequence(GlobalProperty_AnonymizationSequence);
    return "Anonymized" + boost::lexical_cast<std::string>(seq);
  }



  bool OrthancRestApi::ParseModifyRequest(DicomModification& target,
                                          const Json::Value& request)
  {
    if (request.isObject())
    {
      if (request.isMember("RemovePrivateTags"))
      {
        target.SetRemovePrivateTags(true);
      }

      if (request.isMember("Remove"))
      {
        ParseListOfTags(target, request["Remove"], TagOperation_Remove);
      }

      if (request.isMember("Replace"))
      {
        ParseReplacements(target, request["Replace"]);
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
    // curl http://localhost:8042/instances/6e67da51-d119d6ae-c5667437-87b9a8a5-0f07c49f/anonymize -X POST -d '{"Replace":{"PatientName":"hello","0010-0020":"world"},"Keep":["StudyDescription", "SeriesDescription"],"KeepPrivateTags": null,"Remove":["Modality"]}' > Anonymized.dcm

    target.SetupAnonymization();
    std::string patientName = target.GetReplacement(DICOM_TAG_PATIENT_NAME);

    Json::Value request;
    if (call.ParseJsonRequest(request) && request.isObject())
    {
      if (request.isMember("KeepPrivateTags"))
      {
        target.SetRemovePrivateTags(false);
      }

      if (request.isMember("Remove"))
      {
        ParseListOfTags(target, request["Remove"], TagOperation_Remove);
      }

      if (request.isMember("Replace"))
      {
        ParseReplacements(target, request["Replace"]);
      }

      if (request.isMember("Keep"))
      {
        ParseListOfTags(target, request["Keep"], TagOperation_Keep);
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
    else
    {
      return false;
    }
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
        return;
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


  static bool CreateDicomV1(ParsedDicomFile& dicom,
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
        LOG(ERROR) << "Only string values are supported when creating DICOM instances";
        return false;
      }

      std::string value = request[name].asString();

      DicomTag tag = FromDcmtkBridge::ParseTag(name);
      if (tag == DICOM_TAG_PIXEL_DATA)
      {
        dicom.EmbedImage(value);
      }
      else
      {
        dicom.Replace(tag, value);
      }
    }

    return true;
  }


  static bool CreateDicomV2(ParsedDicomFile& dicom,
                            ServerContext& context,
                            const Json::Value& request)
  {
    assert(request.isObject());

    if (!request.isMember("Tags") ||
        request["Tags"].type() != Json::objectValue)
    {
      return false;
    }

    ResourceType parentType = ResourceType_Instance;

    if (request.isMember("Parent"))
    {
      // Locate the parent tags
      std::string parent = request["Parent"].asString();
      if (!context.GetIndex().LookupResourceType(parentType, parent))
      {
        LOG(ERROR) << "Trying to attach a new DICOM instance to an inexistent resource: " << parent;
        return false;
      }

      if (parentType == ResourceType_Instance)
      {
        LOG(ERROR) << "Trying to attach a new DICOM instance to an instance (must be a series, study or patient): " << parent;
        return false;
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
          return false;   // Error: No instance (should never happen)
        }

        context.ReadJson(siblingTags, siblingInstances.front());
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
          return false;
        }

        parent = tmp;
        type = GetParentResourceType(type);
      }

      for (ModuleTags::const_iterator it = moduleTags.begin();
           it != moduleTags.end(); it++)
      {
        std::string t = it->Format();
        if (siblingTags.isMember(t))
        {
          const Json::Value& tag = siblingTags[t];
          if (tag["Type"] == "Null")
          {
            dicom.Replace(*it, "");
          }
          else if (tag["Type"] == "String")
          {
            dicom.Replace(*it, tag["Value"].asString());
          }
        }
      }
    }


    // Inject time-related information
    std::string date, time;
    Toolbox::GetNowDicom(date, time);
    dicom.Replace(DICOM_TAG_ACQUISITION_DATE, date);
    dicom.Replace(DICOM_TAG_ACQUISITION_TIME, time);
    dicom.Replace(DICOM_TAG_CONTENT_DATE, date);
    dicom.Replace(DICOM_TAG_CONTENT_TIME, time);
    dicom.Replace(DICOM_TAG_INSTANCE_CREATION_DATE, date);
    dicom.Replace(DICOM_TAG_INSTANCE_CREATION_TIME, time);

    if (parentType == ResourceType_Patient ||
        parentType == ResourceType_Study ||
        parentType == ResourceType_Instance /* no parent */)
    {
      dicom.Replace(DICOM_TAG_SERIES_DATE, date);
      dicom.Replace(DICOM_TAG_SERIES_TIME, time);
    }

    if (parentType == ResourceType_Patient ||
        parentType == ResourceType_Instance /* no parent */)
    {
      dicom.Replace(DICOM_TAG_STUDY_DATE, date);
      dicom.Replace(DICOM_TAG_STUDY_TIME, time);
    }


    // Inject the user-specified tags
    Json::Value::Members members = request["Tags"].getMemberNames();
    for (size_t i = 0; i < members.size(); i++)
    {
      const std::string& name = members[i];
      if (request["Tags"][name].type() != Json::stringValue)
      {
        LOG(ERROR) << "Only string values are supported when creating DICOM instances";
        return false;
      }

      std::string value = request["Tags"][name].asString();

      DicomTag tag = FromDcmtkBridge::ParseTag(name);
      if (dicom.HasTag(tag))
      {
        LOG(ERROR) << "Trying to override a value inherited from a parent module";
        return false;
      }

      if (tag == DICOM_TAG_PIXEL_DATA)
      {
        LOG(ERROR) << "Use \"Content\" to inject an image into a new DICOM instance";
        return false;
      }
      else
      {
        dicom.Replace(tag, value);
      }
    }


    // Inject the content (either an image, or a PDF file)
    if (request.isMember("Content"))
    {
      if (request["Content"].type() != Json::stringValue)
      {
        LOG(ERROR) << "The payload of the DICOM instance must be specified according to Data URI scheme";
        return false;
      }

      std::string mime, base64;
      Toolbox::DecodeDataUriScheme(mime, base64, request["Content"].asString());
      Toolbox::ToLowerCase(mime);

      std::string content;
      Toolbox::DecodeBase64(content, base64);

      if (mime == "image/png")
      {
        dicom.EmbedImage(mime, content);
      }
      else if (mime == "application/pdf")
      {
        dicom.EmbedPdf(content);
      }
      else
      {
        LOG(ERROR) << "Unsupported MIME type for the content of a new DICOM file";
        return false;
      }
    }


    return true;
  }


  static void CreateDicom(RestApiPostCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    Json::Value request;
    if (call.ParseJsonRequest(request) && 
        request.isObject())
    {
      ParsedDicomFile dicom;

      if (request.isMember("Tags") ? 
          CreateDicomV2(dicom, context, request) :
          CreateDicomV1(dicom, request))
      {
        DicomInstanceToStore toStore;
        toStore.SetParsedDicomFile(dicom);

        std::string id;
        StoreStatus status = OrthancRestApi::GetContext(call).Store(id, toStore);

        if (status == StoreStatus_Failure)
        {
          LOG(ERROR) << "Error while storing a manually-created instance";
          return;
        }

        OrthancRestApi::GetApi(call).AnswerStoredInstance(call, id, status);
      }
    }
  }


  void OrthancRestApi::RegisterAnonymizeModify()
  {
    Register("/instances/{id}/modify", ModifyInstance);
    Register("/series/{id}/modify", ModifyResource<ChangeType_ModifiedSeries, ResourceType_Series>);
    Register("/studies/{id}/modify", ModifyResource<ChangeType_ModifiedStudy, ResourceType_Study>);
    Register("/patients/{id}/modify", ModifyResource<ChangeType_ModifiedPatient, ResourceType_Patient>);

    Register("/instances/{id}/anonymize", AnonymizeInstance);
    Register("/series/{id}/anonymize", AnonymizeResource<ChangeType_ModifiedSeries, ResourceType_Series>);
    Register("/studies/{id}/anonymize", AnonymizeResource<ChangeType_ModifiedStudy, ResourceType_Study>);
    Register("/patients/{id}/anonymize", AnonymizeResource<ChangeType_ModifiedPatient, ResourceType_Patient>);

    Register("/tools/create-dicom", CreateDicom);
  }
}
