/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2025 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2025 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#define MODALITY_WORKLISTS_NAME "worklists"

#include "../../../../OrthancFramework/Sources/Compatibility.h"
#include "../../../../OrthancFramework/Sources/OrthancException.h"
#include "../../../../OrthancFramework/Sources/Logging.h"
#include "../../../../OrthancFramework/Sources/Toolbox.h"
#include "../../../../OrthancFramework/Sources/SystemToolbox.h"
#include "../../../../OrthancFramework/Sources/DicomParsing/ParsedDicomFile.h"
#include "../../../../OrthancFramework/Sources/DicomFormat/DicomPath.h"
#include "../../../../OrthancFramework/Sources/DicomParsing/FromDcmtkBridge.h"
#include "../../../../OrthancFramework/Sources/DicomFormat/DicomInstanceHasher.h"
#include "../Common/OrthancPluginCppWrapper.h"

#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
#include <json/value.h>
#include <string.h>
#include <iostream>
#include <algorithm>

static boost::filesystem::path worklistDirectory_;
static bool filterIssuerAet_ = false;
static unsigned int limitAnswers_ = 0;
static std::unique_ptr<boost::thread> worklistHousekeeperThread_;
static bool worklistHousekeeperThreadShouldStop_ = false;
static bool deleteWorklistsOnStableStudy_ = true;
static unsigned int hkIntervalInSeconds_ = 60;
static unsigned int deleteDelayInHours_ = 24;
static std::unique_ptr<OrthancPlugins::KeyValueStore> worklistsStore_;
static bool setStudyInstanceUidIfMissing_ = true;

enum WorklistStorageType
{
  WorklistStorageType_Folder = 1,
  WorklistStorageType_OrthancDb = 2
};

static WorklistStorageType worklistStorage_ = WorklistStorageType_Folder;

struct Worklist
{
  std::string id_;
  std::string createdAt_;
  std::string dicomContent_;

  Worklist(const std::string& id, 
           const std::string& dicomContent) :
    id_(id),
    createdAt_(Orthanc::SystemToolbox::GetNowIsoString(true)),
    dicomContent_(dicomContent)
  {
  }


  Worklist(const std::string& id, const Json::Value& jsonWl) :
    id_(id)
  {
    createdAt_ = jsonWl["CreatedAt"].asString();
    std::string b64DicomContent = jsonWl["Dicom"].asString();
    Orthanc::Toolbox::DecodeBase64(dicomContent_, b64DicomContent);

  }

  void Serialize(std::string& target) const
  {
    Json::Value t;
    t["CreatedAt"] = createdAt_;
    std::string b64DicomContent;
    Orthanc::Toolbox::EncodeBase64(b64DicomContent, dicomContent_);
    t["Dicom"] = b64DicomContent;
    
    Orthanc::Toolbox::WriteFastJson(target, t);
  }

  bool IsOlderThan(unsigned int delayInHours) const
  {
    boost::posix_time::ptime now(boost::posix_time::from_iso_string(Orthanc::SystemToolbox::GetNowIsoString(true)));
    boost::posix_time::ptime creationDate(boost::posix_time::from_iso_string(createdAt_));

    return (now - creationDate).total_seconds() > (3600 * deleteDelayInHours_);
  }
};

/**
 * This is the main function for matching a DICOM worklist against a query.
 **/
static bool MatchWorklist(OrthancPluginWorklistAnswers*      answers,
                           const OrthancPluginWorklistQuery*  query,
                           const OrthancPlugins::FindMatcher& matcher,
                           const std::string& dicomContent)
{
  OrthancPlugins::MemoryBuffer dicom;
  dicom.Assign(dicomContent);

  if (matcher.IsMatch(dicom))
  {
    // This DICOM file matches the worklist query, add it to the answers
    OrthancPluginErrorCode code = OrthancPluginWorklistAddAnswer
      (OrthancPlugins::GetGlobalContext(), answers, query, dicom.GetData(), dicom.GetSize());

    if (code != OrthancPluginErrorCode_Success)
    {
      ORTHANC_PLUGINS_LOG_ERROR("Error while adding an answer to a worklist request");
      ORTHANC_PLUGINS_THROW_PLUGIN_ERROR_CODE(code);
    }

    return true;
  }

  return false;
}


