/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2015 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 **/


#include "../Configuration.h"

#include <OrthancCPlugin.h>
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
static int32_t ServeStaticResource(OrthancPluginRestOutput* output,
                                   const char* url,
                                   const OrthancPluginHttpRequest* request)
{
  if (request->method != OrthancPluginHttpMethod_Get)
  {
    OrthancPluginSendMethodNotAllowed(context, output, "GET");
    return 0;
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

    return 0;
  }
  catch (std::runtime_error&)
  {
    std::string s = "Unknown static resource in plugin: " + std::string(request->groups[0]);
    OrthancPluginLogError(context, s.c_str());
    OrthancPluginSendHttpStatusCode(context, output, 404);
    return 0;
  }
}
#endif


#if ORTHANC_PLUGIN_STANDALONE == 0
static int32_t ServeFolder(OrthancPluginRestOutput* output,
                           const char* url,
                           const OrthancPluginHttpRequest* request)
{
  if (request->method != OrthancPluginHttpMethod_Get)
  {
    OrthancPluginSendMethodNotAllowed(context, output, "GET");
    return 0;
  }

  std::string path = ORTHANC_PLUGIN_RESOURCES_ROOT "/" + std::string(request->groups[0]);
  const char* mime = GetMimeType(path);

  std::string s;
  if (ReadFile(s, path))
  {
    const char* resource = s.size() ? s.c_str() : NULL;
    OrthancPluginAnswerBuffer(context, output, resource, s.size(), mime);

    return 0;
  }
  else
  {
    std::string s = "Unknown static resource in plugin: " + std::string(request->groups[0]);
    OrthancPluginLogError(context, s.c_str());
    OrthancPluginSendHttpStatusCode(context, output, 404);
    return 0;
  }
}
#endif


static int32_t RedirectRoot(OrthancPluginRestOutput* output,
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

  return 0;
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
