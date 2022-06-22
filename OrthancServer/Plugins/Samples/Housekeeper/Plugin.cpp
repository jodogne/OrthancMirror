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


#include "../../../../OrthancFramework/Sources/Compatibility.h"
#include "../Common/OrthancPluginCppWrapper.h"

#include <boost/thread.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/special_defs.hpp>
#include <json/value.h>
#include <json/writer.h>
#include <string.h>
#include <iostream>
#include <algorithm>
#include <map>
#include <list>
#include <time.h>

static int globalPropertyId_ = 0;
static bool force_ = false;
static unsigned int throttleDelay_ = 0;
static std::unique_ptr<boost::thread> workerThread_;
static bool workerThreadShouldStop_ = false;
static bool triggerOnStorageCompressionChange_ = true;
static bool triggerOnMainDicomTagsChange_ = true;
static bool triggerOnUnnecessaryDicomAsJsonFiles_ = true;
static bool triggerOnIngestTranscodingChange_ = true;


struct RunningPeriod
{
  int fromHour_;
  int toHour_;
  int weekday_;

  RunningPeriod(const std::string& weekday, const std::string& period)
  {
    if (weekday == "Monday")
    {
      weekday_ = 1;
    }
    else if (weekday == "Tuesday")
    {
      weekday_ = 2;
    }
    else if (weekday == "Wednesday")
    {
      weekday_ = 3;
    }
    else if (weekday == "Thursday")
    {
      weekday_ = 4;
    }
    else if (weekday == "Friday")
    {
      weekday_ = 5;
    }
    else if (weekday == "Saturday")
    {
      weekday_ = 6;
    }
    else if (weekday == "Sunday")
    {
      weekday_ = 0;
    }
    else
    {
      OrthancPlugins::LogWarning("Housekeeper: invalid schedule: unknown 'day': " + weekday);      
      ORTHANC_PLUGINS_THROW_EXCEPTION(BadFileFormat);
    }

    std::vector<std::string> hours;
    boost::split(hours, period, boost::is_any_of("-"));

    fromHour_ = boost::lexical_cast<int>(hours[0]);
    toHour_ = boost::lexical_cast<int>(hours[1]);
  }

  bool isInPeriod() const
  {
    time_t now = time(NULL);
    tm* nowLocalTime = localtime(&now);

    if (nowLocalTime->tm_wday != weekday_)
    {
      return false;
    }

    if (nowLocalTime->tm_hour >= fromHour_ && nowLocalTime->tm_hour < toHour_)
    {
      return true;
    }

    return false;
  }
};


struct RunningPeriods
{
  std::list<RunningPeriod> runningPeriods_;

  void load(const Json::Value& scheduleConfiguration)
  {
//   "Monday": ["0-6", "20-24"],

    Json::Value::Members names = scheduleConfiguration.getMemberNames();

    for (Json::Value::Members::const_iterator it = names.begin();
      it != names.end(); it++)
    {
      for (Json::Value::ArrayIndex i = 0; i < scheduleConfiguration[*it].size(); i++)
      {
        runningPeriods_.push_back(RunningPeriod(*it, scheduleConfiguration[*it][i].asString()));
      }
    }
  }

  bool isInPeriod()
  {
    if (runningPeriods_.size() == 0)
    {
      return true;  // if no config: always run
    }

    for (std::list<RunningPeriod>::const_iterator it = runningPeriods_.begin();
      it != runningPeriods_.end(); it++)
    {
      if (it->isInPeriod())
      {
        return true;
      }
    }
    return false;
  }
};

RunningPeriods runningPeriods_;


struct DbConfiguration
{
  std::string orthancVersion;
  std::string patientsMainDicomTagsSignature;
  std::string studiesMainDicomTagsSignature;
  std::string seriesMainDicomTagsSignature;
  std::string instancesMainDicomTagsSignature;
  std::string ingestTranscoding;
  bool storageCompressionEnabled;

