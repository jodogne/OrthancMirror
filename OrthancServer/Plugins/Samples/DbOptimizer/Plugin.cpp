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
#include <json/value.h>
#include <json/writer.h>
#include <string.h>
#include <iostream>
#include <algorithm>
#include <map>

static int globalPropertyId_ = 0;
static bool force_ = false;
static uint throttleDelay_ = 0;
static std::unique_ptr<boost::thread> workerThread_;
static bool workerThreadShouldStop = false;

struct DbConfiguration
{
  std::string orthancVersion;
  std::map<OrthancPluginResourceType, std::string> mainDicomTagsSignature;

  DbConfiguration()
  {
  }

  bool IsDefined() const
  {
    return !orthancVersion.empty() && mainDicomTagsSignature.size() == 4;
  }

  void Clear()
  {
    orthancVersion.clear();
    mainDicomTagsSignature.clear();
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
      signatures["Patient"] = mainDicomTagsSignature[OrthancPluginResourceType_Patient];
      signatures["Study"] = mainDicomTagsSignature[OrthancPluginResourceType_Study];
      signatures["Series"] = mainDicomTagsSignature[OrthancPluginResourceType_Series];
      signatures["Instance"] = mainDicomTagsSignature[OrthancPluginResourceType_Instance];

      target["MainDicomTagsSignature"] = signatures;
      target["OrthancVersion"] = orthancVersion;
    }
  }

  void FromJson(Json::Value& source)
  {
    if (!source.isNull())
    {
      orthancVersion = source["OrthancVersion"].asString();

      const Json::Value& signatures = source["MainDicomTagsSignature"];
      mainDicomTagsSignature[OrthancPluginResourceType_Patient] = signatures["Patient"].asString();
      mainDicomTagsSignature[OrthancPluginResourceType_Study] = signatures["Study"].asString();
      mainDicomTagsSignature[OrthancPluginResourceType_Series] = signatures["Series"].asString();
      mainDicomTagsSignature[OrthancPluginResourceType_Instance] = signatures["Instance"].asString();
    }
  }
};

struct PluginStatus
{
  int statusVersion;
  int64_t lastProcessedChange;
  int64_t lastChangeToProcess;

  DbConfiguration currentlyProcessingConfiguration; // last configuration being processed (has not reached last change yet)
  DbConfiguration lastProcessedConfiguration;       // last configuration that has been fully processed (till last change)

  PluginStatus()
  : statusVersion(1),
    lastProcessedChange(-1),
    lastChangeToProcess(-1)
  {
  }

  void ToJson(Json::Value& target)
  {
    target = Json::objectValue;

    target["Version"] = statusVersion;
    target["LastProcessedChange"] = Json::Value::Int64(lastProcessedChange);
    target["LastChangeToProcess"] = Json::Value::Int64(lastChangeToProcess);

    currentlyProcessingConfiguration.ToJson(target["CurrentlyProcessingConfiguration"]);
    lastProcessedConfiguration.ToJson(target["LastProcessedConfiguration"]);
  }

  void FromJson(Json::Value& source)
  {
    statusVersion = source["Version"].asInt();
    lastProcessedChange = source["LastProcessedChange"].asInt64();
    lastChangeToProcess = source["LastChangeToProcess"].asInt64();

    Json::Value& current = source["CurrentlyProcessingConfiguration"];
    Json::Value& last = source["LastProcessedConfiguration"];

    currentlyProcessingConfiguration.FromJson(current);
    lastProcessedConfiguration.FromJson(last);
  }
};


static void ReadStatusFromDb(PluginStatus& pluginStatus)
{
  OrthancPlugins::OrthancString globalPropertyContent;

  globalPropertyContent.Assign(OrthancPluginGetGlobalProperty(OrthancPlugins::GetGlobalContext(),
                                                              globalPropertyId_,
                                                              ""));

  if (!globalPropertyContent.IsNullOrEmpty())
  {
    Json::Value jsonStatus;
    globalPropertyContent.ToJson(jsonStatus);
    pluginStatus.FromJson(jsonStatus);
  }
  else
  {
    // default config
    pluginStatus.statusVersion = 1;
    pluginStatus.lastProcessedChange = -1;
    pluginStatus.lastChangeToProcess = -1;
    
    pluginStatus.currentlyProcessingConfiguration.orthancVersion = "1.9.0"; // when we don't know, we assume some files were stored with Orthanc 1.9.0 (last version saving the dicom-as-json files)

    // default main dicom tags signature are the one from Orthanc 1.4.2 (last time the list was changed):
    pluginStatus.currentlyProcessingConfiguration.mainDicomTagsSignature[OrthancPluginResourceType_Patient] = "0010,0010;0010,0020;0010,0030;0010,0040;0010,1000";
    pluginStatus.currentlyProcessingConfiguration.mainDicomTagsSignature[OrthancPluginResourceType_Study] = "0008,0020;0008,0030;0008,0050;0008,0080;0008,0090;0008,1030;0020,000d;0020,0010;0032,1032;0032,1060";
    pluginStatus.currentlyProcessingConfiguration.mainDicomTagsSignature[OrthancPluginResourceType_Series] = "0008,0021;0008,0031;0008,0060;0008,0070;0008,1010;0008,103e;0008,1070;0018,0010;0018,0015;0018,0024;0018,1030;0018,1090;0018,1400;0020,000e;0020,0011;0020,0037;0020,0105;0020,1002;0040,0254;0054,0081;0054,0101;0054,1000";
    pluginStatus.currentlyProcessingConfiguration.mainDicomTagsSignature[OrthancPluginResourceType_Instance] = "0008,0012;0008,0013;0008,0018;0020,0012;0020,0013;0020,0032;0020,0037;0020,0100;0020,4000;0028,0008;0054,1330"; 
  }
}

