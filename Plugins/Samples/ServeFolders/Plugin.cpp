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
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#include "../Common/OrthancPluginCppWrapper.h"

#include <json/reader.h>
#include <json/value.h>
#include <boost/filesystem.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>


#if HAS_ORTHANC_EXCEPTION == 1
#  error The macro HAS_ORTHANC_EXCEPTION must be set to 0 to compile this plugin
#endif



static OrthancPluginContext* context_ = NULL;
static std::map<std::string, std::string> extensions_;
static std::map<std::string, std::string> folders_;
static const char* INDEX_URI = "/app/plugin-serve-folders.html";
static bool allowCache_ = false;
static bool generateETag_ = true;


static void SetHttpHeaders(OrthancPluginRestOutput* output)
{
  if (!allowCache_)
  {
    // http://stackoverflow.com/a/2068407/881731
    OrthancPluginSetHttpHeader(context_, output, "Cache-Control", "no-cache, no-store, must-revalidate");
    OrthancPluginSetHttpHeader(context_, output, "Pragma", "no-cache");
    OrthancPluginSetHttpHeader(context_, output, "Expires", "0");
  }
}


static void RegisterDefaultExtensions()
{
  extensions_["css"]  = "text/css";
  extensions_["gif"]  = "image/gif";
  extensions_["html"] = "text/html";
  extensions_["jpeg"] = "image/jpeg";
  extensions_["jpg"]  = "image/jpeg";
  extensions_["js"]   = "application/javascript";
  extensions_["json"] = "application/json";
  extensions_["nexe"] = "application/x-nacl";
  extensions_["nmf"]  = "application/json";
  extensions_["pexe"] = "application/x-pnacl";
  extensions_["png"]  = "image/png";
  extensions_["svg"]  = "image/svg+xml";
  extensions_["wasm"] = "application/wasm";
  extensions_["woff"] = "application/x-font-woff";
  extensions_["xml"]  = "application/xml";
}


static std::string GetMimeType(const std::string& path)
{
  size_t dot = path.find_last_of('.');

  std::string extension = (dot == std::string::npos) ? "" : path.substr(dot + 1);
  std::transform(extension.begin(), extension.end(), extension.begin(), tolower);

  std::map<std::string, std::string>::const_iterator found = extensions_.find(extension);

  if (found != extensions_.end() &&
      !found->second.empty())
  {
    return found->second;
  }
  else
  {
    OrthancPlugins::LogWarning(context_, "ServeFolders: Unknown MIME type for extension \"" + extension + "\"");
    return "application/octet-stream";
  }
}


static bool LookupFolder(std::string& folder,
                         OrthancPluginRestOutput* output,
                         const OrthancPluginHttpRequest* request)
{
  const std::string uri = request->groups[0];

  std::map<std::string, std::string>::const_iterator found = folders_.find(uri);
  if (found == folders_.end())
  {
    OrthancPlugins::LogError(context_, "Unknown URI in plugin server-folders: " + uri);
    OrthancPluginSendHttpStatusCode(context_, output, 404);
    return false;
  }
  else
  {
    folder = found->second;
    return true;
  }
}


static void Answer(OrthancPluginRestOutput* output,
                   const char* content,
                   size_t size,
                   const std::string& mime)
{
  if (generateETag_)
  {
    OrthancPlugins::OrthancString md5(context_);
    md5.Assign(OrthancPluginComputeMd5(context_, content, size));

    std::string etag = "\"" + std::string(md5.GetContent()) + "\"";
    OrthancPluginSetHttpHeader(context_, output, "ETag", etag.c_str());
  }

  SetHttpHeaders(output);
  OrthancPluginAnswerBuffer(context_, output, content, size, mime.c_str());
}