  DbConfiguration()
  : storageCompressionEnabled(false)
  {
  }

  bool IsDefined() const
  {
    return !orthancVersion.empty();
  }

  void Clear()
  {
    orthancVersion.clear();
    patientsMainDicomTagsSignature.clear();
    studiesMainDicomTagsSignature.clear();
    seriesMainDicomTagsSignature.clear();
    instancesMainDicomTagsSignature.clear();
    ingestTranscoding.clear();
  }

  void ToJson(Json::Value& target)
  {
    if (!IsDefined())
    {
      target = Json::nullValue;
    }
    else
    {
      Json::Value signatures;

      target = Json::objectValue;

      // default main dicom tags signature are the one from Orthanc 1.4.2 (last time the list was changed):
      signatures["Patient"] = patientsMainDicomTagsSignature;
      signatures["Study"] = studiesMainDicomTagsSignature;
      signatures["Series"] = seriesMainDicomTagsSignature;
      signatures["Instance"] = instancesMainDicomTagsSignature;

      target["MainDicomTagsSignature"] = signatures;
      target["OrthancVersion"] = orthancVersion;
      target["StorageCompressionEnabled"] = storageCompressionEnabled;
      target["IngestTranscoding"] = ingestTranscoding;
    }
  }

  void FromJson(Json::Value& source)
  {
    if (!source.isNull())
    {
      orthancVersion = source["OrthancVersion"].asString();

      const Json::Value& signatures = source["MainDicomTagsSignature"];
      patientsMainDicomTagsSignature = signatures["Patient"].asString();
      studiesMainDicomTagsSignature = signatures["Study"].asString();
      seriesMainDicomTagsSignature = signatures["Series"].asString();
      instancesMainDicomTagsSignature = signatures["Instance"].asString();

      storageCompressionEnabled = source["StorageCompressionEnabled"].asBool();
      ingestTranscoding = source["IngestTranscoding"].asString();
    }
  }
};


struct PluginStatus
{
  int statusVersion;
  int64_t lastProcessedChange;
  int64_t lastChangeToProcess;
  boost::posix_time::ptime lastTimeStarted;

  DbConfiguration currentlyProcessingConfiguration; // last configuration being processed (has not reached last change yet)
  DbConfiguration lastProcessedConfiguration;       // last configuration that has been fully processed (till last change)

  PluginStatus()
  : statusVersion(1),
    lastProcessedChange(-1),
    lastChangeToProcess(-1),
    lastTimeStarted(boost::date_time::not_a_date_time)
  {
  }

  void ToJson(Json::Value& target)
  {
    target = Json::objectValue;

    target["Version"] = statusVersion;
    target["LastProcessedChange"] = Json::Value::Int64(lastProcessedChange);
    target["LastChangeToProcess"] = Json::Value::Int64(lastChangeToProcess);
    
    if (lastTimeStarted == boost::date_time::not_a_date_time)
    {
      target["LastTimeStarted"] = Json::Value::null;  
    }
    else
    {
      target["LastTimeStarted"] = boost::posix_time::to_iso_string(lastTimeStarted);
    }

    currentlyProcessingConfiguration.ToJson(target["CurrentlyProcessingConfiguration"]);
    lastProcessedConfiguration.ToJson(target["LastProcessedConfiguration"]);
  }

  void FromJson(Json::Value& source)
  {
    statusVersion = source["Version"].asInt();
    lastProcessedChange = source["LastProcessedChange"].asInt64();
    lastChangeToProcess = source["LastChangeToProcess"].asInt64();
    if (source["LastTimeStarted"].isNull())
    {
      lastTimeStarted = boost::date_time::not_a_date_time;
    }
    else
    {
      lastTimeStarted = boost::posix_time::from_iso_string(source["LastTimeStarted"].asString());
    }

    Json::Value& current = source["CurrentlyProcessingConfiguration"];
    Json::Value& last = source["LastProcessedConfiguration"];

    currentlyProcessingConfiguration.FromJson(current);
    lastProcessedConfiguration.FromJson(last);
  }
};

