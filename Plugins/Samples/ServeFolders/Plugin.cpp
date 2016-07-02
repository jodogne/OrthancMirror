/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
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


static OrthancPluginContext* context_ = NULL;
static std::map<std::string, std::string> folders_;
static const char* INDEX_URI = "/app/plugin-serve-folders.html";
static bool allowCache_ = true;
static bool generateETag_ = true;  // TODO parameter


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


static const char* GetMimeType(const std::string& path)
{
  size_t dot = path.find_last_of('.');

  std::string extension = (dot == std::string::npos) ? "" : path.substr(dot);
  std::transform(extension.begin(), extension.end(), extension.begin(), tolower);

  if (extension == ".html")
  {
    return "text/html";
  }
  else if (extension == ".css")
  {
    return "text/css";
  }
  else if (extension == ".js")
  {
    return "application/javascript";
  }
  else if (extension == ".gif")
  {
    return "image/gif";
  }
  else if (extension == ".svg")
  {
    return "image/svg+xml";
  }
  else if (extension == ".json" ||
           extension == ".nmf")
  {
    return "application/json";
  }
  else if (extension == ".xml")
  {
    return "application/xml";
  }
  else if (extension == ".png")
  {
    return "image/png";
  }
  else if (extension == ".jpg" || 
           extension == ".jpeg")
  {
    return "image/jpeg";
  }
  else if (extension == ".woff")
  {
    return "application/x-font-woff";
  }
  else if (extension == ".pexe")
  {
    return "application/x-pnacl";
  }
  else if (extension == ".nexe")
  {
    return "application/x-nacl";
  }
  else
  {
    std::string s = "Unknown MIME type for extension: " + extension;
    OrthancPluginLogWarning(context_, s.c_str());
    return "application/octet-stream";
  }
}


static bool ReadFile(std::string& target,
                     const std::string& path)
{
  try
  {
    OrthancPlugins::MemoryBuffer buffer(context_);
    buffer.ReadFile(path);
    buffer.ToString(target);
    return true;
  }
  catch (OrthancPlugins::PluginException)
  {
    return false;
  }
}


static bool ReadConfiguration(Json::Value& configuration,
                              OrthancPluginContext* context)
{
  std::string s;

  {
    char* tmp = OrthancPluginGetConfiguration(context);
    if (tmp == NULL)
    {
      OrthancPluginLogError(context, "Error while retrieving the configuration from Orthanc");
      return false;
    }

    s.assign(tmp);
    OrthancPluginFreeString(context, tmp);      
  }

  Json::Reader reader;
  if (reader.parse(s, configuration))
  {
    return true;
  }
  else
  {
    OrthancPluginLogError(context, "Unable to parse the configuration");
    return false;
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
    std::string s = "Unknown URI in plugin server-folders: " + uri;
    OrthancPluginLogError(context_, s.c_str());
    OrthancPluginSendHttpStatusCode(context_, output, 404);
    return false;
  }
  else
  {
    folder = found->second;
    return true;
  }
}


static OrthancPluginErrorCode FolderCallback(OrthancPluginRestOutput* output,
                                             const char* url,
                                             const OrthancPluginHttpRequest* request)
{
  namespace fs = boost::filesystem;  

  if (request->method != OrthancPluginHttpMethod_Get)
  {
    OrthancPluginSendMethodNotAllowed(context_, output, "GET");
    return OrthancPluginErrorCode_Success;
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

      SetHttpHeaders(output);
      OrthancPluginAnswerBuffer(context_, output, s.c_str(), s.size(), "text/html");
    }
    else
    {
      std::string path = folder + "/" + item.string();
      const char* mime = GetMimeType(path);

      std::string s;
      if (ReadFile(s, path))
      {
        const char* resource = s.size() ? s.c_str() : NULL;

        if (generateETag_)
        {
          OrthancPlugins::OrthancString md5(context_, OrthancPluginComputeMd5(context_, resource, s.size()));
          std::string etag = "\"" + std::string(md5.GetContent()) + "\"";
          OrthancPluginSetHttpHeader(context_, output, "ETag", etag.c_str());
        }

        boost::posix_time::ptime lastModification = boost::posix_time::from_time_t(fs::last_write_time(path));
        std::string t = boost::posix_time::to_iso_string(lastModification);
        OrthancPluginSetHttpHeader(context_, output, "Last-Modified", t.c_str());

        SetHttpHeaders(output);
        OrthancPluginAnswerBuffer(context_, output, resource, s.size(), mime);
      }
      else
      {
        std::string s = "Inexistent file in served folder: " + path;
        OrthancPluginLogError(context_, s.c_str());
        OrthancPluginSendHttpStatusCode(context_, output, 404);
      }
    }
  }

  return OrthancPluginErrorCode_Success;
}


