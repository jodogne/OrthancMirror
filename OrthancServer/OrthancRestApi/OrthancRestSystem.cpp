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
#include "../../Core/MetricsRegistry.h"
#include "../../Plugins/Engine/OrthancPlugins.h"
#include "../../Plugins/Engine/PluginsManager.h"
#include "../OrthancConfiguration.h"
#include "../ServerContext.h"


static const char* LOG_LEVEL_DEFAULT = "default";
static const char* LOG_LEVEL_VERBOSE = "verbose";
static const char* LOG_LEVEL_TRACE = "trace";


namespace Orthanc
{
  // System information -------------------------------------------------------

  static void ServeRoot(RestApiGetCall& call)
  {
    call.GetOutput().Redirect("app/explorer.html");
  }
 
  static void GetSystemInformation(RestApiGetCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    Json::Value result = Json::objectValue;

    result["ApiVersion"] = ORTHANC_API_VERSION;
    result["Version"] = ORTHANC_VERSION;
    result["DatabaseVersion"] = OrthancRestApi::GetIndex(call).GetDatabaseVersion();
    result["IsHttpServerSecure"] = context.IsHttpServerSecure();  // New in Orthanc 1.5.8

    {
      OrthancConfiguration::ReaderLock lock;
      result["DicomAet"] = lock.GetConfiguration().GetStringParameter("DicomAet", "ORTHANC");
      result["DicomPort"] = lock.GetConfiguration().GetUnsignedIntegerParameter("DicomPort", 4242);
      result["HttpPort"] = lock.GetConfiguration().GetUnsignedIntegerParameter("HttpPort", 8042);
      result["Name"] = lock.GetConfiguration().GetStringParameter("Name", "");
    }

    result["StorageAreaPlugin"] = Json::nullValue;
    result["DatabaseBackendPlugin"] = Json::nullValue;

#if ORTHANC_ENABLE_PLUGINS == 1
    result["PluginsEnabled"] = true;
    const OrthancPlugins& plugins = context.GetPlugins();

    if (plugins.HasStorageArea())
    {
      std::string p = plugins.GetStorageAreaLibrary().GetPath();
      result["StorageAreaPlugin"] = boost::filesystem::canonical(p).string();
    }

    if (plugins.HasDatabaseBackend())
    {
      std::string p = plugins.GetDatabaseBackendLibrary().GetPath();
      result["DatabaseBackendPlugin"] = boost::filesystem::canonical(p).string();     
    }
#else
    result["PluginsEnabled"] = false;
#endif

    call.GetOutput().AnswerJson(result);
  }

  static void GetStatistics(RestApiGetCall& call)
  {
    static const uint64_t MEGA_BYTES = 1024 * 1024;

    uint64_t diskSize, uncompressedSize, countPatients, countStudies, countSeries, countInstances;
    OrthancRestApi::GetIndex(call).GetGlobalStatistics(diskSize, uncompressedSize, countPatients, 
                                                       countStudies, countSeries, countInstances);
    
    Json::Value result = Json::objectValue;
    result["TotalDiskSize"] = boost::lexical_cast<std::string>(diskSize);
    result["TotalUncompressedSize"] = boost::lexical_cast<std::string>(uncompressedSize);
    result["TotalDiskSizeMB"] = static_cast<unsigned int>(diskSize / MEGA_BYTES);
    result["TotalUncompressedSizeMB"] = static_cast<unsigned int>(uncompressedSize / MEGA_BYTES);
    result["CountPatients"] = static_cast<unsigned int>(countPatients);
    result["CountStudies"] = static_cast<unsigned int>(countStudies);
    result["CountSeries"] = static_cast<unsigned int>(countSeries);
    result["CountInstances"] = static_cast<unsigned int>(countInstances);

    call.GetOutput().AnswerJson(result);
  }

  static void GenerateUid(RestApiGetCall& call)
  {
    std::string level = call.GetArgument("level", "");
    if (level == "patient")
    {
      call.GetOutput().AnswerBuffer(FromDcmtkBridge::GenerateUniqueIdentifier(ResourceType_Patient), MimeType_PlainText);
    }
    else if (level == "study")
    {
      call.GetOutput().AnswerBuffer(FromDcmtkBridge::GenerateUniqueIdentifier(ResourceType_Study), MimeType_PlainText);
    }
    else if (level == "series")
    {
      call.GetOutput().AnswerBuffer(FromDcmtkBridge::GenerateUniqueIdentifier(ResourceType_Series), MimeType_PlainText);
    }
    else if (level == "instance")
    {
      call.GetOutput().AnswerBuffer(FromDcmtkBridge::GenerateUniqueIdentifier(ResourceType_Instance), MimeType_PlainText);
    }
  }