static PluginStatus pluginStatus_;
static boost::recursive_mutex pluginStatusMutex_;

static void ReadStatusFromDb()
{
  boost::recursive_mutex::scoped_lock lock(pluginStatusMutex_);

  OrthancPlugins::OrthancString globalPropertyContent;

  globalPropertyContent.Assign(OrthancPluginGetGlobalProperty(OrthancPlugins::GetGlobalContext(),
                                                              globalPropertyId_,
                                                              ""));

  if (!globalPropertyContent.IsNullOrEmpty())
  {
    Json::Value jsonStatus;
    globalPropertyContent.ToJson(jsonStatus);
    pluginStatus_.FromJson(jsonStatus);
  }
  else
  {
    // default config
    pluginStatus_.statusVersion = 1;
    pluginStatus_.lastProcessedChange = -1;
    pluginStatus_.lastChangeToProcess = -1;
    pluginStatus_.lastTimeStarted = boost::date_time::not_a_date_time;
    
    pluginStatus_.lastProcessedConfiguration.orthancVersion = "1.9.0"; // when we don't know, we assume some files were stored with Orthanc 1.9.0 (last version saving the dicom-as-json files)

    // default main dicom tags signature are the one from Orthanc 1.4.2 (last time the list was changed):
    pluginStatus_.lastProcessedConfiguration.patientsMainDicomTagsSignature = "0010,0010;0010,0020;0010,0030;0010,0040;0010,1000";
    pluginStatus_.lastProcessedConfiguration.studiesMainDicomTagsSignature = "0008,0020;0008,0030;0008,0050;0008,0080;0008,0090;0008,1030;0020,000d;0020,0010;0032,1032;0032,1060";
    pluginStatus_.lastProcessedConfiguration.seriesMainDicomTagsSignature = "0008,0021;0008,0031;0008,0060;0008,0070;0008,1010;0008,103e;0008,1070;0018,0010;0018,0015;0018,0024;0018,1030;0018,1090;0018,1400;0020,000e;0020,0011;0020,0037;0020,0105;0020,1002;0040,0254;0054,0081;0054,0101;0054,1000";
    pluginStatus_.lastProcessedConfiguration.instancesMainDicomTagsSignature = "0008,0012;0008,0013;0008,0018;0020,0012;0020,0013;0020,0032;0020,0037;0020,0100;0020,4000;0028,0008;0054,1330"; 
  }
}

static void SaveStatusInDb()
{
  boost::recursive_mutex::scoped_lock lock(pluginStatusMutex_);

  Json::Value jsonStatus;
  pluginStatus_.ToJson(jsonStatus);

  Json::StreamWriterBuilder builder;
  builder.settings_["indentation"] = "   ";
  std::string serializedStatus = Json::writeString(builder, jsonStatus);

  OrthancPluginSetGlobalProperty(OrthancPlugins::GetGlobalContext(),
                                 globalPropertyId_,
                                 serializedStatus.c_str());
}

static void GetCurrentDbConfiguration(DbConfiguration& configuration)
{
  Json::Value signatures;
  Json::Value systemInfo;

  OrthancPlugins::RestApiGet(systemInfo, "/system", false);
  configuration.patientsMainDicomTagsSignature = systemInfo["MainDicomTags"]["Patient"].asString();
  configuration.studiesMainDicomTagsSignature = systemInfo["MainDicomTags"]["Study"].asString();
  configuration.seriesMainDicomTagsSignature = systemInfo["MainDicomTags"]["Series"].asString();
  configuration.instancesMainDicomTagsSignature = systemInfo["MainDicomTags"]["Instance"].asString();
  configuration.storageCompressionEnabled = systemInfo["StorageCompression"].asBool();
  configuration.ingestTranscoding = systemInfo["IngestTranscoding"].asString();

  configuration.orthancVersion = OrthancPlugins::GetGlobalContext()->orthancVersion;
}