static OrthancPlugins::FindMatcher* CreateMatcher(const OrthancPluginWorklistQuery* query,
                                                  const char*                       issuerAet)
{
  // Extract the DICOM instance underlying the C-Find query
  OrthancPlugins::MemoryBuffer dicom;
  dicom.GetDicomQuery(query);

  // Convert the DICOM as JSON, and dump it to the user in "--verbose" mode
  Json::Value json;
  dicom.DicomToJson(json, OrthancPluginDicomToJsonFormat_Short,
                    static_cast<OrthancPluginDicomToJsonFlags>(0), 0);

  ORTHANC_PLUGINS_LOG_INFO("Received worklist query from remote modality " +
                           std::string(issuerAet) + ":\n" + json.toStyledString());

  if (!filterIssuerAet_)
  {
    return new OrthancPlugins::FindMatcher(query);
  }
  else
  {
    // Alternative sample showing how to fine-tune an incoming C-Find
    // request, before matching it against the worklist database. The
    // code below will restrict the original DICOM request by
    // requesting the ScheduledStationAETitle to correspond to the AET
    // of the C-Find issuer. This code will make the integration test
    // "test_filter_issuer_aet" succeed (cf. the orthanc-tests repository).

    static const char* SCHEDULED_PROCEDURE_STEP_SEQUENCE = "0040,0100";
    static const char* SCHEDULED_STATION_AETITLE = "0040,0001";
    static const char* PREGNANCY_STATUS = "0010,21c0";

    if (!json.isMember(SCHEDULED_PROCEDURE_STEP_SEQUENCE))
    {
      // Create a ScheduledProcedureStepSequence sequence, with one empty element
      json[SCHEDULED_PROCEDURE_STEP_SEQUENCE] = Json::arrayValue;
      json[SCHEDULED_PROCEDURE_STEP_SEQUENCE].append(Json::objectValue);
    }

    Json::Value& v = json[SCHEDULED_PROCEDURE_STEP_SEQUENCE];

    if (v.type() != Json::arrayValue ||
        v.size() != 1 ||
        v[0].type() != Json::objectValue)
    {
      ORTHANC_PLUGINS_THROW_EXCEPTION(BadFileFormat);
    }

    // Set the ScheduledStationAETitle if none was provided
    if (!v[0].isMember(SCHEDULED_STATION_AETITLE) ||
        v[0].type() != Json::stringValue ||
        v[0][SCHEDULED_STATION_AETITLE].asString().size() == 0 ||
        v[0][SCHEDULED_STATION_AETITLE].asString() == "*")
    {
      v[0][SCHEDULED_STATION_AETITLE] = issuerAet;
    }

    if (json.isMember(PREGNANCY_STATUS) &&
        json[PREGNANCY_STATUS].asString().size() == 0)
    {
      json.removeMember(PREGNANCY_STATUS);
    }

    // Encode the modified JSON as a DICOM instance, then convert it to a C-Find matcher
    OrthancPlugins::MemoryBuffer modified;
    modified.CreateDicom(json, OrthancPluginCreateDicomFlags_None);
    
    return new OrthancPlugins::FindMatcher(modified);
  }
}

static void ListWorklistsFromDb(std::vector<Worklist>& target)
{
  assert(worklistStorage_ == WorklistStorageType_OrthancDb);
  assert(worklistsStore_);
  
  std::unique_ptr<OrthancPlugins::KeyValueStore::Iterator> it(worklistsStore_->CreateIterator());
  
  while (it->Next())
  {
    std::string serialized;
    it->GetValue(serialized);
    
    Json::Value jsonWl;
    if (Orthanc::Toolbox::ReadJson(jsonWl, serialized))
    {
      Worklist wl(it->GetKey(), jsonWl);
      target.push_back(wl);
    }
  };
  
}


