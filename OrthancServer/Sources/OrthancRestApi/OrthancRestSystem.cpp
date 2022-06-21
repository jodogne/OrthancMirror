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


#include "../PrecompiledHeadersServer.h"
#include "OrthancRestApi.h"

#include "../../../OrthancFramework/Sources/DicomParsing/FromDcmtkBridge.h"
#include "../../../OrthancFramework/Sources/MetricsRegistry.h"
#include "../../Plugins/Engine/OrthancPlugins.h"
#include "../../Plugins/Engine/PluginsManager.h"
#include "../OrthancConfiguration.h"
#include "../OrthancInitialization.h"
#include "../ServerContext.h"

#include <boost/algorithm/string/predicate.hpp>


namespace Orthanc
{
  // System information -------------------------------------------------------

  static void ServeRoot(RestApiGetCall& call)
  {
    call.GetOutput().Redirect("app/explorer.html");
  }
 
  static void ServeFavicon(RestApiGetCall& call)
  {
    call.GetOutput().Redirect("app/images/favicon.ico");
  }

  static void GetMainDicomTagsConfiguration(Json::Value& result)
  {
      Json::Value v;
      
      result["Patient"] = DicomMap::GetMainDicomTagsSignature(ResourceType_Patient);
      result["Study"] = DicomMap::GetMainDicomTagsSignature(ResourceType_Study);
      result["Series"] = DicomMap::GetMainDicomTagsSignature(ResourceType_Series);
      result["Instance"] = DicomMap::GetMainDicomTagsSignature(ResourceType_Instance);
  }

  static void GetSystemInformation(RestApiGetCall& call)
  {
    static const char* const API_VERSION = "ApiVersion";
    static const char* const CHECK_REVISIONS = "CheckRevisions";
    static const char* const DATABASE_BACKEND_PLUGIN = "DatabaseBackendPlugin";
    static const char* const DATABASE_VERSION = "DatabaseVersion";
    static const char* const DATABASE_SERVER_IDENTIFIER = "DatabaseServerIdentifier";
    static const char* const DICOM_AET = "DicomAet";
    static const char* const DICOM_PORT = "DicomPort";
    static const char* const HTTP_PORT = "HttpPort";
    static const char* const IS_HTTP_SERVER_SECURE = "IsHttpServerSecure";
    static const char* const NAME = "Name";
    static const char* const PLUGINS_ENABLED = "PluginsEnabled";
    static const char* const STORAGE_AREA_PLUGIN = "StorageAreaPlugin";
    static const char* const VERSION = "Version";
    static const char* const MAIN_DICOM_TAGS = "MainDicomTags";
    static const char* const STORAGE_COMPRESSION = "StorageCompression";
    static const char* const OVERWRITE_INSTANCES = "OverwriteInstances";
    static const char* const INGEST_TRANSCODING = "IngestTranscoding";
    
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("System")
        .SetSummary("Get system information")
        .SetDescription("Get system information about Orthanc")
        .SetAnswerField(API_VERSION, RestApiCallDocumentation::Type_Number, "Version of the REST API")
        .SetAnswerField(VERSION, RestApiCallDocumentation::Type_String, "Version of Orthanc")
        .SetAnswerField(DATABASE_VERSION, RestApiCallDocumentation::Type_Number,
                        "Version of the database: https://book.orthanc-server.com/developers/db-versioning.html")
        .SetAnswerField(DATABASE_SERVER_IDENTIFIER, RestApiCallDocumentation::Type_String,
                        "ID of the server in the database (when running multiple Orthanc on the same DB)")
        .SetAnswerField(IS_HTTP_SERVER_SECURE, RestApiCallDocumentation::Type_Boolean,
                        "Whether the REST API is properly secured (assuming no reverse proxy is in use): https://book.orthanc-server.com/faq/security.html#securing-the-http-server")
        .SetAnswerField(STORAGE_AREA_PLUGIN, RestApiCallDocumentation::Type_String,
                        "Information about the installed storage area plugin (`null` if no such plugin is installed)")
        .SetAnswerField(DATABASE_BACKEND_PLUGIN, RestApiCallDocumentation::Type_String,
                        "Information about the installed database index plugin (`null` if no such plugin is installed)")
        .SetAnswerField(DICOM_AET, RestApiCallDocumentation::Type_String, "The DICOM AET of Orthanc")
        .SetAnswerField(DICOM_PORT, RestApiCallDocumentation::Type_Number, "The port to the DICOM server of Orthanc")
        .SetAnswerField(HTTP_PORT, RestApiCallDocumentation::Type_Number, "The port to the HTTP server of Orthanc")
        .SetAnswerField(NAME, RestApiCallDocumentation::Type_String,
                        "The name of the Orthanc server, cf. the `Name` configuration option")
        .SetAnswerField(PLUGINS_ENABLED, RestApiCallDocumentation::Type_Boolean,
                        "Whether Orthanc was built with support for plugins")
        .SetAnswerField(CHECK_REVISIONS, RestApiCallDocumentation::Type_Boolean,
                        "Whether Orthanc handle revisions of metadata and attachments to deal with multiple writers (new in Orthanc 1.9.2)")
        .SetAnswerField(MAIN_DICOM_TAGS, RestApiCallDocumentation::Type_JsonObject,
                        "The list of MainDicomTags saved in DB for each resource level (new in Orthanc 1.11.0)")
        .SetAnswerField(STORAGE_COMPRESSION, RestApiCallDocumentation::Type_Boolean,
                        "Whether storage compression is enabled (new in Orthanc 1.11.0)")
        .SetAnswerField(OVERWRITE_INSTANCES, RestApiCallDocumentation::Type_Boolean,
                        "Whether instances are overwritten when re-ingested (new in Orthanc 1.11.0)")
        .SetAnswerField(INGEST_TRANSCODING, RestApiCallDocumentation::Type_String,
                        "Whether instances are transcoded when ingested into Orthanc (`""` if no transcoding is performed) (new in Orthanc 1.11.0)")
        .SetHttpGetSample("https://demo.orthanc-server.com/system", true);
      return;
    }