static void CheckNeedsProcessing(bool& needsReconstruct, bool& needsReingest, const DbConfiguration& current, const DbConfiguration& last)
{
  needsReconstruct = false;
  needsReingest = false;

  if (!last.IsDefined())
  {
    return;
  }

  const char* lastVersion = last.orthancVersion.c_str();

  if (!OrthancPlugins::CheckMinimalVersion(lastVersion, 1, 9, 1))
  {
    if (triggerOnUnnecessaryDicomAsJsonFiles_)
    {
      OrthancPlugins::LogWarning("Housekeeper: your storage might still contain some dicom-as-json files -> will perform housekeeping");
      needsReconstruct = true;  // the default reconstruct removes the dicom-as-json
    }
    else
    {
      OrthancPlugins::LogWarning("Housekeeper: your storage might still contain some dicom-as-json files but the trigger has been disabled");
    }
  }

  if (last.patientsMainDicomTagsSignature != current.patientsMainDicomTagsSignature)
  {
    if (triggerOnMainDicomTagsChange_)
    {
      OrthancPlugins::LogWarning("Housekeeper: Patient main dicom tags have changed, -> will perform housekeeping");
      needsReconstruct = true;
    }
    else
    {
      OrthancPlugins::LogWarning("Housekeeper: Patient main dicom tags have changed but the trigger is disabled");
    }
  }

  if (last.studiesMainDicomTagsSignature != current.studiesMainDicomTagsSignature)
  {
    if (triggerOnMainDicomTagsChange_)
    {
      OrthancPlugins::LogWarning("Housekeeper: Study main dicom tags have changed, -> will perform housekeeping");
      needsReconstruct = true;
    }
    else
    {
      OrthancPlugins::LogWarning("Housekeeper: Study main dicom tags have changed but the trigger is disabled");
    }
  }

  if (last.seriesMainDicomTagsSignature != current.seriesMainDicomTagsSignature)
  {
    if (triggerOnMainDicomTagsChange_)
    {
      OrthancPlugins::LogWarning("Housekeeper: Series main dicom tags have changed, -> will perform housekeeping");
      needsReconstruct = true;
    }
    else
    {
      OrthancPlugins::LogWarning("Housekeeper: Series main dicom tags have changed but the trigger is disabled");
    }
  }

  if (last.instancesMainDicomTagsSignature != current.instancesMainDicomTagsSignature)
  {
    if (triggerOnMainDicomTagsChange_)
    {
      OrthancPlugins::LogWarning("Housekeeper: Instance main dicom tags have changed, -> will perform housekeeping");
      needsReconstruct = true;
    }
    else
    {
      OrthancPlugins::LogWarning("Housekeeper: Instance main dicom tags have changed but the trigger is disabled");
    }
  }

  if (current.storageCompressionEnabled != last.storageCompressionEnabled)
  {
    if (triggerOnStorageCompressionChange_)
    {
      if (current.storageCompressionEnabled)
      {
        OrthancPlugins::LogWarning("Housekeeper: storage compression is now enabled -> will perform housekeeping");
      }
      else
      {
        OrthancPlugins::LogWarning("Housekeeper: storage compression is now disabled -> will perform housekeeping");
      }
      
      needsReingest = true;
    }
    else
    {
      OrthancPlugins::LogWarning("Housekeeper: storage compression has changed but the trigger is disabled");
    }
  }

  if (current.ingestTranscoding != last.ingestTranscoding)
  {
    if (triggerOnIngestTranscodingChange_)
    {
      OrthancPlugins::LogWarning("Housekeeper: ingest transcoding has changed -> will perform housekeeping");
      
      needsReingest = true;
    }
    else
    {
      OrthancPlugins::LogWarning("Housekeeper: ingest transcoding has changed but the trigger is disabled");
    }
  }

}