  static void ExecuteScript(RestApiPostCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    if (!context.IsExecuteLuaEnabled())
    {
      LOG(ERROR) << "The URI /tools/execute-script is disallowed for security, "
                 << "check your configuration file";
      call.GetOutput().SignalError(HttpStatus_403_Forbidden);
      return;
    }

    std::string result;
    std::string command;
    call.BodyToString(command);

    {
      LuaScripting::Lock lock(context.GetLuaScripting());
      lock.GetLua().Execute(result, command);
    }

    call.GetOutput().AnswerBuffer(result, MimeType_PlainText);
  }

  template <bool UTC>
  static void GetNowIsoString(RestApiGetCall& call)
  {
    call.GetOutput().AnswerBuffer(SystemToolbox::GetNowIsoString(UTC), MimeType_PlainText);
  }


  static void GetDicomConformanceStatement(RestApiGetCall& call)
  {
    std::string statement;
    GetFileResource(statement, EmbeddedResources::DICOM_CONFORMANCE_STATEMENT);
    call.GetOutput().AnswerBuffer(statement, MimeType_PlainText);
  }


  static void GetDefaultEncoding(RestApiGetCall& call)
  {
    Encoding encoding = GetDefaultDicomEncoding();
    call.GetOutput().AnswerBuffer(EnumerationToString(encoding), MimeType_PlainText);
  }


  static void SetDefaultEncoding(RestApiPutCall& call)
  {
    std::string body;
    call.BodyToString(body);

    Encoding encoding = StringToEncoding(body.c_str());

    {
      OrthancConfiguration::WriterLock lock;
      lock.GetConfiguration().SetDefaultEncoding(encoding);
    }

    call.GetOutput().AnswerBuffer(EnumerationToString(encoding), MimeType_PlainText);
  }


  
  // Plugins information ------------------------------------------------------

  static void ListPlugins(RestApiGetCall& call)
  {
    Json::Value v = Json::arrayValue;

    v.append("explorer.js");

    if (OrthancRestApi::GetContext(call).HasPlugins())
    {
#if ORTHANC_ENABLE_PLUGINS == 1
      std::list<std::string> plugins;
      OrthancRestApi::GetContext(call).GetPlugins().GetManager().ListPlugins(plugins);

      for (std::list<std::string>::const_iterator 
             it = plugins.begin(); it != plugins.end(); ++it)
      {
        v.append(*it);
      }
#endif
    }

    call.GetOutput().AnswerJson(v);
  }


  static void GetPlugin(RestApiGetCall& call)
  {
    if (!OrthancRestApi::GetContext(call).HasPlugins())
    {
      return;
    }

#if ORTHANC_ENABLE_PLUGINS == 1
    const PluginsManager& manager = OrthancRestApi::GetContext(call).GetPlugins().GetManager();
    std::string id = call.GetUriComponent("id", "");

    if (manager.HasPlugin(id))
    {
      Json::Value v = Json::objectValue;
      v["ID"] = id;
      v["Version"] = manager.GetPluginVersion(id);

      const OrthancPlugins& plugins = OrthancRestApi::GetContext(call).GetPlugins();
      const char *c = plugins.GetProperty(id.c_str(), _OrthancPluginProperty_RootUri);
      if (c != NULL)
      {
        std::string root = c;
        if (!root.empty())
        {
          // Turn the root URI into a URI relative to "/app/explorer.js"
          if (root[0] == '/')
          {
            root = ".." + root;
          }

          v["RootUri"] = root;
        }
      }

      c = plugins.GetProperty(id.c_str(), _OrthancPluginProperty_Description);
      if (c != NULL)
      {
        v["Description"] = c;
      }

      c = plugins.GetProperty(id.c_str(), _OrthancPluginProperty_OrthancExplorer);
      v["ExtendsOrthancExplorer"] = (c != NULL);

      call.GetOutput().AnswerJson(v);
    }
#endif
  }


  static void GetOrthancExplorerPlugins(RestApiGetCall& call)
  {
    std::string s = "// Extensions to Orthanc Explorer by the registered plugins\n\n";

    if (OrthancRestApi::GetContext(call).HasPlugins())
    {
#if ORTHANC_ENABLE_PLUGINS == 1
      const OrthancPlugins& plugins = OrthancRestApi::GetContext(call).GetPlugins();
      const PluginsManager& manager = plugins.GetManager();

      std::list<std::string> lst;
      manager.ListPlugins(lst);

      for (std::list<std::string>::const_iterator
             it = lst.begin(); it != lst.end(); ++it)
      {
        const char* tmp = plugins.GetProperty(it->c_str(), _OrthancPluginProperty_OrthancExplorer);
        if (tmp != NULL)
        {
          s += "/**\n * From plugin: " + *it + " (version " + manager.GetPluginVersion(*it) + ")\n **/\n\n";
          s += std::string(tmp) + "\n\n";
        }
      }
#endif
    }

    call.GetOutput().AnswerBuffer(s, MimeType_JavaScript);
  }