    ServerContext& context = OrthancRestApi::GetContext(call);

    Json::Value result = Json::objectValue;

    result[API_VERSION] = ORTHANC_API_VERSION;
    result[VERSION] = ORTHANC_VERSION;
    result[DATABASE_VERSION] = OrthancRestApi::GetIndex(call).GetDatabaseVersion();
    result[IS_HTTP_SERVER_SECURE] = context.IsHttpServerSecure();  // New in Orthanc 1.5.8

    {
      OrthancConfiguration::ReaderLock lock;
      result[DICOM_AET] = lock.GetConfiguration().GetOrthancAET();
      result[DICOM_PORT] = lock.GetConfiguration().GetUnsignedIntegerParameter(DICOM_PORT, 4242);
      result[HTTP_PORT] = lock.GetConfiguration().GetUnsignedIntegerParameter(HTTP_PORT, 8042);
      result[NAME] = lock.GetConfiguration().GetStringParameter(NAME, "");
      result[CHECK_REVISIONS] = lock.GetConfiguration().GetBooleanParameter(CHECK_REVISIONS, false);  // New in Orthanc 1.9.2
      result[STORAGE_COMPRESSION] = lock.GetConfiguration().GetBooleanParameter(STORAGE_COMPRESSION, false); // New in Orthanc 1.11.0
      result[OVERWRITE_INSTANCES] = lock.GetConfiguration().GetBooleanParameter(OVERWRITE_INSTANCES, false); // New in Orthanc 1.11.0
      result[INGEST_TRANSCODING] = lock.GetConfiguration().GetStringParameter(INGEST_TRANSCODING, ""); // New in Orthanc 1.11.0
      result[DATABASE_SERVER_IDENTIFIER] = lock.GetConfiguration().GetDatabaseServerIdentifier();
    }