static void ListWorklistsFromFolder(std::vector<Worklist>& target)
{
  assert(worklistStorage_ == WorklistStorageType_Folder);

  // Loop over the regular files in the database folder
  namespace fs = boost::filesystem;

  fs::path source = worklistDirectory_;
  fs::directory_iterator end;

  try
  {
    for (fs::directory_iterator it(source); it != end; ++it)
    {
      fs::file_type type(it->status().type());

      if (type == fs::regular_file ||
          type == fs::reparse_file)   // cf. BitBucket issue #11
      {
        std::string extension = it->path().extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), tolower);  // Convert to lowercase

        if (extension == ".wl")
        {
          std::string worklistId = Orthanc::SystemToolbox::PathToUtf8(it->path().filename().replace_extension(""));
          std::string dicomContent;
          Orthanc::SystemToolbox::ReadFile(dicomContent, it->path());

          Worklist wl(worklistId, dicomContent);
          target.push_back(wl);
        }
      }
    }
  }
  catch (fs::filesystem_error&)
  {
    LOG(ERROR) << "Inexistent folder while scanning for worklists: " + source.string();
  }
}


OrthancPluginErrorCode WorklistCallback(OrthancPluginWorklistAnswers*     answers,
                                        const OrthancPluginWorklistQuery* query,
                                        const char*                       issuerAet,
                                        const char*                       calledAet)
{
  try
  {
    unsigned int parsedFilesCount = 0;
    unsigned int matchedWorklistCount = 0;

    // Construct an object to match the worklists in the database against the C-Find query
    std::unique_ptr<OrthancPlugins::FindMatcher> matcher(CreateMatcher(query, issuerAet));

    std::vector<Worklist> worklists;

    if (worklistStorage_ == WorklistStorageType_Folder)
    {
      ListWorklistsFromFolder(worklists);
    }
    else if (worklistStorage_ == WorklistStorageType_OrthancDb)
    {
      ListWorklistsFromDb(worklists);
    }

    for (std::vector<Worklist>::const_iterator it = worklists.begin(); it != worklists.end(); ++it)
    {
      if (MatchWorklist(answers, query, *matcher, it->dicomContent_))
      {
        if (limitAnswers_ != 0 &&
            matchedWorklistCount >= limitAnswers_)
        {
          // Too many answers are to be returned wrt. the
          // "LimitAnswers" configuration parameter. Mark the
          // C-FIND result as incomplete.
          OrthancPluginWorklistMarkIncomplete(OrthancPlugins::GetGlobalContext(), answers);
          return OrthancPluginErrorCode_Success;
        }
        
        LOG(INFO) << "Worklist matched: " << it->id_;
        matchedWorklistCount++;
      }
    }

    LOG(INFO) << "Worklist C-Find: parsed " << boost::lexical_cast<std::string>(parsedFilesCount) <<
                 " worklists, found " << boost::lexical_cast<std::string>(matchedWorklistCount) << " match(es)";


    return OrthancPluginErrorCode_Success;
  }
  catch (OrthancPlugins::PluginException& e)
  {
    return e.GetErrorCode();
  }
}

static void DeleteWorklist(const std::string& worklistId)
{
  switch (worklistStorage_)
  {
    case WorklistStorageType_Folder:
    {
      boost::filesystem::path path = worklistDirectory_ / (worklistId + ".wl");
      if (!Orthanc::SystemToolbox::IsRegularFile(path))
      {
        throw Orthanc::OrthancException(Orthanc::ErrorCode_UnknownResource);
      }
      Orthanc::SystemToolbox::RemoveFile(path);
    }; break;
    case WorklistStorageType_OrthancDb:
    {
      if (worklistsStore_.get() != NULL)
      {
        std::string notUsed;
        if (!worklistsStore_->GetValue(notUsed, worklistId))
        {
          throw Orthanc::OrthancException(Orthanc::ErrorCode_UnknownResource);
        }
        worklistsStore_->DeleteKey(worklistId);
      }
    }; break;
    default:
      throw Orthanc::OrthancException(Orthanc::ErrorCode_NotImplemented);
  }

}

