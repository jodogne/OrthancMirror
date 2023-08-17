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


#include "MultitenantDicomServer.h"

#include "../../../../OrthancFramework/Sources/DicomParsing/FromDcmtkBridge.h"
#include "../../../../OrthancFramework/Sources/Logging.h"
#include "../../../../OrthancFramework/Sources/OrthancException.h"

#include "../Common/OrthancPluginCppWrapper.h"


typedef std::list<MultitenantDicomServer*> DicomServers;

static DicomServers dicomServers_;


static OrthancPluginErrorCode OnChangeCallback(OrthancPluginChangeType changeType,
                                               OrthancPluginResourceType resourceType,
                                               const char* resourceId)
{
  switch (changeType)
  {
    case OrthancPluginChangeType_OrthancStarted:
    {
      for (DicomServers::iterator it = dicomServers_.begin(); it != dicomServers_.end(); ++it)
      {    
        if (*it != NULL)
        {
          try
          {
            (*it)->Start();
          }
          catch (Orthanc::OrthancException& e)
          {
            LOG(ERROR) << "Exception while stopping the multitenant DICOM server: " << e.What();
          }
        }
      }

      break;
    }
    
    case OrthancPluginChangeType_OrthancStopped:
    {
      for (DicomServers::iterator it = dicomServers_.begin(); it != dicomServers_.end(); ++it)
      {    
        if (*it != NULL)
        {
          try
          {
            (*it)->Stop();
          }
          catch (Orthanc::OrthancException& e)
          {
            LOG(ERROR) << "Exception while stopping the multitenant DICOM server: " << e.What();
          }
        }
      }
      
      break;
    }
    
    default:
      break;
  }

  return OrthancPluginErrorCode_Success;
}


extern "C"
{
  ORTHANC_PLUGINS_API int32_t OrthancPluginInitialize(OrthancPluginContext* context)
  {
    OrthancPlugins::SetGlobalContext(context);

    /* Check the version of the Orthanc core */
    if (OrthancPluginCheckVersion(OrthancPlugins::GetGlobalContext()) == 0)
    {
      char info[1024];
      sprintf(info, "Your version of Orthanc (%s) must be above %d.%d.%d to run this plugin",
              OrthancPlugins::GetGlobalContext()->orthancVersion,
              ORTHANC_PLUGINS_MINIMAL_MAJOR_NUMBER,
              ORTHANC_PLUGINS_MINIMAL_MINOR_NUMBER,
              ORTHANC_PLUGINS_MINIMAL_REVISION_NUMBER);
      OrthancPluginLogError(OrthancPlugins::GetGlobalContext(), info);
      return -1;
    }

#if ORTHANC_FRAMEWORK_VERSION_IS_ABOVE(1, 7, 2)
    Orthanc::Logging::InitializePluginContext(context);
#else
    Orthanc::Logging::Initialize(context);
#endif

    if (!OrthancPlugins::CheckMinimalOrthancVersion(1, 12, 0))
    {
      OrthancPlugins::ReportMinimalOrthancVersion(1, 12, 0);
      return -1;
    }

    // For static builds, this is not required - but does not harm - because the dictionary seems to be shared between the Orthanc Core and the plugin.
    // For dynamic builds, this is however required.  See https://discourse.orthanc-server.org/t/dimse-failure-using-multitenant-plugin/3665
    Orthanc::FromDcmtkBridge::InitializeDictionary(false /* loadPrivateDictionary */);

    /* Disable "gethostbyaddr" (which results in memory leaks) and use raw IP addresses */
    dcmDisableGethostbyaddr.set(OFTrue);

    OrthancPluginSetDescription(context, "Multitenant plugin for Orthanc.");

    OrthancPluginRegisterOnChangeCallback(context, OnChangeCallback);
    
    try
    {
      OrthancPlugins::OrthancConfiguration globalConfig;

      OrthancPlugins::OrthancConfiguration pluginConfig;
      globalConfig.GetSection(pluginConfig, KEY_MULTITENANT_DICOM);

      if (pluginConfig.GetJson().isMember(KEY_SERVERS))
      {
        const Json::Value& servers = pluginConfig.GetJson() [KEY_SERVERS];

        if (servers.type() != Json::arrayValue)
        {
          throw Orthanc::OrthancException(Orthanc::ErrorCode_BadParameterType, "Configuration option \"" +
                                          std::string(KEY_MULTITENANT_DICOM) + "." + std::string(KEY_SERVERS) + "\" must be an array");
        }
        else
        {
          for (Json::Value::ArrayIndex i = 0; i < servers.size(); i++)
          {
            dicomServers_.push_back(new MultitenantDicomServer(servers[i]));
          }
        }
      }
      
      return 0;
    }
    catch (Orthanc::OrthancException& e)
    {
      LOG(ERROR) << "Exception while starting the multitenant DICOM server: " << e.What();
      return -1;
    }
  }


  ORTHANC_PLUGINS_API void OrthancPluginFinalize()
  {
    for (DicomServers::iterator it = dicomServers_.begin(); it != dicomServers_.end(); ++it)
    {    
      if (*it != NULL)
      {
        try
        {
          delete *it;
        }
        catch (Orthanc::OrthancException& e)
        {
          LOG(ERROR) << "Exception while destroying the multitenant DICOM server: " << e.What();
        }
      }
    }
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetName()
  {
    return "multitenant-dicom";
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetVersion()
  {
    return ORTHANC_PLUGIN_VERSION;
  }
}