static bool ProcessChanges(bool needsReconstruct, bool needsReingest, const DbConfiguration& currentDbConfiguration)
{
  Json::Value changes;

  {
    boost::recursive_mutex::scoped_lock lock(pluginStatusMutex_);

    pluginStatus_.currentlyProcessingConfiguration = currentDbConfiguration;

    OrthancPlugins::RestApiGet(changes, "/changes?since=" + boost::lexical_cast<std::string>(pluginStatus_.lastProcessedChange) + "&limit=100", false);
  }

  for (Json::ArrayIndex i = 0; i < changes["Changes"].size(); i++)
  {
    const Json::Value& change = changes["Changes"][i];
    int64_t seq = change["Seq"].asInt64();

    if (change["ChangeType"] == "NewStudy") // some StableStudy might be missing if orthanc was shutdown during a StableAge -> consider only the NewStudy events that can not be missed
    {
      Json::Value result;
      Json::Value request;
      if (needsReingest)
      {
        request["ReconstructFiles"] = true;
      }
      OrthancPlugins::RestApiPost(result, "/studies/" + change["ID"].asString() + "/reconstruct", request, false);
    }

    {
      boost::recursive_mutex::scoped_lock lock(pluginStatusMutex_);

      pluginStatus_.lastProcessedChange = seq;

      if (seq >= pluginStatus_.lastChangeToProcess)  // we are done !
      {
        return true;
      }
    }

    if (change["ChangeType"] == "NewStudy")
    {
      boost::this_thread::sleep(boost::posix_time::milliseconds(throttleDelay_ * 1000));
    }
  }

  return false;
}


static void WorkerThread()
{
  DbConfiguration currentDbConfiguration;

  OrthancPluginLogWarning(OrthancPlugins::GetGlobalContext(), "Starting Housekeeper worker thread");

  ReadStatusFromDb();

  GetCurrentDbConfiguration(currentDbConfiguration);

  bool needsReconstruct = false;
  bool needsReingest = false;
  bool needsFullProcessing = false;
  bool needsProcessing = false;

  {
    boost::recursive_mutex::scoped_lock lock(pluginStatusMutex_);

    // compare with last full processed configuration
    CheckNeedsProcessing(needsReconstruct, needsReingest, currentDbConfiguration, pluginStatus_.lastProcessedConfiguration);
    needsFullProcessing = needsReconstruct || needsReingest;
    needsProcessing = needsFullProcessing;

      // if a processing was in progress, check if the config has changed since
    if (pluginStatus_.currentlyProcessingConfiguration.IsDefined())
    {
      needsProcessing = true;       // since a processing was in progress, we need at least a partial processing

      bool needsReconstruct2 = false;
      bool needsReingest2 = false;

      CheckNeedsProcessing(needsReconstruct2, needsReingest2, currentDbConfiguration, pluginStatus_.currentlyProcessingConfiguration);
      needsFullProcessing = needsReconstruct2 || needsReingest2;  // if the configuration has changed compared to the config being processed, we need a full processing again
    }
  }

  if (!needsProcessing)
  {
    OrthancPlugins::LogWarning("Housekeeper: everything has been processed already !");
    return;
  }

  if (force_ || needsFullProcessing)
  {
    if (force_)
    {
      OrthancPlugins::LogWarning("Housekeeper: forcing execution -> will perform housekeeping");
    }
    else
    {
      OrthancPlugins::LogWarning("Housekeeper: the DB configuration has changed since last run, will reprocess the whole DB !");
    }
    
    Json::Value changes;
    OrthancPlugins::RestApiGet(changes, "/changes?last", false);

    {
      boost::recursive_mutex::scoped_lock lock(pluginStatusMutex_);

      pluginStatus_.lastProcessedChange = 0;
      pluginStatus_.lastChangeToProcess = changes["Last"].asInt64();  // the last change is the last change at the time we start.  We assume that every new ingested file will be constructed correctly
      pluginStatus_.lastTimeStarted = boost::posix_time::microsec_clock::universal_time();
    }
  }
  else
  {
    OrthancPlugins::LogWarning("Housekeeper: the DB configuration has not changed since last run, will continue processing changes");
  }

  bool completed = false;
  {
    boost::recursive_mutex::scoped_lock lock(pluginStatusMutex_);
    completed = pluginStatus_.lastChangeToProcess == 0;  // if the DB is empty at start, no need to process anyting
  }

  bool loggedNotRightPeriodChangeMessage = false;

  while (!workerThreadShouldStop_ && !completed)
  {
    if (runningPeriods_.isInPeriod())
    {
      completed = ProcessChanges(needsReconstruct, needsReingest, currentDbConfiguration);
      SaveStatusInDb();
      
      if (!completed)
      {
        boost::recursive_mutex::scoped_lock lock(pluginStatusMutex_);
    
        OrthancPlugins::LogInfo("Housekeeper: processed changes " + 
                                boost::lexical_cast<std::string>(pluginStatus_.lastProcessedChange) + 
                                " / " + boost::lexical_cast<std::string>(pluginStatus_.lastChangeToProcess));
        
        boost::this_thread::sleep(boost::posix_time::milliseconds(throttleDelay_ * 100));  // wait 1/10 of the delay between changes
      }

      loggedNotRightPeriodChangeMessage = false;
    }
    else
    {
      if (!loggedNotRightPeriodChangeMessage)
      {
        OrthancPlugins::LogInfo("Housekeeper: entering quiet period");
        loggedNotRightPeriodChangeMessage = true;
      }
    }
  }  

  if (completed)
  {
    boost::recursive_mutex::scoped_lock lock(pluginStatusMutex_);

    pluginStatus_.lastProcessedConfiguration = currentDbConfiguration;
    pluginStatus_.currentlyProcessingConfiguration.Clear();

    pluginStatus_.lastProcessedChange = -1;
    pluginStatus_.lastChangeToProcess = -1;
    
    SaveStatusInDb();

    OrthancPluginLogWarning(OrthancPlugins::GetGlobalContext(), "Housekeeper: finished processing all changes");
  }
}