static void WorklistHkWorkerThread()
{
  OrthancPluginSetCurrentThreadName(OrthancPlugins::GetGlobalContext(), "WL HOUSEKEEPER");

  OrthancPluginLogWarning(OrthancPlugins::GetGlobalContext(), "Starting Worklist Housekeeper worker thread");
  Orthanc::Toolbox::ElapsedTimer timer;

  while (!worklistHousekeeperThreadShouldStop_)
  {
    boost::this_thread::sleep(boost::posix_time::milliseconds(1000));

    if (timer.GetElapsedMilliseconds() > (hkIntervalInSeconds_ * 1000))
    {
      timer.Restart();
      LOG(INFO) << "Performing Worklist Housekeeping";

      std::vector<Worklist> worklists;

      if (worklistStorage_ == WorklistStorageType_OrthancDb)
      {
        ListWorklistsFromDb(worklists);
      }
      else if (worklistStorage_ == WorklistStorageType_Folder)
      {
        ListWorklistsFromFolder(worklists);
      }

      for (std::vector<Worklist>::const_iterator it = worklists.begin(); it != worklists.end(); ++it)
      {
        if (deleteDelayInHours_ > 0 && it->IsOlderThan(deleteDelayInHours_))
        {
          LOG(INFO) << "Deleting worklist " << it->id_ << " " << deleteDelayInHours_ << " hours after its creation";
          DeleteWorklist(it->id_);
        }
        else if (deleteWorklistsOnStableStudy_)
        {
          std::string studyInstanceUid;
          std::string patientId;

          Orthanc::ParsedDicomFile parsed(it->dicomContent_);
          
          if (parsed.GetTagValue(studyInstanceUid, Orthanc::DICOM_TAG_STUDY_INSTANCE_UID) &&
              parsed.GetTagValue(patientId, Orthanc::DICOM_TAG_PATIENT_ID))
          {
            Orthanc::DicomInstanceHasher hasher(patientId, studyInstanceUid, "fake-id", "fake-id");
            const std::string& studyOrthancId = hasher.HashStudy();

            Json::Value studyInfo;
            if (OrthancPlugins::RestApiGet(studyInfo, "/studies/" + studyOrthancId, false))
            {
              if (studyInfo["IsStable"].asBool())
              {
                LOG(INFO) << "Deleting worklist " << it->id_ << " because its study is now stable";
                DeleteWorklist(it->id_);
              }
            }
          }
        }
      }
    }
  }
}


static Orthanc::DicomToJsonFormat GetFormat(const OrthancPluginHttpRequest* request)
{
  std::map<std::string, std::string> getArguments;
  OrthancPlugins::GetGetArguments(getArguments, request);

  Orthanc::DicomToJsonFormat format = Orthanc::DicomToJsonFormat_Human;

  if (getArguments.find("format") != getArguments.end())
  {
    format = Orthanc::StringToDicomToJsonFormat(getArguments["format"]);
  }

  return format;
}

