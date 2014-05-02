/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2014 Medical Physics Department, CHU of Liege,
 * Belgium
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


#include "OrthancRestApi.h"

#include <glog/logging.h>

namespace Orthanc
{
  // Modification of DICOM instances ------------------------------------------

  namespace
  {
    typedef std::set<DicomTag> Removals;
    typedef std::map<DicomTag, std::string> Replacements;
    typedef std::map< std::pair<DicomRootLevel, std::string>, std::string>  UidMap;
  }

  static void ReplaceInstanceInternal(ParsedDicomFile& toModify,
                                      const Removals& removals,
                                      const Replacements& replacements,
                                      bool removePrivateTags)
  {
    if (removePrivateTags)
    {
      toModify.RemovePrivateTags();
    }

    for (Removals::const_iterator it = removals.begin(); 
         it != removals.end(); ++it)
    {
      toModify.Remove(*it);
    }

    for (Replacements::const_iterator it = replacements.begin(); 
         it != replacements.end(); ++it)
    {
      toModify.Replace(it->first, it->second, DicomReplaceMode_InsertIfAbsent);
    }

    // A new SOP instance UID is automatically generated
    std::string instanceUid = FromDcmtkBridge::GenerateUniqueIdentifier(DicomRootLevel_Instance);
    toModify.Replace(DICOM_TAG_SOP_INSTANCE_UID, instanceUid, DicomReplaceMode_InsertIfAbsent);
  }


  static void ParseRemovals(Removals& target,
                            const Json::Value& removals)
  {
    if (!removals.isArray())
    {
      throw OrthancException(ErrorCode_BadRequest);
    }

    for (Json::Value::ArrayIndex i = 0; i < removals.size(); i++)
    {
      std::string name = removals[i].asString();
      DicomTag tag = FromDcmtkBridge::ParseTag(name);
      target.insert(tag);

      VLOG(1) << "Removal: " << name << " " << tag << std::endl;
    }
  }


