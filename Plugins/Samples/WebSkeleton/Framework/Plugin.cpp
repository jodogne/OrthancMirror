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


#include "../Configuration.h"

#include <orthanc/OrthancCPlugin.h>
#include <string>
#include <stdexcept>
#include <algorithm>
#include <sys/stat.h>

#if ORTHANC_PLUGIN_STANDALONE == 1
// This is an auto-generated file for standalone builds
#include <EmbeddedResources.h>
#endif

static OrthancPluginContext* context = NULL;


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
  else if (extension == ".json")
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
  else if (extension == ".jpg" || extension == ".jpeg")
  {
    return "image/jpeg";
  }
  else
  {
    std::string s = "Unknown MIME type for extension: " + extension;
    OrthancPluginLogWarning(context, s.c_str());
    return "application/octet-stream";
  }
}


static bool ReadFile(std::string& content,
                     const std::string& path)
{
  struct stat s;
  if (stat(path.c_str(), &s) != 0 ||
      !(s.st_mode & S_IFREG))
  {
    // Either the path does not exist, or it is not a regular file
    return false;
  }

  FILE* fp = fopen(path.c_str(), "rb");
  if (fp == NULL)
  {
    return false;
  }

  long size;

  if (fseek(fp, 0, SEEK_END) == -1 ||
      (size = ftell(fp)) < 0)
  {
    fclose(fp);
    return false;
  }

  content.resize(size);
      
  if (fseek(fp, 0, SEEK_SET) == -1)
  {
    fclose(fp);
    return false;
  }

  bool ok = true;

  if (size > 0 &&
      fread(&content[0], size, 1, fp) != 1)
  {
    ok = false;
  }

  fclose(fp);

  return ok;
}


#if ORTHANC_PLUGIN_STANDALONE == 1
static OrthancPluginErrorCode ServeStaticResource(OrthancPluginRestOutput* output,
                                                  const char* url,
                                                  const OrthancPluginHttpRequest* request)
{
  if (request->method != OrthancPluginHttpMethod_Get)
  {
    OrthancPluginSendMethodNotAllowed(context, output, "GET");
    return OrthancPluginErrorCode_Success;
  }

  std::string path = "/" + std::string(request->groups[0]);
  const char* mime = GetMimeType(path);

  try
  {
    std::string s;
    Orthanc::EmbeddedResources::GetDirectoryResource
      (s, Orthanc::EmbeddedResources::STATIC_RESOURCES, path.c_str());

    const char* resource = s.size() ? s.c_str() : NULL;
    OrthancPluginAnswerBuffer(context, output, resource, s.size(), mime);
  }
  catch (std::runtime_error&)
  {
    std::string s = "Unknown static resource in plugin: " + std::string(request->groups[0]);
    OrthancPluginLogError(context, s.c_str());
    OrthancPluginSendHttpStatusCode(context, output, 404);
  }

  return OrthancPluginErrorCode_Success;
}
#endif


#if ORTHANC_PLUGIN_STANDALONE == 0
static OrthancPluginErrorCode ServeFolder(OrthancPluginRestOutput* output,
                                          const char* url,
                                          const OrthancPluginHttpRequest* request)
{
  if (request->method != OrthancPluginHttpMethod_Get)
  {
    OrthancPluginSendMethodNotAllowed(context, output, "GET");
    return OrthancPluginErrorCode_Success;
  }

  std::string path = ORTHANC_PLUGIN_RESOURCES_ROOT "/" + std::string(request->groups[0]);
  const char* mime = GetMimeType(path);

  std::string s;
  if (ReadFile(s, path))
  {
    const char* resource = s.size() ? s.c_str() : NULL;
    OrthancPluginAnswerBuffer(context, output, resource, s.size(), mime);
  }
  else
  {
    std::string s = "Unknown static resource in plugin: " + std::string(request->groups[0]);
    OrthancPluginLogError(context, s.c_str());
    OrthancPluginSendHttpStatusCode(context, output, 404);
  }

  return OrthancPluginErrorCode_Success;
}
#endif


static OrthancPluginErrorCode RedirectRoot(OrthancPluginRestOutput* output,
                                           const char* url,
                                           const OrthancPluginHttpRequest* request)
{
  if (request->method != OrthancPluginHttpMethod_Get)
  {
    OrthancPluginSendMethodNotAllowed(context, output, "GET");
  }
  else
  {
    OrthancPluginRedirect(context, output, ORTHANC_PLUGIN_WEB_ROOT "index.html");
  }

  return OrthancPluginErrorCode_Success;
}


extern "C"
{
  ORTHANC_PLUGINS_API int32_t OrthancPluginInitialize(OrthancPluginContext* c)
  {
    context = c;
    
    /* Check the version of the Orthanc core */
    if (OrthancPluginCheckVersion(c) == 0)
    {
      char info[256];
      sprintf(info, "Your version of Orthanc (%s) must be above %d.%d.%d to run this plugin",
              c->orthancVersion,
              ORTHANC_PLUGINS_MINIMAL_MAJOR_NUMBER,
              ORTHANC_PLUGINS_MINIMAL_MINOR_NUMBER,
              ORTHANC_PLUGINS_MINIMAL_REVISION_NUMBER);
      OrthancPluginLogError(context, info);
      return -1;
    }

    /* Register the callbacks */

#if ORTHANC_PLUGIN_STANDALONE == 1
    OrthancPluginLogInfo(context, "Serving static resources (standalone build)");
    OrthancPluginRegisterRestCallback(context, ORTHANC_PLUGIN_WEB_ROOT "(.*)", ServeStaticResource);
#else
    OrthancPluginLogInfo(context, "Serving resources from folder: " ORTHANC_PLUGIN_RESOURCES_ROOT);
    OrthancPluginRegisterRestCallback(context, ORTHANC_PLUGIN_WEB_ROOT "(.*)", ServeFolder);
#endif

    OrthancPluginRegisterRestCallback(context, "/", RedirectRoot);
 
    return 0;
  }


  ORTHANC_PLUGINS_API void OrthancPluginFinalize()
  {
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetName()
  {
    return ORTHANC_PLUGIN_NAME;
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetVersion()
  {
    return ORTHANC_PLUGIN_VERSION;
  }
}