static Orthanc::ParsedDicomFile* GetWorklist(const std::string& id)
{
  std::string fileContent;

  switch (worklistStorage_)
  {
    case WorklistStorageType_Folder:
    {
      boost::filesystem::path path = worklistDirectory_ / Orthanc::SystemToolbox::PathFromUtf8(id + ".wl");  // the id might be a filename from a file that was pushed by an external program (therefore, it can contain fancy characters)
      if (!Orthanc::SystemToolbox::IsRegularFile(path))
      {
        throw Orthanc::OrthancException(Orthanc::ErrorCode_UnknownResource, "Worklist not found");
      }

      Orthanc::SystemToolbox::ReadFile(fileContent, path);
    }; break;
    case WorklistStorageType_OrthancDb:
    {
      std::string serializedWl;
      if (!worklistsStore_->GetValue(serializedWl, id))
      {
        throw Orthanc::OrthancException(Orthanc::ErrorCode_UnknownResource, "Worklist not found");
      }

      Json::Value jsonWl;
      if (Orthanc::Toolbox::ReadJson(jsonWl, serializedWl))
      {
        Worklist wl(id, jsonWl);
        fileContent = wl.dicomContent_;
      }
    }; break;
    default:
      throw Orthanc::OrthancException(Orthanc::ErrorCode_NotImplemented);
  }

  return new Orthanc::ParsedDicomFile(fileContent);  
}

static void SerializeWorklistForApi(Json::Value& target, const std::string& id, const Orthanc::ParsedDicomFile& parsed, Orthanc::DicomToJsonFormat format)
{
  target["ID"] = id;
  target["Tags"] = Json::objectValue;
  parsed.DatasetToJson(target["Tags"], format, Orthanc::DicomToJsonFlags_None, 0);
}