void ServeFolder(OrthancPluginRestOutput* output,
                 const char* url,
                 const OrthancPluginHttpRequest* request)
{
  namespace fs = boost::filesystem;  

  if (request->method != OrthancPluginHttpMethod_Get)
  {
    OrthancPluginSendMethodNotAllowed(context_, output, "GET");
    return;
  }

  std::string folder;

  if (LookupFolder(folder, output, request))
  {
    const fs::path item(request->groups[1]);
    const fs::path parent((fs::path(folder) / item).parent_path());

    if (item.filename().string() == "index.html" &&
        fs::is_directory(parent) &&
        !fs::is_regular_file(fs::path(folder) / item))
    {
      // On-the-fly generation of an "index.html" 
      std::string s;
      s += "<html>\n";
      s += "  <body>\n";
      s += "    <ul>\n";

      fs::directory_iterator end;

      for (fs::directory_iterator it(parent) ; it != end; ++it)
      {
        if (fs::is_directory(it->status()))
        {
          std::string f = it->path().filename().string();
          s += "      <li><a href=\"" + f + "/index.html\">" + f + "/</a></li>\n";
        }
      }

      for (fs::directory_iterator it(parent) ; it != end; ++it)
      {
        fs::file_type type = it->status().type();

        if (type == fs::regular_file ||
            type == fs::reparse_file)  // cf. BitBucket issue #11
        {
          std::string f = it->path().filename().string();
          s += "      <li><a href=\"" + f + "\">" + f + "</a></li>\n";
        }
      }

      s += "    </ul>\n";
      s += "  </body>\n";
      s += "</html>\n";

      Answer(output, s.c_str(), s.size(), "text/html");
    }
    else
    {
      std::string path = folder + "/" + item.string();
      std::string mime = GetMimeType(path);

      OrthancPlugins::MemoryBuffer content(context_);

      try
      {
        content.ReadFile(path);
      }
      catch (...)
      {
        ORTHANC_PLUGINS_THROW_EXCEPTION(InexistentFile);
      }

      boost::posix_time::ptime lastModification = boost::posix_time::from_time_t(fs::last_write_time(path));
      std::string t = boost::posix_time::to_iso_string(lastModification);
      OrthancPluginSetHttpHeader(context_, output, "Last-Modified", t.c_str());

      Answer(output, content.GetData(), content.GetSize(), mime);
    }
  }
}


void ListServedFolders(OrthancPluginRestOutput* output,
                       const char* url,
                       const OrthancPluginHttpRequest* request)
{
  if (request->method != OrthancPluginHttpMethod_Get)
  {
    OrthancPluginSendMethodNotAllowed(context_, output, "GET");
    return;
  }

  std::string s = "<html><body><h1>Additional folders served by Orthanc</h1>\n";

  if (folders_.empty())
  {
    s += "<p>Empty section <tt>ServeFolders</tt> in your configuration file: No additional folder is served.</p>\n";
  }
  else
  {
    s += "<ul>\n";
    for (std::map<std::string, std::string>::const_iterator
           it = folders_.begin(); it != folders_.end(); ++it)
    {
      // The URI is relative to INDEX_URI ("/app/plugin-serve-folders.html")
      s += "<li><a href=\"../" + it->first + "/index.html\">" + it->first + "</li>\n";
    }
    
    s += "</ul>\n";
  }

  s += "</body></html>\n";

  Answer(output, s.c_str(), s.size(), "text/html");
}


static void ConfigureFolders(const Json::Value& folders)
{
  if (folders.type() != Json::objectValue)
  {
    OrthancPlugins::LogError(context_, "The list of folders to be served is badly formatted (must be a JSON object)");
    ORTHANC_PLUGINS_THROW_EXCEPTION(BadFileFormat);
  }

  Json::Value::Members members = folders.getMemberNames();

  // Register the callback for each base URI
  for (Json::Value::Members::const_iterator 
         it = members.begin(); it != members.end(); ++it)
  {
    if (folders[*it].type() != Json::stringValue)
    {
      OrthancPlugins::LogError(context_, "The folder to be served \"" + *it + 
                               "\" must be associated with a string value (its mapped URI)");
      ORTHANC_PLUGINS_THROW_EXCEPTION(BadFileFormat);
    }

    std::string baseUri = *it;

    // Remove the heading and trailing slashes in the root URI, if any
    while (!baseUri.empty() &&
           *baseUri.begin() == '/')
    {
      baseUri = baseUri.substr(1);
    }

    while (!baseUri.empty() &&
           *baseUri.rbegin() == '/')
    {
      baseUri.resize(baseUri.size() - 1);
    }

    if (baseUri.empty())
    {
      OrthancPlugins::LogError(context_, "The URI of a folder to be served cannot be empty");
      ORTHANC_PLUGINS_THROW_EXCEPTION(BadFileFormat);
    }

    // Check whether the source folder exists and is indeed a directory
    const std::string folder = folders[*it].asString();
    if (!boost::filesystem::is_directory(folder))
    {
      OrthancPlugins::LogError(context_, "Trying and serve an inexistent folder: " + folder);
      ORTHANC_PLUGINS_THROW_EXCEPTION(InexistentFile);
    }

    folders_[baseUri] = folder;

    // Register the callback to serve the folder
    {
      const std::string regex = "/(" + baseUri + ")/(.*)";
      OrthancPlugins::RegisterRestCallback<ServeFolder>(context_, regex.c_str(), true);
    }
  }
}