static OrthancPluginErrorCode ListServedFolders(OrthancPluginRestOutput* output,
                                                const char* url,
                                                const OrthancPluginHttpRequest* request)
{
  if (request->method != OrthancPluginHttpMethod_Get)
  {
    OrthancPluginSendMethodNotAllowed(context_, output, "GET");
    return OrthancPluginErrorCode_Success;
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

  SetHttpHeaders(output);
  OrthancPluginAnswerBuffer(context_, output, s.c_str(), s.size(), "text/html");

  return OrthancPluginErrorCode_Success;
}


extern "C"
{
  ORTHANC_PLUGINS_API int32_t OrthancPluginInitialize(OrthancPluginContext* context)
  {
    context_ = context;

    /* Check the version of the Orthanc core */
    if (OrthancPluginCheckVersion(context_) == 0)
    {
      char info[1024];
      sprintf(info, "Your version of Orthanc (%s) must be above %d.%d.%d to run this plugin",
              context_->orthancVersion,
              ORTHANC_PLUGINS_MINIMAL_MAJOR_NUMBER,
              ORTHANC_PLUGINS_MINIMAL_MINOR_NUMBER,
              ORTHANC_PLUGINS_MINIMAL_REVISION_NUMBER);
      OrthancPluginLogError(context_, info);
      return -1;
    }

    OrthancPluginSetDescription(context_, "Serve additional folders with the HTTP server of Orthanc.");

    Json::Value configuration;
    if (!ReadConfiguration(configuration, context_))
    {
      return -1;
    }

    if (configuration.isMember("ServeFoldersNoCache"))
    {
      OrthancPluginLogWarning(context_, "Disabling the cache");
      allowCache_ = false;
    }

    if (configuration.isMember("ServeFolders"))
    {
      if (configuration["ServeFolders"].type() != Json::objectValue)
      {
        OrthancPluginLogError(context_, "The \"ServeFolders\" configuration section is badly formatted (must be a JSON object)");
        return -1;
      }
    }
    else
    {
      OrthancPluginLogWarning(context_, "No section \"ServeFolders\" in your configuration file: "
                              "No additional folder will be served!");
      configuration["ServeFolders"] = Json::objectValue;
    }


    Json::Value::Members members = configuration["ServeFolders"].getMemberNames();

    // Register the callback for each base URI
    for (Json::Value::Members::const_iterator 
           it = members.begin(); it != members.end(); ++it)
    {
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
        OrthancPluginLogError(context_, "The URI of a folder to be served cannot be empty");
        return -1;
      }

      // Check whether the source folder exists and is indeed a directory
      const std::string folder = configuration["ServeFolders"][*it].asString();
      if (!boost::filesystem::is_directory(folder))
      {
        std::string msg = "Trying and serve an inexistent folder: " + folder;
        OrthancPluginLogError(context_, msg.c_str());
        return -1;
      }

      folders_[baseUri] = folder;

      // Register the callback to serve the folder
      {
        const std::string regex = "/(" + baseUri + ")/(.*)";
        OrthancPluginRegisterRestCallback(context, regex.c_str(), FolderCallback);
      }
    }

    OrthancPluginRegisterRestCallback(context, INDEX_URI, ListServedFolders);
    OrthancPluginSetRootUri(context, INDEX_URI);

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