  // Jobs information ------------------------------------------------------

  static void ListJobs(RestApiGetCall& call)
  {
    bool expand = call.HasArgument("expand");

    Json::Value v = Json::arrayValue;

    std::set<std::string> jobs;
    OrthancRestApi::GetContext(call).GetJobsEngine().GetRegistry().ListJobs(jobs);

    for (std::set<std::string>::const_iterator it = jobs.begin();
         it != jobs.end(); ++it)
    {
      if (expand)
      {
        JobInfo info;
        if (OrthancRestApi::GetContext(call).GetJobsEngine().GetRegistry().GetJobInfo(info, *it))
        {
          Json::Value tmp;
          info.Format(tmp);
          v.append(tmp);
        }
      }
      else
      {
        v.append(*it);
      }
    }
    
    call.GetOutput().AnswerJson(v);
  }

  static void GetJobInfo(RestApiGetCall& call)
  {
    std::string id = call.GetUriComponent("id", "");

    JobInfo info;
    if (OrthancRestApi::GetContext(call).GetJobsEngine().GetRegistry().GetJobInfo(info, id))
    {
      Json::Value json;
      info.Format(json);
      call.GetOutput().AnswerJson(json);
    }
  }


  static void GetJobOutput(RestApiGetCall& call)
  {
    std::string job = call.GetUriComponent("id", "");
    std::string key = call.GetUriComponent("key", "");

    std::string value;
    MimeType mime;
    
    if (OrthancRestApi::GetContext(call).GetJobsEngine().
        GetRegistry().GetJobOutput(value, mime, job, key))
    {
      call.GetOutput().AnswerBuffer(value, mime);
    }
    else
    {
      throw OrthancException(ErrorCode_InexistentItem,
                             "Job has no such output: " + key);
    }
  }


  enum JobAction
  {
    JobAction_Cancel,
    JobAction_Pause,
    JobAction_Resubmit,
    JobAction_Resume
  };

  template <JobAction action>
  static void ApplyJobAction(RestApiPostCall& call)
  {
    std::string id = call.GetUriComponent("id", "");

    bool ok = false;

    switch (action)
    {
      case JobAction_Cancel:
        ok = OrthancRestApi::GetContext(call).GetJobsEngine().GetRegistry().Cancel(id);
        break;

      case JobAction_Pause:
        ok = OrthancRestApi::GetContext(call).GetJobsEngine().GetRegistry().Pause(id);
        break;
 
      case JobAction_Resubmit:
        ok = OrthancRestApi::GetContext(call).GetJobsEngine().GetRegistry().Resubmit(id);
        break;

      case JobAction_Resume:
        ok = OrthancRestApi::GetContext(call).GetJobsEngine().GetRegistry().Resume(id);
        break;

      default:
        throw OrthancException(ErrorCode_InternalError);
    }
    
    if (ok)
    {
      call.GetOutput().AnswerBuffer("{}", MimeType_Json);
    }
  }

  
  static void GetMetricsPrometheus(RestApiGetCall& call)
  {
#if ORTHANC_ENABLE_PLUGINS == 1
    OrthancRestApi::GetContext(call).GetPlugins().RefreshMetrics();
#endif

    static const float MEGA_BYTES = 1024 * 1024;

    ServerContext& context = OrthancRestApi::GetContext(call);

    uint64_t diskSize, uncompressedSize, countPatients, countStudies, countSeries, countInstances;
    context.GetIndex().GetGlobalStatistics(diskSize, uncompressedSize, countPatients, 
                                           countStudies, countSeries, countInstances);

    unsigned int jobsPending, jobsRunning, jobsSuccess, jobsFailed;
    context.GetJobsEngine().GetRegistry().GetStatistics(jobsPending, jobsRunning, jobsSuccess, jobsFailed);

    MetricsRegistry& registry = context.GetMetricsRegistry();
    registry.SetValue("orthanc_disk_size_mb", static_cast<float>(diskSize) / MEGA_BYTES);
    registry.SetValue("orthanc_uncompressed_size_mb", static_cast<float>(diskSize) / MEGA_BYTES);
    registry.SetValue("orthanc_count_patients", static_cast<unsigned int>(countPatients));
    registry.SetValue("orthanc_count_studies", static_cast<unsigned int>(countStudies));
    registry.SetValue("orthanc_count_series", static_cast<unsigned int>(countSeries));
    registry.SetValue("orthanc_count_instances", static_cast<unsigned int>(countInstances));
    registry.SetValue("orthanc_jobs_pending", jobsPending);
    registry.SetValue("orthanc_jobs_running", jobsRunning);
    registry.SetValue("orthanc_jobs_completed", jobsSuccess + jobsFailed);
    registry.SetValue("orthanc_jobs_success", jobsSuccess);
    registry.SetValue("orthanc_jobs_failed", jobsFailed);
    
    std::string s;
    registry.ExportPrometheusText(s);

    call.GetOutput().AnswerBuffer(s, MimeType_PrometheusText);
  }