    result[STORAGE_AREA_PLUGIN] = Json::nullValue;
    result[DATABASE_BACKEND_PLUGIN] = Json::nullValue;

#if ORTHANC_ENABLE_PLUGINS == 1
    result[PLUGINS_ENABLED] = true;
    const OrthancPlugins& plugins = context.GetPlugins();

    if (plugins.HasStorageArea())
    {
      std::string p = plugins.GetStorageAreaLibrary().GetPath();
      result[STORAGE_AREA_PLUGIN] = boost::filesystem::canonical(p).string();
    }

    if (plugins.HasDatabaseBackend())
    {
      std::string p = plugins.GetDatabaseBackendLibrary().GetPath();
      result[DATABASE_BACKEND_PLUGIN] = boost::filesystem::canonical(p).string();     
    }
#else
    result[PLUGINS_ENABLED] = false;
#endif

    result[MAIN_DICOM_TAGS] = Json::objectValue;
    GetMainDicomTagsConfiguration(result[MAIN_DICOM_TAGS]);

    call.GetOutput().AnswerJson(result);
  }

  static void GetStatistics(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("System")
        .SetSummary("Get database statistics")
        .SetDescription("Get statistics related to the database of Orthanc")
        .SetAnswerField("CountInstances", RestApiCallDocumentation::Type_Number, "Number of DICOM instances stored in Orthanc")
        .SetAnswerField("CountSeries", RestApiCallDocumentation::Type_Number, "Number of DICOM series stored in Orthanc")
        .SetAnswerField("CountStudies", RestApiCallDocumentation::Type_Number, "Number of DICOM studies stored in Orthanc")
        .SetAnswerField("CountPatients", RestApiCallDocumentation::Type_Number, "Number of patients stored in Orthanc")
        .SetAnswerField("TotalDiskSize", RestApiCallDocumentation::Type_String, "Size of the storage area (in bytes)")
        .SetAnswerField("TotalDiskSizeMB", RestApiCallDocumentation::Type_Number, "Size of the storage area (in megabytes)")
        .SetAnswerField("TotalUncompressedSize", RestApiCallDocumentation::Type_String, "Total size of all the files once uncompressed (in bytes). This corresponds to `TotalDiskSize` if no compression is enabled, cf. `StorageCompression` configuration option")
        .SetAnswerField("TotalUncompressedSizeMB", RestApiCallDocumentation::Type_Number, "Total size of all the files once uncompressed (in megabytes)")
        .SetHttpGetSample("https://demo.orthanc-server.com/statistics", true);
      return;
    }

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
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("System")
        .SetSummary("Generate an identifier")
        .SetDescription("Generate a random DICOM identifier")
        .SetHttpGetArgument("level", RestApiCallDocumentation::Type_String,
                            "Type of DICOM resource among: `patient`, `study`, `series` or `instance`", true)
        .AddAnswerType(MimeType_PlainText, "The generated identifier");
      return;
    }

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
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("System")
        .SetSummary("Execute Lua script")
        .SetDescription("Execute the provided Lua script by the Orthanc server. This is very insecure for "
                        "Orthanc servers that are remotely accessible, cf. configuration option `ExecuteLuaEnabled`")
        .AddRequestType(MimeType_PlainText, "The Lua script to be executed")
        .AddAnswerType(MimeType_PlainText, "Output of the Lua script");
      return;
    }

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
    if (call.IsDocumentation())
    {
      std::string type(UTC ? "UTC" : "local");
      call.GetDocumentation()
        .SetTag("System")
        .SetSummary("Get " + type + " time")
        .AddAnswerType(MimeType_PlainText, "The " + type + " time")
        .SetHttpGetSample("https://demo.orthanc-server.com" + call.FlattenUri(), false);
      return;
    }

    call.GetOutput().AnswerBuffer(SystemToolbox::GetNowIsoString(UTC), MimeType_PlainText);
  }


  static void GetDicomConformanceStatement(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("System")
        .SetSummary("Get DICOM conformance")
        .SetDescription("Get the DICOM conformance statement of Orthanc")
        .AddAnswerType(MimeType_PlainText, "The DICOM conformance statement");
      return;
    }

    std::string statement;
    GetFileResource(statement, ServerResources::DICOM_CONFORMANCE_STATEMENT);
    call.GetOutput().AnswerBuffer(statement, MimeType_PlainText);
  }


  static void GetDefaultEncoding(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("System")
        .SetSummary("Get default encoding")
        .SetDescription("Get the default encoding that is used by Orthanc if parsing "
                        "a DICOM instance without the `SpecificCharacterEncoding` tag, or during C-FIND. "
                        "This corresponds to the configuration option `DefaultEncoding`.")
        .AddAnswerType(MimeType_PlainText, "The name of the encoding");
      return;
    }

    Encoding encoding = GetDefaultDicomEncoding();
    call.GetOutput().AnswerBuffer(EnumerationToString(encoding), MimeType_PlainText);
  }


  static void SetDefaultEncoding(RestApiPutCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("System")
        .SetSummary("Set default encoding")
        .SetDescription("Change the default encoding that is used by Orthanc if parsing "
                        "a DICOM instance without the `SpecificCharacterEncoding` tag, or during C-FIND. "
                        "This corresponds to the configuration option `DefaultEncoding`.")
        .AddRequestType(MimeType_PlainText, "The name of the encoding. Check out configuration "
                        "option `DefaultEncoding` for the allowed values.");
      return;
    }

    std::string body;
    call.BodyToString(body);

    Encoding encoding = StringToEncoding(body.c_str());

    {
      OrthancConfiguration::WriterLock lock;
      lock.GetConfiguration().SetDefaultEncoding(encoding);
    }

    call.GetOutput().AnswerBuffer(EnumerationToString(encoding), MimeType_PlainText);
  }


  static void AnswerAcceptedTransferSyntaxes(RestApiCall& call)
  {
    std::set<DicomTransferSyntax> syntaxes;
    OrthancRestApi::GetContext(call).GetAcceptedTransferSyntaxes(syntaxes);
    
    Json::Value json = Json::arrayValue;
    for (std::set<DicomTransferSyntax>::const_iterator
           syntax = syntaxes.begin(); syntax != syntaxes.end(); ++syntax)
    {
      json.append(GetTransferSyntaxUid(*syntax));
    }
    
    call.GetOutput().AnswerJson(json);
  }
  

  static void GetAcceptedTransferSyntaxes(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("System")
        .SetSummary("Get accepted transfer syntaxes")
        .SetDescription("Get the list of UIDs of the DICOM transfer syntaxes that are accepted "
                        "by Orthanc C-STORE SCP. This corresponds to the configuration options "
                        "`AcceptedTransferSyntaxes` and `XXXTransferSyntaxAccepted`.")
        .AddAnswerType(MimeType_Json, "JSON array containing the transfer syntax UIDs");
      return;
    }

    AnswerAcceptedTransferSyntaxes(call);
  }


  static void SetAcceptedTransferSyntaxes(RestApiPutCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("System")
        .SetSummary("Set accepted transfer syntaxes")
        .SetDescription("Set the DICOM transfer syntaxes that accepted by Orthanc C-STORE SCP")
        .AddRequestType(MimeType_PlainText, "UID of the transfer syntax to be accepted. "
                        "Wildcards `?` and `*` are accepted.")
        .AddRequestType(MimeType_Json, "JSON array containing a list of transfer syntax "
                        "UIDs to be accepted. Wildcards `?` and `*` are accepted.")
        .AddAnswerType(MimeType_Json, "JSON array containing the now-accepted transfer syntax UIDs");
      return;
    }

    std::set<DicomTransferSyntax> syntaxes;

    Json::Value json;
    if (call.ParseJsonRequest(json))
    {
      OrthancConfiguration::ParseAcceptedTransferSyntaxes(syntaxes, json);
    }
    else
    {
      std::string body;
      call.BodyToString(body);
      OrthancConfiguration::ParseAcceptedTransferSyntaxes(syntaxes, body);
    }

    OrthancRestApi::GetContext(call).SetAcceptedTransferSyntaxes(syntaxes);
    
    AnswerAcceptedTransferSyntaxes(call);
  }


  static void GetUnknownSopClassAccepted(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("System")
        .SetSummary("Is unknown SOP class accepted?")
        .SetDescription("Shall Orthanc C-STORE SCP accept DICOM instances with an unknown SOP class UID?")
        .AddAnswerType(MimeType_PlainText, "`1` if accepted, `0` if not accepted");
      return;
    }

    const bool accepted = OrthancRestApi::GetContext(call).IsUnknownSopClassAccepted();
    call.GetOutput().AnswerBuffer(accepted ? "1" : "0", MimeType_PlainText);
  }


  static void SetUnknownSopClassAccepted(RestApiPutCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("System")
        .SetSummary("Set unknown SOP class accepted")
        .SetDescription("Set whether Orthanc C-STORE SCP should accept DICOM instances with an unknown SOP class UID")
        .AddRequestType(MimeType_PlainText, "`1` if accepted, `0` if not accepted");
      return;
    }

    OrthancRestApi::GetContext(call).SetUnknownSopClassAccepted(call.ParseBooleanBody());
    call.GetOutput().AnswerBuffer("", MimeType_PlainText);
  }


  
  // Plugins information ------------------------------------------------------

  static void ListPlugins(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("System")
        .SetSummary("List plugins")
        .SetDescription("List all the installed plugins")
        .AddAnswerType(MimeType_Json, "JSON array containing the identifiers of the installed plugins")
        .SetHttpGetSample("https://demo.orthanc-server.com/plugins", true);
      return;
    }

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
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("System")
        .SetSummary("Get plugin")
        .SetDescription("Get system information about the plugin whose identifier is provided in the URL")
        .SetUriArgument("id", "Identifier of the job of interest")
        .AddAnswerType(MimeType_Json, "JSON object containing information about the plugin")
        .SetHttpGetSample("https://demo.orthanc-server.com/plugins/dicom-web", true);
      return;
    }

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
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("System")
        .SetSummary("JavaScript extensions to Orthanc Explorer")
        .SetDescription("Get the JavaScript extensions that are installed by all the plugins using the "
                        "`OrthancPluginExtendOrthancExplorer()` function of the plugin SDK. "
                        "This route is for internal use of Orthanc Explorer.")
        .AddAnswerType(MimeType_JavaScript, "The JavaScript extensions");
      return;
    }

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
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Jobs")
        .SetSummary("List jobs")
        .SetDescription("List all the available jobs")
        .SetHttpGetArgument("expand", RestApiCallDocumentation::Type_String,
                            "If present, retrieve detailed information about the individual jobs", false)
        .AddAnswerType(MimeType_Json, "JSON array containing either the jobs identifiers, or detailed information "
                       "about the reported jobs (if `expand` argument is provided)")
        .SetTruncatedJsonHttpGetSample("https://demo.orthanc-server.com/jobs", 3);
      return;
    }

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
    if (call.IsDocumentation())
    {
      Json::Value sample;
      sample["CompletionTime"] = "20201227T161842.520129";
      sample["Content"]["ArchiveSizeMB"] = 22;
      sample["Content"]["Description"] = "REST API";
      sample["Content"]["InstancesCount"] = 232;
      sample["Content"]["UncompressedSizeMB"] = 64;
      sample["CreationTime"] = "20201227T161836.428311";
      sample["EffectiveRuntime"] = 6.0810000000000004;
      sample["ErrorCode"] = 0;
      sample["ErrorDescription"] = "Success";
      sample["ID"] = "645ecb02-7c0e-4465-b767-df873222dcfb";
      sample["Priority"] = 0;
      sample["Progress"] = 100;
      sample["State"] = "Success";
      sample["Timestamp"] = "20201228T160340.253201";
      sample["Type"] = "Media";
      
      call.GetDocumentation()
        .SetTag("Jobs")
        .SetSummary("Get job")
        .SetDescription("Retrieve detailed information about the job whose identifier is provided in the URL: "
                        "https://book.orthanc-server.com/users/advanced-rest.html#jobs")
        .SetUriArgument("id", "Identifier of the job of interest")
        .AddAnswerType(MimeType_Json, "JSON object detailing the job")
        .SetSample(sample);
      return;
    }

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
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Jobs")
        .SetSummary("Get job output")
        .SetDescription("Retrieve some output produced by a job. As of Orthanc 1.8.2, only the jobs that generate a "
                        "DICOMDIR media or a ZIP archive provide such an output (with `key` equals to `archive`).")
        .SetUriArgument("id", "Identifier of the job of interest")
        .SetUriArgument("key", "Name of the output of interest")
        .AddAnswerType(MimeType_Binary, "Content of the output of the job");
      return;
    }

    std::string job = call.GetUriComponent("id", "");
    std::string key = call.GetUriComponent("key", "");

    std::string value;
    MimeType mime;
    std::string filename;
    
    if (OrthancRestApi::GetContext(call).GetJobsEngine().
        GetRegistry().GetJobOutput(value, mime, filename, job, key))
    {
      if (!filename.empty())
      {
        call.GetOutput().SetContentFilename(filename.c_str());
      }

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
    if (call.IsDocumentation())
    {
      std::string verb;
      switch (action)
      {
        case JobAction_Cancel:
          verb = "Cancel";
          break;

        case JobAction_Pause:
          verb = "Pause";
          break;
 
        case JobAction_Resubmit:
          verb = "Resubmit";
          break;

        case JobAction_Resume:
          verb = "Resume";
          break;

        default:
          throw OrthancException(ErrorCode_InternalError);
      }      
      
      call.GetDocumentation()
        .SetTag("Jobs")
        .SetSummary(verb + " job")
        .SetDescription(verb + " the job whose identifier is provided in the URL. Check out the "
                        "Orthanc Book for more information about the state machine applicable to jobs: "
                        "https://book.orthanc-server.com/users/advanced-rest.html#jobs")
        .SetUriArgument("id", "Identifier of the job of interest")
        .AddAnswerType(MimeType_Json, "Empty JSON object in the case of a success");
      return;
    }

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
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("System")
        .SetSummary("Get usage metrics")
        .SetDescription("Get usage metrics of Orthanc in the Prometheus file format (OpenMetrics): "
                        "https://book.orthanc-server.com/users/advanced-rest.html#instrumentation-with-prometheus")
        .SetHttpGetSample("https://demo.orthanc-server.com/tools/metrics-prometheus", false);
      return;
    }

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
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("System")
        .SetSummary("Are metrics collected?")
        .SetDescription("Returns a Boolean specifying whether Prometheus metrics "
                        "are collected and exposed at `/tools/metrics-prometheus`")
        .AddAnswerType(MimeType_PlainText, "`1` if metrics are collected, `0` if metrics are disabled");
      return;
    }

    bool enabled = OrthancRestApi::GetContext(call).GetMetricsRegistry().IsEnabled();
    call.GetOutput().AnswerBuffer(enabled ? "1" : "0", MimeType_PlainText);
  }


  static void PutMetricsEnabled(RestApiPutCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("System")
        .SetSummary("Enable collection of metrics")
        .SetDescription("Enable or disable the collection and publication of metrics at `/tools/metrics-prometheus`")
        .AddRequestType(MimeType_PlainText, "`1` if metrics are collected, `0` if metrics are disabled");
      return;
    }

    const bool enabled = call.ParseBooleanBody();

    // Success
    OrthancRestApi::GetContext(call).GetMetricsRegistry().SetEnabled(enabled);
    call.GetOutput().AnswerBuffer("", MimeType_PlainText);
  }


  static void GetLogLevel(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Logs")
        .SetSummary("Get main log level")
        .SetDescription("Get the main log level of Orthanc")
        .AddAnswerType(MimeType_PlainText, "Possible values: `default`, `verbose` or `trace`");
      return;
    }

    const std::string s = EnumerationToString(GetGlobalVerbosity());
    call.GetOutput().AnswerBuffer(s, MimeType_PlainText);
  }


  static void PutLogLevel(RestApiPutCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Logs")
        .SetSummary("Set main log level")
        .SetDescription("Set the main log level of Orthanc")
        .AddRequestType(MimeType_PlainText, "Possible values: `default`, `verbose` or `trace`");
      return;
    }

    std::string body;
    call.BodyToString(body);

    SetGlobalVerbosity(StringToVerbosity(body));
    
    // Success
    LOG(WARNING) << "REST API call has switched the log level to: " << body;
    call.GetOutput().AnswerBuffer("", MimeType_PlainText);
  }


  static Logging::LogCategory GetCategory(const RestApiCall& call)
  {
    static const std::string PREFIX = "log-level-";

    if (call.GetFullUri().size() == 2 &&
        call.GetFullUri() [0] == "tools" &&
        boost::starts_with(call.GetFullUri() [1], PREFIX))
    {
      Logging::LogCategory category;
      if (Logging::LookupCategory(category, call.GetFullUri() [1].substr(PREFIX.size())))
      {
        return category;
      }
    }

    throw OrthancException(ErrorCode_InternalError);
  }
  

  static void GetLogLevelCategory(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      std::string category = Logging::GetCategoryName(GetCategory(call));
      call.GetDocumentation()
        .SetTag("Logs")
        .SetSummary("Get log level for `" + category + "`")
        .SetDescription("Get the log level of the log category `" + category + "`")
        .AddAnswerType(MimeType_PlainText, "Possible values: `default`, `verbose` or `trace`");
      return;
    }

    const std::string s = EnumerationToString(GetCategoryVerbosity(GetCategory(call)));
    call.GetOutput().AnswerBuffer(s, MimeType_PlainText);
  }


  static void PutLogLevelCategory(RestApiPutCall& call)
  {
    if (call.IsDocumentation())
    {
      std::string category = Logging::GetCategoryName(GetCategory(call));
      call.GetDocumentation()
        .SetTag("Logs")
        .SetSummary("Set log level for `" + category + "`")
        .SetDescription("Set the log level of the log category `" + category + "`")
        .AddRequestType(MimeType_PlainText, "Possible values: `default`, `verbose` or `trace`");
      return;
    }

    std::string body;
    call.BodyToString(body);

    Verbosity verbosity = StringToVerbosity(body);
    Logging::LogCategory category = GetCategory(call);
    SetCategoryVerbosity(category, verbosity);
    
    // Success
    LOG(WARNING) << "REST API call has switched the log level of category \""
                 << Logging::GetCategoryName(category) << "\" to \""
                 << EnumerationToString(verbosity) << "\"";
    call.GetOutput().AnswerBuffer("", MimeType_PlainText);
  }


  void OrthancRestApi::RegisterSystem(bool orthancExplorerEnabled)
  {
    if (orthancExplorerEnabled)
    {
      Register("/", ServeRoot);
      Register("/favicon.ico", ServeFavicon);  // New in Orthanc 1.9.0
    }
    
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

    for (size_t i = 0; i < Logging::GetCategoriesCount(); i++)
    {
      const std::string name(Logging::GetCategoryName(i));
      Register("/tools/log-level-" + name, GetLogLevelCategory);
      Register("/tools/log-level-" + name, PutLogLevelCategory);
    }

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

    // New in Orthanc 1.9.0
    Register("/tools/accepted-transfer-syntaxes", GetAcceptedTransferSyntaxes);
    Register("/tools/accepted-transfer-syntaxes", SetAcceptedTransferSyntaxes);
    Register("/tools/unknown-sop-class-accepted", GetUnknownSopClassAccepted);
    Register("/tools/unknown-sop-class-accepted", SetUnknownSopClassAccepted);
  }
}
