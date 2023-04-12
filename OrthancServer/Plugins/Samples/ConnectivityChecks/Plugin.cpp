/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2021-2023 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#include <ConnectivityChecksResources.h>
#include <orthanc/OrthancCPlugin.h>

#include "../../../../OrthancFramework/Sources/OrthancException.h"
#include "../../../../OrthancFramework/Sources/SystemToolbox.h"

#define ROOT_URI "/connectivity-checks"


static OrthancPluginContext* context_ = NULL;


template <Orthanc::EmbeddedResources::DirectoryResourceId DIRECTORY>
static OrthancPluginErrorCode ServeStaticResource(OrthancPluginRestOutput* output,
                                                  const char* url,
                                                  const OrthancPluginHttpRequest* request)
{
  if (request->method != OrthancPluginHttpMethod_Get)
  {
    OrthancPluginSendMethodNotAllowed(context_, output, "GET");
    return OrthancPluginErrorCode_Success;
  }

  std::string path = "/" + std::string(request->groups[0]);
  std::string mime = Orthanc::EnumerationToString(Orthanc::SystemToolbox::AutodetectMimeType(path));

  try
  {
    std::string s;
    Orthanc::EmbeddedResources::GetDirectoryResource(s, DIRECTORY, path.c_str());

    const char* resource = s.size() ? s.c_str() : NULL;
    OrthancPluginAnswerBuffer(context_, output, resource, s.size(), mime.c_str());
  }
  catch (Orthanc::OrthancException&)
  {
    std::string s = "Unknown static resource in plugin: " + std::string(request->groups[0]);
    OrthancPluginLogError(context_, s.c_str());
    OrthancPluginSendHttpStatusCode(context_, output, 404);
  }

  return OrthancPluginErrorCode_Success;
}



extern "C"
{
  ORTHANC_PLUGINS_API int32_t OrthancPluginInitialize(OrthancPluginContext* c)
  {
    context_ = c;
    
    /* Check the version of the Orthanc core */
    if (OrthancPluginCheckVersion(c) == 0)
    {
      char info[256];
      sprintf(info, "Your version of Orthanc (%s) must be above %d.%d.%d to run this plugin",
              c->orthancVersion,
              ORTHANC_PLUGINS_MINIMAL_MAJOR_NUMBER,
              ORTHANC_PLUGINS_MINIMAL_MINOR_NUMBER,
              ORTHANC_PLUGINS_MINIMAL_REVISION_NUMBER);
      OrthancPluginLogError(context_, info);
      return -1;
    }

    /* Register the callbacks */
    OrthancPluginSetDescription(context_, "Utilities to check connectivity to DICOM modalities, DICOMweb servers and Orthanc peers.");
    OrthancPluginSetRootUri(context_, ROOT_URI "/app/index.html");
    OrthancPluginRegisterRestCallback(context_, ROOT_URI "/libs/(.*)", ServeStaticResource<Orthanc::EmbeddedResources::LIBRARIES>);
    OrthancPluginRegisterRestCallback(context_, ROOT_URI "/app/(.*)", ServeStaticResource<Orthanc::EmbeddedResources::WEB_RESOURCES>);
 
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