  static void GetMetricsEnabled(RestApiGetCall& call)
  {
    bool enabled = OrthancRestApi::GetContext(call).GetMetricsRegistry().IsEnabled();
    call.GetOutput().AnswerBuffer(enabled ? "1" : "0", MimeType_PlainText);
  }


  static void PutMetricsEnabled(RestApiPutCall& call)
  {
    bool enabled;

    std::string body;
    call.BodyToString(body);

    if (body == "1")
    {
      enabled = true;
    }
    else if (body == "0")
    {
      enabled = false;
    }
    else
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange,
                             "The HTTP body must be 0 or 1, but found: " + body);
    }

    // Success
    OrthancRestApi::GetContext(call).GetMetricsRegistry().SetEnabled(enabled);
    call.GetOutput().AnswerBuffer("", MimeType_PlainText);
  }


  static void GetLogLevel(RestApiGetCall& call)
  {
    std::string s;
    
    if (Logging::IsTraceLevelEnabled())
    {
      s = LOG_LEVEL_TRACE;
    }
    else if (Logging::IsInfoLevelEnabled())
    {
      s = LOG_LEVEL_VERBOSE;
    }
    else
    {
      s = LOG_LEVEL_DEFAULT;
    }
    
    call.GetOutput().AnswerBuffer(s, MimeType_PlainText);
  }


  static void PutLogLevel(RestApiPutCall& call)
  {
    std::string body;
    call.BodyToString(body);

    if (body == LOG_LEVEL_DEFAULT)
    {
      Logging::EnableInfoLevel(false);
      Logging::EnableTraceLevel(false);
    }
    else if (body == LOG_LEVEL_VERBOSE)
    {
      Logging::EnableInfoLevel(true);
      Logging::EnableTraceLevel(false);
    }
    else if (body == LOG_LEVEL_TRACE)
    {
      Logging::EnableInfoLevel(true);
      Logging::EnableTraceLevel(true);
    }
    else
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange,
                             "The log level must be one of the following values: \"" +
                             std::string(LOG_LEVEL_DEFAULT) + "\", \"" +
                             std::string(LOG_LEVEL_VERBOSE) + "\", of \"" +
                             std::string(LOG_LEVEL_TRACE) + "\"");
    }

    // Success
    LOG(WARNING) << "REST API call has switched the log level to: " << body;
    call.GetOutput().AnswerBuffer("", MimeType_PlainText);
  }


  void OrthancRestApi::RegisterSystem()
  {
    Register("/", ServeRoot);
    Register("/system", GetSystemInformation);
    Register("/statistics", GetStatistics);
    Register("/tools/generate-uid", GenerateUid);
    Register("/tools/execute-script", ExecuteScript);
    Register("/tools/now", GetNowIsoString<true>);
    Register("/tools/now-local", GetNowIsoString<false>);
    Register("/tools/dicom-conformance", GetDicomConformanceStatement);
    Register("/tools/default-encoding", GetDefaultEncoding);
    Register("/tools/default-encoding", SetDefaultEncoding);
    Register("/tools/metrics", GetMetricsEnabled);
    Register("/tools/metrics", PutMetricsEnabled);
    Register("/tools/metrics-prometheus", GetMetricsPrometheus);
    Register("/tools/log-level", GetLogLevel);
    Register("/tools/log-level", PutLogLevel);

    Register("/plugins", ListPlugins);
    Register("/plugins/{id}", GetPlugin);
    Register("/plugins/explorer.js", GetOrthancExplorerPlugins);

    Register("/jobs", ListJobs);
    Register("/jobs/{id}", GetJobInfo);
    Register("/jobs/{id}/cancel", ApplyJobAction<JobAction_Cancel>);
    Register("/jobs/{id}/pause", ApplyJobAction<JobAction_Pause>);
    Register("/jobs/{id}/resubmit", ApplyJobAction<JobAction_Resubmit>);
    Register("/jobs/{id}/resume", ApplyJobAction<JobAction_Resume>);
    Register("/jobs/{id}/{key}", GetJobOutput);
  }
}