  static void ParseReplacements(Replacements& target,
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
      target[tag] = value;

      VLOG(1) << "Replacement: " << name << " " << tag << " == " << value << std::endl;
    }
  }


  static std::string GeneratePatientName(ServerContext& context)
  {
    uint64_t seq = context.GetIndex().IncrementGlobalSequence(GlobalProperty_AnonymizationSequence);
    return "Anonymized" + boost::lexical_cast<std::string>(seq);
  }


  static void SetupAnonymization(Removals& removals,
                                 Replacements& replacements)
  {
    // This is Table E.1-1 from PS 3.15-2008 - DICOM Part 15: Security and System Management Profiles
    removals.insert(DicomTag(0x0008, 0x0014));  // Instance Creator UID
    //removals.insert(DicomTag(0x0008, 0x0018)); // SOP Instance UID => set by ReplaceInstanceInternal()
    removals.insert(DicomTag(0x0008, 0x0050));  // Accession Number
    removals.insert(DicomTag(0x0008, 0x0080));  // Institution Name
    removals.insert(DicomTag(0x0008, 0x0081));  // Institution Address
    removals.insert(DicomTag(0x0008, 0x0090));  // Referring Physician's Name 
    removals.insert(DicomTag(0x0008, 0x0092));  // Referring Physician's Address 
    removals.insert(DicomTag(0x0008, 0x0094));  // Referring Physician's Telephone Numbers 
    removals.insert(DicomTag(0x0008, 0x1010));  // Station Name 
    removals.insert(DicomTag(0x0008, 0x1030));  // Study Description 
    removals.insert(DicomTag(0x0008, 0x103e));  // Series Description 
    removals.insert(DicomTag(0x0008, 0x1040));  // Institutional Department Name 
    removals.insert(DicomTag(0x0008, 0x1048));  // Physician(s) of Record 
    removals.insert(DicomTag(0x0008, 0x1050));  // Performing Physicians' Name 
    removals.insert(DicomTag(0x0008, 0x1060));  // Name of Physician(s) Reading Study 
    removals.insert(DicomTag(0x0008, 0x1070));  // Operators' Name 
    removals.insert(DicomTag(0x0008, 0x1080));  // Admitting Diagnoses Description 
    removals.insert(DicomTag(0x0008, 0x1155));  // Referenced SOP Instance UID 
    removals.insert(DicomTag(0x0008, 0x2111));  // Derivation Description 
    removals.insert(DicomTag(0x0010, 0x0010));  // Patient's Name 
    //removals.insert(DicomTag(0x0010, 0x0020));  // Patient ID => cf. below (*)
    removals.insert(DicomTag(0x0010, 0x0030));  // Patient's Birth Date 
    removals.insert(DicomTag(0x0010, 0x0032));  // Patient's Birth Time 
    removals.insert(DicomTag(0x0010, 0x0040));  // Patient's Sex 
    removals.insert(DicomTag(0x0010, 0x1000));  // Other Patient Ids 
    removals.insert(DicomTag(0x0010, 0x1001));  // Other Patient Names 
    removals.insert(DicomTag(0x0010, 0x1010));  // Patient's Age 
    removals.insert(DicomTag(0x0010, 0x1020));  // Patient's Size 
    removals.insert(DicomTag(0x0010, 0x1030));  // Patient's Weight 
    removals.insert(DicomTag(0x0010, 0x1090));  // Medical Record Locator 
    removals.insert(DicomTag(0x0010, 0x2160));  // Ethnic Group 
    removals.insert(DicomTag(0x0010, 0x2180));  // Occupation 
    removals.insert(DicomTag(0x0010, 0x21b0));  // Additional Patient's History 
    removals.insert(DicomTag(0x0010, 0x4000));  // Patient Comments 
    removals.insert(DicomTag(0x0018, 0x1000));  // Device Serial Number 
    removals.insert(DicomTag(0x0018, 0x1030));  // Protocol Name 
    //removals.insert(DicomTag(0x0020, 0x000d));  // Study Instance UID => cf. below (*)
    //removals.insert(DicomTag(0x0020, 0x000e));  // Series Instance UID => cf. below (*)
    removals.insert(DicomTag(0x0020, 0x0010));  // Study ID 
    removals.insert(DicomTag(0x0020, 0x0052));  // Frame of Reference UID 
    removals.insert(DicomTag(0x0020, 0x0200));  // Synchronization Frame of Reference UID 
    removals.insert(DicomTag(0x0020, 0x4000));  // Image Comments 
    removals.insert(DicomTag(0x0040, 0x0275));  // Request Attributes Sequence 
    removals.insert(DicomTag(0x0040, 0xa124));  // UID
    removals.insert(DicomTag(0x0040, 0xa730));  // Content Sequence 
    removals.insert(DicomTag(0x0088, 0x0140));  // Storage Media File-set UID 
    removals.insert(DicomTag(0x3006, 0x0024));  // Referenced Frame of Reference UID 
    removals.insert(DicomTag(0x3006, 0x00c2));  // Related Frame of Reference UID 

    /**
     *   (*) Patient ID, Study Instance UID and Series Instance UID
     * are modified by "AnonymizeInstance()" if anonymizing a single
     * instance, or by "RetrieveMappedUid()" if anonymizing a
     * patient/study/series.
     **/


    // Some more removals (from the experience of DICOM files at the CHU of Liege)
    removals.insert(DicomTag(0x0010, 0x1040));  // Patient's Address
    removals.insert(DicomTag(0x0032, 0x1032));  // Requesting Physician
    removals.insert(DicomTag(0x0010, 0x2154));  // PatientTelephoneNumbers
    removals.insert(DicomTag(0x0010, 0x2000));  // Medical Alerts

    // Set the DeidentificationMethod tag
    replacements.insert(std::make_pair(DicomTag(0x0012, 0x0063), "Orthanc " ORTHANC_VERSION " - PS 3.15-2008 Table E.1-1"));

    // Set the PatientIdentityRemoved tag
    replacements.insert(std::make_pair(DicomTag(0x0012, 0x0062), "YES"));
  }


  static bool ParseModifyRequest(Removals& removals,
                                 Replacements& replacements,
                                 bool& removePrivateTags,
                                 const RestApi::PostCall& call)
  {
    removePrivateTags = false;
    Json::Value request;
    if (call.ParseJsonRequest(request) &&
        request.isObject())
    {
      Json::Value removalsPart = Json::arrayValue;
      Json::Value replacementsPart = Json::objectValue;

      if (request.isMember("Remove"))
      {
        removalsPart = request["Remove"];
      }

      if (request.isMember("Replace"))
      {
        replacementsPart = request["Replace"];
      }

      if (request.isMember("RemovePrivateTags"))
      {
        removePrivateTags = true;
      }
      
      ParseRemovals(removals, removalsPart);
      ParseReplacements(replacements, replacementsPart);

      return true;
    }
    else
    {
      return false;
    }
  }


  static bool ParseAnonymizationRequest(Removals& removals,
                                        Replacements& replacements,
                                        bool& removePrivateTags,
                                        bool& keepPatientId,
                                        RestApi::PostCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    removePrivateTags = true;
    keepPatientId = false;

    Json::Value request;
    if (call.ParseJsonRequest(request) &&
        request.isObject())
    {
      Json::Value keepPart = Json::arrayValue;
      Json::Value removalsPart = Json::arrayValue;
      Json::Value replacementsPart = Json::objectValue;

      if (request.isMember("Keep"))
      {
        keepPart = request["Keep"];
      }

      if (request.isMember("KeepPrivateTags"))
      {
        removePrivateTags = false;
      }

      if (request.isMember("Replace"))
      {
        replacementsPart = request["Replace"];
      }

      Removals toKeep;
      ParseRemovals(toKeep, keepPart);

      SetupAnonymization(removals, replacements);

      for (Removals::iterator it = toKeep.begin(); it != toKeep.end(); ++it)
      {
        if (*it == DICOM_TAG_PATIENT_ID)
        {
          keepPatientId = true;
        }

        removals.erase(*it);
      }

      Removals additionalRemovals;
      ParseRemovals(additionalRemovals, removalsPart);

      for (Removals::iterator it = additionalRemovals.begin(); 
           it != additionalRemovals.end(); ++it)
      {
        removals.insert(*it);
      }     

      ParseReplacements(replacements, replacementsPart);

      // Generate random Patient's Name if none is specified
      if (toKeep.find(DICOM_TAG_PATIENT_NAME) == toKeep.end() &&
          replacements.find(DICOM_TAG_PATIENT_NAME) == replacements.end())
      {
        replacements.insert(std::make_pair(DICOM_TAG_PATIENT_NAME, GeneratePatientName(context)));
      }

      return true;
    }
    else
    {
      return false;
    }
  }


  static void AnonymizeOrModifyInstance(Removals& removals,
                                        Replacements& replacements,
                                        bool removePrivateTags,
                                        RestApi::PostCall& call)
  {
    std::string id = call.GetUriComponent("id", "");

    ServerContext::DicomCacheLocker locker(OrthancRestApi::GetContext(call), id);

    std::auto_ptr<ParsedDicomFile> modified(locker.GetDicom().Clone());
    ReplaceInstanceInternal(*modified, removals, replacements, removePrivateTags);
    modified->Answer(call.GetOutput());
  }


  static bool RetrieveMappedUid(ParsedDicomFile& dicom,
                                DicomRootLevel level,
                                Replacements& replacements,
                                UidMap& uidMap)
  {
    std::auto_ptr<DicomTag> tag;

    switch (level)
    {
      case DicomRootLevel_Series:
        tag.reset(new DicomTag(DICOM_TAG_SERIES_INSTANCE_UID));
        break;

      case DicomRootLevel_Study:
        tag.reset(new DicomTag(DICOM_TAG_STUDY_INSTANCE_UID));
        break;

      case DicomRootLevel_Patient:
        tag.reset(new DicomTag(DICOM_TAG_PATIENT_ID));
        break;

      default:
        throw OrthancException(ErrorCode_InternalError);
    }

    std::string original;
    if (!dicom.GetTagValue(original, *tag))
    {
      throw OrthancException(ErrorCode_InternalError);
    }

    std::string mapped;
    bool isNew;

    UidMap::const_iterator previous = uidMap.find(std::make_pair(level, original));
    if (previous == uidMap.end())
    {
      mapped = FromDcmtkBridge::GenerateUniqueIdentifier(level);
      uidMap.insert(std::make_pair(std::make_pair(level, original), mapped));
      isNew = true;
    }
    else
    {
      mapped = previous->second;
      isNew = false;
    }    

    replacements[*tag] = mapped;
    return isNew;
  }


  static void AnonymizeOrModifyResource(Removals& removals,
                                        Replacements& replacements,
                                        bool removePrivateTags,
                                        bool keepPatientId,
                                        MetadataType metadataType,
                                        ChangeType changeType,
                                        ResourceType resourceType,
                                        RestApi::PostCall& call)
  {
    typedef std::list<std::string> Instances;

    bool isFirst = true;
    Json::Value result(Json::objectValue);

    ServerContext& context = OrthancRestApi::GetContext(call);

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

    UidMap uidMap;
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

      if (isFirst && keepPatientId)
      {
        std::string patientId = originalHasher.GetPatientId();
        uidMap[std::make_pair(DicomRootLevel_Patient, patientId)] = patientId;
      }

      bool isNewSeries = RetrieveMappedUid(original, DicomRootLevel_Series, replacements, uidMap);
      bool isNewStudy = RetrieveMappedUid(original, DicomRootLevel_Study, replacements, uidMap);
      bool isNewPatient = RetrieveMappedUid(original, DicomRootLevel_Patient, replacements, uidMap);


      /**
       * Compute the resulting DICOM instance and store it into the Orthanc store.
       **/

      std::auto_ptr<ParsedDicomFile> modified(original.Clone());
      ReplaceInstanceInternal(*modified, removals, replacements, removePrivateTags);

      std::string modifiedInstance;
      if (context.Store(modifiedInstance, modified->GetDicom()) != StoreStatus_Success)
      {
        LOG(ERROR) << "Error while storing a modified instance " << *it;
        return;
      }


      /**
       * Record metadata information (AnonymizedFrom/ModifiedFrom).
       **/

      DicomInstanceHasher modifiedHasher = modified->GetHasher();

      if (isNewSeries)
      {
        context.GetIndex().SetMetadata(modifiedHasher.HashSeries(), 
                                       metadataType, originalHasher.HashSeries());
      }

      if (isNewStudy)
      {
        context.GetIndex().SetMetadata(modifiedHasher.HashStudy(), 
                                       metadataType, originalHasher.HashStudy());
      }

      if (isNewPatient)
      {
        context.GetIndex().SetMetadata(modifiedHasher.HashPatient(), 
                                       metadataType, originalHasher.HashPatient());
      }

      assert(*it == originalHasher.HashInstance());
      assert(modifiedInstance == modifiedHasher.HashInstance());
      context.GetIndex().SetMetadata(modifiedInstance, metadataType, *it);


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



  static void ModifyInstance(RestApi::PostCall& call)
  {
    Removals removals;
    Replacements replacements;
    bool removePrivateTags;

    if (ParseModifyRequest(removals, replacements, removePrivateTags, call))
    {
      AnonymizeOrModifyInstance(removals, replacements, removePrivateTags, call);
    }
  }


  static void AnonymizeInstance(RestApi::PostCall& call)
  {
    Removals removals;
    Replacements replacements;
    bool removePrivateTags, keepPatientId;

    if (ParseAnonymizationRequest(removals, replacements, removePrivateTags, keepPatientId, call))
    {
      // TODO Handle "keepPatientId"

      // Generate random patient ID if not specified
      if (replacements.find(DICOM_TAG_PATIENT_ID) == replacements.end())
      {
        replacements.insert(std::make_pair(DICOM_TAG_PATIENT_ID, 
                                           FromDcmtkBridge::GenerateUniqueIdentifier(DicomRootLevel_Patient)));
      }

      // Generate random study UID if not specified
      if (replacements.find(DICOM_TAG_STUDY_INSTANCE_UID) == replacements.end())
      {
        replacements.insert(std::make_pair(DICOM_TAG_STUDY_INSTANCE_UID, 
                                           FromDcmtkBridge::GenerateUniqueIdentifier(DicomRootLevel_Study)));
      }

      // Generate random series UID if not specified
      if (replacements.find(DICOM_TAG_SERIES_INSTANCE_UID) == replacements.end())
      {
        replacements.insert(std::make_pair(DICOM_TAG_SERIES_INSTANCE_UID, 
                                           FromDcmtkBridge::GenerateUniqueIdentifier(DicomRootLevel_Series)));
      }

      AnonymizeOrModifyInstance(removals, replacements, removePrivateTags, call);
    }
  }


  static void ModifySeriesInplace(RestApi::PostCall& call)
  {
    Removals removals;
    Replacements replacements;
    bool removePrivateTags;

    if (ParseModifyRequest(removals, replacements, removePrivateTags, call))
    {
      AnonymizeOrModifyResource(removals, replacements, removePrivateTags, true /*keepPatientId*/,
                                MetadataType_ModifiedFrom, ChangeType_ModifiedSeries, 
                                ResourceType_Series, call);
    }
  }


  static void AnonymizeSeriesInplace(RestApi::PostCall& call)
  {
    Removals removals;
    Replacements replacements;
    bool removePrivateTags, keepPatientId;

    if (ParseAnonymizationRequest(removals, replacements, removePrivateTags, keepPatientId, call))
    {
      AnonymizeOrModifyResource(removals, replacements, removePrivateTags, keepPatientId,
                                MetadataType_AnonymizedFrom, ChangeType_AnonymizedSeries, 
                                ResourceType_Series, call);
    }
  }


  static void ModifyStudyInplace(RestApi::PostCall& call)
  {
    Removals removals;
    Replacements replacements;
    bool removePrivateTags;

    if (ParseModifyRequest(removals, replacements, removePrivateTags, call))
    {
      AnonymizeOrModifyResource(removals, replacements, removePrivateTags, true /*keepPatientId*/,
                                MetadataType_ModifiedFrom, ChangeType_ModifiedStudy, 
                                ResourceType_Study, call);
    }
  }


  static void AnonymizeStudyInplace(RestApi::PostCall& call)
  {
    Removals removals;
    Replacements replacements;
    bool removePrivateTags, keepPatientId;

    if (ParseAnonymizationRequest(removals, replacements, removePrivateTags, keepPatientId, call))
    {
      AnonymizeOrModifyResource(removals, replacements, removePrivateTags, keepPatientId,
                                MetadataType_AnonymizedFrom, ChangeType_AnonymizedStudy, 
                                ResourceType_Study, call);
    }
  }


  /*static void ModifyPatientInplace(RestApi::PostCall& call)
  {
    Removals removals;
    Replacements replacements;
    bool removePrivateTags;

    if (ParseModifyRequest(removals, replacements, removePrivateTags, call))
    {
      AnonymizeOrModifyResource(false, removals, replacements, removePrivateTags, 
                                MetadataType_ModifiedFrom, ChangeType_ModifiedPatient, 
                                ResourceType_Patient, call);
    }
    }*/


  static void AnonymizePatientInplace(RestApi::PostCall& call)
  {
    Removals removals;
    Replacements replacements;
    bool removePrivateTags, keepPatientId;

    if (ParseAnonymizationRequest(removals, replacements, removePrivateTags, keepPatientId, call))
    {
      AnonymizeOrModifyResource(removals, replacements, removePrivateTags, keepPatientId,
                                MetadataType_AnonymizedFrom, ChangeType_AnonymizedPatient, 
                                ResourceType_Patient, call);
    }
  }



  void OrthancRestApi::RegisterAnonymizeModify()
  {
    Register("/instances/{id}/modify", ModifyInstance);
    Register("/series/{id}/modify", ModifySeriesInplace);
    Register("/studies/{id}/modify", ModifyStudyInplace);
    //Register("/patients/{id}/modify", ModifyPatientInplace);

    Register("/instances/{id}/anonymize", AnonymizeInstance);
    Register("/series/{id}/anonymize", AnonymizeSeriesInplace);
    Register("/studies/{id}/anonymize", AnonymizeStudyInplace);
    Register("/patients/{id}/anonymize", AnonymizePatientInplace);
  }
}