static void SaveStatusInDb(PluginStatus& pluginStatus)
{
  Json::Value jsonStatus;
  pluginStatus.ToJson(jsonStatus);

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
  configuration.mainDicomTagsSignature[OrthancPluginResourceType_Patient] = systemInfo["MainDicomTags"]["Patient"].asString();
  configuration.mainDicomTagsSignature[OrthancPluginResourceType_Study] = systemInfo["MainDicomTags"]["Study"].asString();
  configuration.mainDicomTagsSignature[OrthancPluginResourceType_Series] = systemInfo["MainDicomTags"]["Series"].asString();
  configuration.mainDicomTagsSignature[OrthancPluginResourceType_Instance] = systemInfo["MainDicomTags"]["Instance"].asString();

  configuration.orthancVersion = OrthancPlugins::GetGlobalContext()->orthancVersion;
}

static bool NeedsProcessing(const DbConfiguration& current, const DbConfiguration& last)
{
  if (!last.IsDefined())
  {
    return true;
  }

  const char* lastVersion = last.orthancVersion.c_str();
  const std::map<OrthancPluginResourceType, std::string>& lastTags = last.mainDicomTagsSignature;
  const std::map<OrthancPluginResourceType, std::string>& currentTags = current.mainDicomTagsSignature;
  bool needsProcessing = false;

  if (!OrthancPlugins::CheckMinimalVersion(lastVersion, 1, 9, 1))
  {
    OrthancPlugins::LogWarning("DbOptimizer: your storage might still contain some dicom-as-json files -> will reconstruct DB");
    needsProcessing = true;
  }

  if (lastTags.at(OrthancPluginResourceType_Patient) != currentTags.at(OrthancPluginResourceType_Patient))
  {
    OrthancPlugins::LogWarning("DbOptimizer: Patient main dicom tags have changed, -> will reconstruct DB");
    needsProcessing = true;
  }

  if (lastTags.at(OrthancPluginResourceType_Study) != currentTags.at(OrthancPluginResourceType_Study))
  {
    OrthancPlugins::LogWarning("DbOptimizer: Study main dicom tags have changed, -> will reconstruct DB");
    needsProcessing = true;
  }

  if (lastTags.at(OrthancPluginResourceType_Series) != currentTags.at(OrthancPluginResourceType_Series))
  {
    OrthancPlugins::LogWarning("DbOptimizer: Series main dicom tags have changed, -> will reconstruct DB");
    needsProcessing = true;
  }

  if (lastTags.at(OrthancPluginResourceType_Instance) != currentTags.at(OrthancPluginResourceType_Instance))
  {
    OrthancPlugins::LogWarning("DbOptimizer: Instance main dicom tags have changed, -> will reconstruct DB");
    needsProcessing = true;
  }

  return needsProcessing;
}

static bool ProcessChanges(PluginStatus& pluginStatus, const DbConfiguration& currentDbConfiguration)
{
  Json::Value changes;

  pluginStatus.currentlyProcessingConfiguration = currentDbConfiguration;

  OrthancPlugins::RestApiGet(changes, "/changes?since=" + boost::lexical_cast<std::string>(pluginStatus.lastProcessedChange) + "&limit=100", false);

  for (Json::ArrayIndex i = 0; i < changes["Changes"].size(); i++)
  {
    const Json::Value& change = changes["Changes"][i];
    int64_t seq = change["Seq"].asInt64();

    if (change["ChangeType"] == "NewStudy") // some StableStudy might be missing if orthanc was shutdown during a StableAge -> consider only the NewStudy events that can not be missed
    {
      Json::Value result;
      OrthancPlugins::RestApiPost(result, "/studies/" + change["ID"].asString() + "/reconstruct", std::string(""), false);
      boost::this_thread::sleep(boost::posix_time::milliseconds(throttleDelay_*1000));
    }

    if (seq >= pluginStatus.lastChangeToProcess)  // we are done !
    {
      return true;
    }

    pluginStatus.lastProcessedChange = seq;
  }

  return false;
}