extern "C"
{

  OrthancPluginErrorCode ListWorklists(OrthancPluginRestOutput* output,
                                       const char* url,
                                       const OrthancPluginHttpRequest* request)
  {
    if (request->method != OrthancPluginHttpMethod_Get)
    {
      OrthancPlugins::AnswerMethodNotAllowed(output, "GET");
    }
    else
    {
      Json::Value response = Json::arrayValue;
      Orthanc::DicomToJsonFormat format = GetFormat(request);

      std::vector<Worklist> worklists;

      if (worklistStorage_ == WorklistStorageType_Folder)
      {
        ListWorklistsFromFolder(worklists);
      }
      else if (worklistStorage_ == WorklistStorageType_OrthancDb)
      {
        ListWorklistsFromDb(worklists);
      }

      for (std::vector<Worklist>::const_iterator it = worklists.begin(); it != worklists.end(); ++it)
      {
        Orthanc::ParsedDicomFile parsed(it->dicomContent_);
        Json::Value jsonWl;
        SerializeWorklistForApi(jsonWl, it->id_, parsed, format);

        if (worklistStorage_ == WorklistStorageType_OrthancDb)
        {
          jsonWl["CreationDate"] = it->createdAt_;
        }

        response.append(jsonWl);
      }

      OrthancPlugins::AnswerJson(response, output);
    }

    return OrthancPluginErrorCode_Success;
  }


  OrthancPluginErrorCode GetDeleteWorklist(OrthancPluginRestOutput* output,
                                     const char* url,
                                     const OrthancPluginHttpRequest* request)
  {
    std::string worklistId = std::string(request->groups[0]);

    if (request->method == OrthancPluginHttpMethod_Delete)
    {
      DeleteWorklist(worklistId);

      OrthancPlugins::AnswerString("{}", "application/json", output);
    }
    else if (request->method == OrthancPluginHttpMethod_Get)
    {
      Orthanc::DicomToJsonFormat format = GetFormat(request);
      std::unique_ptr<Orthanc::ParsedDicomFile> parsed(GetWorklist(worklistId));

      Json::Value jsonWl;
      SerializeWorklistForApi(jsonWl, worklistId, *parsed, format);
      
      OrthancPlugins::AnswerJson(jsonWl, output);
    }
    else
    {
      OrthancPlugins::AnswerMethodNotAllowed(output, "DELETE,GET");
    }

    return OrthancPluginErrorCode_Success;
  }

  OrthancPluginErrorCode PostCreateWorklist(OrthancPluginRestOutput* output,
                                           const char* url,
                                           const OrthancPluginHttpRequest* request)
  {
    if (request->method != OrthancPluginHttpMethod_Post)
    {
      OrthancPlugins::AnswerMethodNotAllowed(output, "POST");
    }
    else
    {
      Json::Value body;

      if (!OrthancPlugins::ReadJson(body, request->body, request->bodySize))
      {
        throw Orthanc::OrthancException(Orthanc::ErrorCode_BadFileFormat, "A JSON payload was expected");
      }

      if (!body.isMember("Tags") || !body["Tags"].isObject())
      {
        throw Orthanc::OrthancException(Orthanc::ErrorCode_BadFileFormat, "'Tags' field is missing or not a JSON object");
      }

      bool force = body.isMember("Force") && body["Force"].asBool();

      Json::Value& jsonWorklist = body["Tags"];

      std::unique_ptr<Orthanc::ParsedDicomFile> dicom(Orthanc::ParsedDicomFile::CreateFromJson(jsonWorklist, Orthanc::DicomFromJsonFlags_None, ""));      

      if (!force) 
      {
        if (!dicom->HasTag(Orthanc::DICOM_TAG_SCHEDULED_PROCEDURE_STEP_SEQUENCE))
        {
          throw Orthanc::OrthancException(Orthanc::ErrorCode_BadFileFormat, "'Tags' is missing a 'ScheduledProcedureStepSequence'.  Use 'Force': true to bypass this check.");
        }
        Orthanc::DicomMap step;
        if (!dicom->LookupSequenceItem(step, Orthanc::DicomPath::Parse("ScheduledProcedureStepSequence"), 0) || !step.HasTag(Orthanc::DICOM_TAG_MODALITY))
        {
          throw Orthanc::OrthancException(Orthanc::ErrorCode_BadFileFormat, "'ScheduledProcedureStepSequence' is missing a 'Modality'  Use 'Force': true to bypass this check.");
        }
        if (!dicom->LookupSequenceItem(step, Orthanc::DicomPath::Parse("ScheduledProcedureStepSequence"), 0) || !step.HasTag(Orthanc::DICOM_TAG_SCHEDULED_PROCEDURE_STEP_START_DATE))
        {
          throw Orthanc::OrthancException(Orthanc::ErrorCode_BadFileFormat, "'ScheduledProcedureStepSequence' is missing a 'ScheduledProcedureStepStartDate'  Use 'Force': true to bypass this check.");
        }
      }

      dicom->SetIfAbsent(Orthanc::DICOM_TAG_MEDIA_STORAGE_SOP_CLASS_UID, "1.2.276.0.7230010.3.1.0.1");
      
      if (setStudyInstanceUidIfMissing_)
      {
        dicom->SetIfAbsent(Orthanc::DICOM_TAG_STUDY_INSTANCE_UID, Orthanc::FromDcmtkBridge::GenerateUniqueIdentifier(Orthanc::ResourceType_Study));
      }

      std::string worklistId = Orthanc::Toolbox::GenerateUuid();
      std::string dicomContent;
      dicom->SaveToMemoryBuffer(dicomContent);
      
      switch (worklistStorage_)
      {
        case WorklistStorageType_Folder:
        {
          Orthanc::SystemToolbox::WriteFile(dicomContent, worklistDirectory_ / Orthanc::SystemToolbox::PathFromUtf8(worklistId + ".wl"), true);
        }; break;
        case WorklistStorageType_OrthancDb:
        {
          Worklist wl(worklistId, dicomContent);
          std::string serializedWl;
          wl.Serialize(serializedWl);

          worklistsStore_->Store(worklistId, serializedWl);
        }; break;
        default:
          throw Orthanc::OrthancException(Orthanc::ErrorCode_NotImplemented);
      }

      Json::Value response;

      response["ID"] = worklistId;
      response["Path"] = "/worklists/" + worklistId;

      OrthancPlugins::AnswerJson(response, output);
    }

    return OrthancPluginErrorCode_Success;
  }

  static OrthancPluginErrorCode OnChangeCallback(OrthancPluginChangeType changeType, 
                                                OrthancPluginResourceType resourceType, 
                                                const char *resourceId)
  {
    try
    {
      switch (changeType)
      {
        case OrthancPluginChangeType_OrthancStarted:
        {
          Json::Value system;
          bool hasKeyValueStores = OrthancPlugins::RestApiGet(system, "/system", false) && system.isMember("Capabilities") && 
                                   system["Capabilities"].isMember("HasKeyValueStores") && system["Capabilities"]["HasKeyValueStores"].asBool();

          if (worklistStorage_ == WorklistStorageType_OrthancDb && !hasKeyValueStores)
          {
            LOG(ERROR) << "The Orthanc DB plugin does not support Key Value Stores.  It is therefore impossible to store the worklists in Orthanc Database";
            return OrthancPluginErrorCode_IncompatibleConfigurations;
          }                                 

          if (deleteDelayInHours_ > 0 && !hasKeyValueStores)
          {
            LOG(ERROR) << "The Orthanc DB plugin does not support Key Value Stores.  It is therefore impossible to use the \"DeleteWorklistsDelay\" option";
            return OrthancPluginErrorCode_IncompatibleConfigurations;
          }

          if (worklistStorage_ == WorklistStorageType_OrthancDb)
          {
            worklistsStore_.reset(new OrthancPlugins::KeyValueStore("worklists"));
          }

          if (deleteDelayInHours_ > 0 || deleteWorklistsOnStableStudy_)
          {
            worklistHousekeeperThread_.reset(new boost::thread(WorklistHkWorkerThread));
          }
        }; break;
        default:
          break;
      }
    }
    catch (Orthanc::OrthancException& e)
    {
      LOG(ERROR) << "Exception: " << e.What();
    }
    catch (...)
    {
      LOG(ERROR) << "Uncatched native exception";
    }      
    return OrthancPluginErrorCode_Success;
  }

  ORTHANC_PLUGINS_API int32_t OrthancPluginInitialize(OrthancPluginContext* c)
  {
    OrthancPlugins::SetGlobalContext(c, MODALITY_WORKLISTS_NAME);
    Orthanc::Logging::InitializePluginContext(c, MODALITY_WORKLISTS_NAME);
  
    /* Check the version of the Orthanc core */
    if (OrthancPluginCheckVersion(c) == 0)
    {
      OrthancPlugins::ReportMinimalOrthancVersion(ORTHANC_PLUGINS_MINIMAL_MAJOR_NUMBER,
                                                  ORTHANC_PLUGINS_MINIMAL_MINOR_NUMBER,
                                                  ORTHANC_PLUGINS_MINIMAL_REVISION_NUMBER);
      return -1;
    }

    { // init the OrthancFramework
      static const char* const LOCALE = "Locale";
      static const char* const DEFAULT_ENCODING = "DefaultEncoding";

      /**
       * This function is a simplified version of function
       * "Orthanc::OrthancInitialize()" that is executed when starting the
       * Orthanc server.
       **/
      OrthancPlugins::OrthancConfiguration globalConfig;
      Orthanc::InitializeFramework(globalConfig.GetStringValue(LOCALE, ""), false /* loadPrivateDictionary */);

      std::string encoding;
      if (globalConfig.LookupStringValue(encoding, DEFAULT_ENCODING))
      {
        Orthanc::SetDefaultDicomEncoding(Orthanc::StringToEncoding(encoding.c_str()));
      }
      else
      {
        Orthanc::SetDefaultDicomEncoding(Orthanc::ORTHANC_DEFAULT_DICOM_ENCODING);
      }      
    }

    ORTHANC_PLUGINS_LOG_WARNING("Sample worklist plugin is initializing");
    OrthancPluginSetDescription2(c, MODALITY_WORKLISTS_NAME, "Serve DICOM modality worklists from a folder with Orthanc.");

    OrthancPlugins::OrthancConfiguration configuration;

    OrthancPlugins::OrthancConfiguration worklists;
    configuration.GetSection(worklists, "Worklists");

    bool enabled = worklists.GetBooleanValue("Enable", false);
    if (enabled)
    {
      std::string folder;
      if (worklists.LookupStringValue(folder, "Database") || worklists.LookupStringValue(folder, "Directory"))
      {
        if (worklists.GetBooleanValue("SaveInOrthancDatabase", false))
        {
          LOG(ERROR) << "Worklists plugin: you can not set the \"SaveInOrthancDatabase\" configuration to \"true\" once you have configured the \"Directory\" (or former \"Database\") configuration.";
          return -1;
        }
        
        worklistStorage_ = WorklistStorageType_Folder;
        worklistDirectory_ = Orthanc::SystemToolbox::PathFromUtf8(folder);
        LOG(WARNING) << "The database of worklists will be read from folder: " << folder;
      }
      else if (worklists.GetBooleanValue("SaveInOrthancDatabase", false))
      {

        worklistStorage_ = WorklistStorageType_OrthancDb;
        ORTHANC_PLUGINS_LOG_WARNING("The database of worklists will be read from Orthanc Database");
      }
      else
      {
        LOG(ERROR) << "The configuration option \"Worklists.Directory\" must contain a path";
        return -1;
      }

      OrthancPluginRegisterWorklistCallback(OrthancPlugins::GetGlobalContext(), WorklistCallback);

      filterIssuerAet_ = worklists.GetBooleanValue("FilterIssuerAet", false);
      limitAnswers_ = worklists.GetUnsignedIntegerValue("LimitAnswers", 0);

      deleteWorklistsOnStableStudy_ = worklists.GetBooleanValue("DeleteWorklistsOnStableStudy", true);
      hkIntervalInSeconds_ = worklists.GetUnsignedIntegerValue("HousekeepingInterval", 60);
      deleteDelayInHours_ = worklists.GetUnsignedIntegerValue("DeleteWorklistsDelay", 0);
      setStudyInstanceUidIfMissing_ = worklists.GetBooleanValue("SetStudyInstanceUidIfMissing", true);

      if (deleteDelayInHours_ > 0 && worklistStorage_ == WorklistStorageType_Folder)
      {
        LOG(ERROR) << "Worklists plugin: you can not set the \"DeleteWorklistsDelay\" configuration once you have configured the \"Directory\" (or former \"Database\") configuration.  This feature only works once \"SaveInOrthancDatabase\" is set to true.";
        return -1;
      }

      OrthancPluginRegisterOnChangeCallback(OrthancPlugins::GetGlobalContext(), OnChangeCallback);

      OrthancPluginRegisterRestCallback(OrthancPlugins::GetGlobalContext(), "/worklists/create", PostCreateWorklist);
      OrthancPluginRegisterRestCallback(OrthancPlugins::GetGlobalContext(), "/worklists/([^/]+)", GetDeleteWorklist);
      OrthancPluginRegisterRestCallback(OrthancPlugins::GetGlobalContext(), "/worklists", ListWorklists);
    }
    else
    {
      ORTHANC_PLUGINS_LOG_WARNING("Worklist server is disabled by the configuration file");
    }

    return 0;
  }


  ORTHANC_PLUGINS_API void OrthancPluginFinalize()
  {
    ORTHANC_PLUGINS_LOG_WARNING("Sample worklist plugin is finalizing");

    worklistHousekeeperThreadShouldStop_ = true;
    if (worklistHousekeeperThread_.get() != NULL && worklistHousekeeperThread_->joinable())
    {
      worklistHousekeeperThread_->join();
    }
    worklistHousekeeperThread_.reset(NULL);
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetName()
  {
    return MODALITY_WORKLISTS_NAME;
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetVersion()
  {
    return MODALITY_WORKLISTS_VERSION;
  }
}