extern "C"
{
  OrthancPluginErrorCode GetPluginStatus(OrthancPluginRestOutput* output,
                                         const char* url,
                                         const OrthancPluginHttpRequest* request)
  {
    if (request->method != OrthancPluginHttpMethod_Get)
    {
      OrthancPlugins::AnswerMethodNotAllowed(output, "GET");
    }
    else
    {
      boost::recursive_mutex::scoped_lock lock(pluginStatusMutex_);

      Json::Value status;
      pluginStatus_.ToJson(status);

      OrthancPlugins::AnswerJson(status, output);
    }

    return OrthancPluginErrorCode_Success;
  }


  OrthancPluginErrorCode OnChangeCallback(OrthancPluginChangeType changeType,
                                          OrthancPluginResourceType resourceType,
                                          const char* resourceId)
  {
    switch (changeType)
    {
      case OrthancPluginChangeType_OrthancStarted:
      {
        workerThread_.reset(new boost::thread(WorkerThread));
        return OrthancPluginErrorCode_Success;
      }
      case OrthancPluginChangeType_OrthancStopped:
      {
        if (workerThread_ && workerThread_->joinable())
        {
          workerThreadShouldStop_ = true;
          workerThread_->join();
        }
      }
      default:
        return OrthancPluginErrorCode_Success;
    }
  }

  ORTHANC_PLUGINS_API int32_t OrthancPluginInitialize(OrthancPluginContext* c)
  {
    OrthancPlugins::SetGlobalContext(c);

    /* Check the version of the Orthanc core */
    if (OrthancPluginCheckVersion(c) == 0)
    {
      OrthancPlugins::ReportMinimalOrthancVersion(ORTHANC_PLUGINS_MINIMAL_MAJOR_NUMBER,
                                                  ORTHANC_PLUGINS_MINIMAL_MINOR_NUMBER,
                                                  ORTHANC_PLUGINS_MINIMAL_REVISION_NUMBER);
      return -1;
    }

    OrthancPlugins::LogWarning("Housekeeper plugin is initializing");
    OrthancPluginSetDescription(c, "Optimizes your DB and storage.");

    OrthancPlugins::OrthancConfiguration orthancConfiguration;

    OrthancPlugins::OrthancConfiguration housekeeper;
    orthancConfiguration.GetSection(housekeeper, "Housekeeper");

    bool enabled = housekeeper.GetBooleanValue("Enable", false);
    if (enabled)
    {
      /*
        {
          "Housekeeper": {
            
            // Enables/disables the plugin
            "Enable": false,

            // the Global Prooperty ID in which the plugin progress
            // is stored.  Must be > 1024 and must not be used by
            // another plugin
            "GlobalPropertyId": 1025,

            // Forces execution even if the plugin did not detect
            // any changes in configuration
            "Force": false,

            // Delay (in seconds) between reconstruction of 2 studies
            // This avoids overloading Orthanc with the housekeeping
            // process and leaves room for other operations.
            "ThrottleDelay": 5,

            // Runs the plugin only at certain period of time.
            // If not specified, the plugin runs all the time
            // Examples: 
            // to run between 0AM and 6AM everyday + every night 
            // from 8PM to 12PM and 24h a day on the weekend:
            // "Schedule": {
            //   "Monday": ["0-6", "20-24"],
            //   "Tuesday": ["0-6", "20-24"],
            //   "Wednesday": ["0-6", "20-24"],
            //   "Thursday": ["0-6", "20-24"],
            //   "Friday": ["0-6", "20-24"],
            //   "Saturday": ["0-24"],
            //   "Sunday": ["0-24"]
            // },

            // configure events that can trigger a housekeeping processing 
            "Triggers" : {
              "StorageCompressionChange": true,
              "MainDicomTagsChange": true,
              "UnnecessaryDicomAsJsonFiles": true
            }

          }
        }
      */


      globalPropertyId_ = housekeeper.GetIntegerValue("GlobalPropertyId", 1025);
      force_ = housekeeper.GetBooleanValue("Force", false);
      throttleDelay_ = housekeeper.GetUnsignedIntegerValue("ThrottleDelay", 5);      

      if (housekeeper.GetJson().isMember("Triggers"))
      {
        triggerOnStorageCompressionChange_ = housekeeper.GetBooleanValue("StorageCompressionChange", true);

        triggerOnMainDicomTagsChange_ = housekeeper.GetBooleanValue("MainDicomTagsChange", true);
        triggerOnUnnecessaryDicomAsJsonFiles_ = housekeeper.GetBooleanValue("UnnecessaryDicomAsJsonFiles", true);
        triggerOnIngestTranscodingChange_ = housekeeper.GetBooleanValue("IngestTranscodingChange", true);
      }

      if (housekeeper.GetJson().isMember("Schedule"))
      {
        runningPeriods_.load(housekeeper.GetJson()["Schedule"]);
      }

      OrthancPluginRegisterOnChangeCallback(c, OnChangeCallback);
      OrthancPluginRegisterRestCallback(c, "/housekeeper/status", GetPluginStatus);   // for bacward compatiblity with version 1.11.0
      OrthancPluginRegisterRestCallback(c, "/plugins/housekeeper/status", GetPluginStatus);
    }
    else
    {
      OrthancPlugins::LogWarning("Housekeeper plugin is disabled by the configuration file");
    }

    return 0;
  }


  ORTHANC_PLUGINS_API void OrthancPluginFinalize()
  {
    OrthancPlugins::LogWarning("Housekeeper plugin is finalizing");
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetName()
  {
    return "housekeeper";
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetVersion()
  {
    return HOUSEKEEPER_VERSION;
  }
}