static void ConfigureExtensions(const Json::Value& extensions)
{
  if (extensions.type() != Json::objectValue)
  {
    OrthancPlugins::LogError(context_, "The list of extensions is badly formatted (must be a JSON object)");
    ORTHANC_PLUGINS_THROW_EXCEPTION(BadFileFormat);
  }

  Json::Value::Members members = extensions.getMemberNames();

  for (Json::Value::Members::const_iterator 
         it = members.begin(); it != members.end(); ++it)
  {
    if (extensions[*it].type() != Json::stringValue)
    {
      OrthancPlugins::LogError(context_, "The file extension \"" + *it + 
                               "\" must be associated with a string value (its MIME type)");
      ORTHANC_PLUGINS_THROW_EXCEPTION(BadFileFormat);
    }

    const std::string& mime = extensions[*it].asString();

    std::string name = *it;

    if (!name.empty() &&
        name[0] == '.')
    {
      name = name.substr(1);  // Remove the leading dot "."
    }

    extensions_[name] = mime;

    if (mime.empty())
    {
      OrthancPlugins::LogWarning(context_, "ServeFolders: Removing MIME type for file extension \"." + name + "\"");
    }
    else
    {
      OrthancPlugins::LogWarning(context_, "ServeFolders: Associating file extension \"." + name + 
                                 "\" with MIME type \"" + mime + "\"");
    }
  }  
}


static void ReadConfiguration()
{
  OrthancPlugins::OrthancConfiguration configuration;

  {
    OrthancPlugins::OrthancConfiguration globalConfiguration(context_);
    globalConfiguration.GetSection(configuration, "ServeFolders");
  }

  if (!configuration.IsSection("Folders"))
  {
    // This is a basic configuration
    ConfigureFolders(configuration.GetJson());
  }
  else
  {
    // This is an advanced configuration
    ConfigureFolders(configuration.GetJson()["Folders"]);

    bool tmp;

    if (configuration.LookupBooleanValue(tmp, "AllowCache"))
    {
      allowCache_ = tmp;
      OrthancPlugins::LogWarning(context_, "ServeFolders: Requesting the HTTP client to " +
                                 std::string(tmp ? "enable" : "disable") + 
                                 " its caching mechanism");
    }

    if (configuration.LookupBooleanValue(tmp, "GenerateETag"))
    {
      generateETag_ = tmp;
      OrthancPlugins::LogWarning(context_, "ServeFolders: The computation of an ETag for the served resources is " +
                                 std::string(tmp ? "enabled" : "disabled"));
    }

    OrthancPlugins::OrthancConfiguration extensions;
    configuration.GetSection(extensions, "Extensions");
    ConfigureExtensions(extensions.GetJson());
  }

  if (folders_.empty())
  {
    OrthancPlugins::LogWarning(context_, "ServeFolders: Empty configuration file: No additional folder will be served!");
  }
}


extern "C"
{
  ORTHANC_PLUGINS_API int32_t OrthancPluginInitialize(OrthancPluginContext* context)
  {
    context_ = context;

    /* Check the version of the Orthanc core */
    if (OrthancPluginCheckVersion(context_) == 0)
    {
      OrthancPlugins::ReportMinimalOrthancVersion(context_, 
                                                  ORTHANC_PLUGINS_MINIMAL_MAJOR_NUMBER,
                                                  ORTHANC_PLUGINS_MINIMAL_MINOR_NUMBER,
                                                  ORTHANC_PLUGINS_MINIMAL_REVISION_NUMBER);
      return -1;
    }

    RegisterDefaultExtensions();
    OrthancPluginSetDescription(context_, "Serve additional folders with the HTTP server of Orthanc.");
    OrthancPluginSetRootUri(context, INDEX_URI);
    OrthancPlugins::RegisterRestCallback<ListServedFolders>(context_, INDEX_URI, true);

    try
    {
      ReadConfiguration();
    }
    catch (OrthancPlugins::PluginException& e)
    {
      OrthancPlugins::LogError(context_, "Error while initializing the ServeFolders plugin: " + 
                               std::string(e.What(context_)));
    }

    return 0;
  }


  ORTHANC_PLUGINS_API void OrthancPluginFinalize()
  {
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetName()
  {
    return "serve-folders";
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetVersion()
  {
    return SERVE_FOLDERS_VERSION;
  }
}