static void WorkerThread()
{
  PluginStatus pluginStatus;
  DbConfiguration currentDbConfiguration;

  OrthancPluginLogWarning(OrthancPlugins::GetGlobalContext(), "Starting DB optimizer worker thread");

  ReadStatusFromDb(pluginStatus);
  GetCurrentDbConfiguration(currentDbConfiguration);

  if (!NeedsProcessing(currentDbConfiguration, pluginStatus.lastProcessedConfiguration))
  {
    OrthancPlugins::LogWarning("DbOptimizer: everything has been processed already !");
    return;
  }

  if (force_ || NeedsProcessing(currentDbConfiguration, pluginStatus.currentlyProcessingConfiguration))
  {
    if (force_)
    {
      OrthancPlugins::LogWarning("DbOptimizer: forcing execution -> will reconstruct DB");
    }
    else
    {
      OrthancPlugins::LogWarning("DbOptimizer: the DB configuration has changed since last run, will reprocess the whole DB !");
    }
    
    Json::Value changes;
    OrthancPlugins::RestApiGet(changes, "/changes?last", false);

    pluginStatus.lastProcessedChange = 0;
    pluginStatus.lastChangeToProcess = changes["Last"].asInt64();  // the last change is the last change at the time we start.  We assume that every new ingested file will be constructed correctly
  }
  else
  {
    OrthancPlugins::LogWarning("DbOptimizer: the DB configuration has not changed since last run, will continue processing changes");
  }

  bool completed = pluginStatus.lastChangeToProcess == 0;  // if the DB is empty at start, no need to process anyting
  while (!workerThreadShouldStop && !completed)
  {
    completed = ProcessChanges(pluginStatus, currentDbConfiguration);
    SaveStatusInDb(pluginStatus);
    
    if (!completed)
    {
      OrthancPlugins::LogInfo("DbOptimizer: processed changes " + 
                              boost::lexical_cast<std::string>(pluginStatus.lastProcessedChange) + 
                              " / " + boost::lexical_cast<std::string>(pluginStatus.lastChangeToProcess));
      
      boost::this_thread::sleep(boost::posix_time::milliseconds(throttleDelay_*100));  // wait 1/10 of the delay between changes
    }
  }  

  if (completed)
  {
    pluginStatus.lastProcessedConfiguration = currentDbConfiguration;
    pluginStatus.currentlyProcessingConfiguration.Clear();

    pluginStatus.lastProcessedChange = -1;
    pluginStatus.lastChangeToProcess = -1;
    
    SaveStatusInDb(pluginStatus);

    OrthancPluginLogWarning(OrthancPlugins::GetGlobalContext(), "DbOptimizer: finished processing all changes");
  }
}

extern "C"
{
  OrthancPluginErrorCode OnChangeCallback(OrthancPluginChangeType changeType,
                                          OrthancPluginResourceType resourceType,
                                          const char* resourceId)
  {
    switch (changeType)
    {
      case OrthancPluginChangeType_OrthancStarted:
      {
        OrthancPluginLogWarning(OrthancPlugins::GetGlobalContext(), "Starting DB Optmizer worker thread");
        workerThread_.reset(new boost::thread(WorkerThread));
        return OrthancPluginErrorCode_Success;
      }
      case OrthancPluginChangeType_OrthancStopped:
      {
        if (workerThread_ && workerThread_->joinable())
        {
          workerThreadShouldStop = true;
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

    OrthancPlugins::LogWarning("DB Optimizer plugin is initializing");
    OrthancPluginSetDescription(c, "Optimizes your DB and storage.");

    OrthancPlugins::OrthancConfiguration configuration;

    OrthancPlugins::OrthancConfiguration dbOptimizer;
    configuration.GetSection(dbOptimizer, "DbOptimizer");

    bool enabled = dbOptimizer.GetBooleanValue("Enable", false);
    if (enabled)
    {
      globalPropertyId_ = dbOptimizer.GetIntegerValue("GlobalPropertyId", 1025);
      force_ = dbOptimizer.GetBooleanValue("Force", false);
      throttleDelay_ = dbOptimizer.GetUnsignedIntegerValue("ThrottleDelay", 0);      
      OrthancPluginRegisterOnChangeCallback(c, OnChangeCallback);
    }
    else
    {
      OrthancPlugins::LogWarning("DB Optimizer plugin is disabled by the configuration file");
    }

    return 0;
  }


  ORTHANC_PLUGINS_API void OrthancPluginFinalize()
  {
    OrthancPlugins::LogWarning("DB Optimizer plugin is finalizing");
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetName()
  {
    return "db-optimizer";
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetVersion()
  {
    return DB_OPTIMIZER_VERSION;
  }
}
