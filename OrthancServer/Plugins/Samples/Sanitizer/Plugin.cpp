/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
 * Copyright (C) 2021-2021 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

#include <boost/filesystem.hpp>
#include <json/value.h> 
#include <string.h>
#include <iostream>




OrthancPluginReceivedInstanceCallbackResult ReceivedInstanceCallback(const void* receivedDicomBuffer,
                                                                     uint64_t receivedDicomBufferSize,
                                                                     void** modifiedDicomBuffer,
                                                                     uint64_t* modifiedDicomBufferSize)
                                                                    //  OrthancPluginMemoryBuffer* modifiedDicomBuffer)
{
    // note: this sample plugin won't work with multi-frame images or badly formed images 
    // OrthancPluginCreateDicom and OrthancPluginDicomBufferToJson do not support multi-frame and are quite touchy with invalid tag values

    Json::Value receivedDicomAsJson;
    OrthancPlugins::OrthancString str;
    str.Assign(OrthancPluginDicomBufferToJson
               (OrthancPlugins::GetGlobalContext(), 
                receivedDicomBuffer, 
                receivedDicomBufferSize, 
                OrthancPluginDicomToJsonFormat_Short, 
                static_cast<OrthancPluginDicomToJsonFlags>(OrthancPluginDicomToJsonFlags_IncludeBinary | OrthancPluginDicomToJsonFlags_IncludePrivateTags | OrthancPluginDicomToJsonFlags_IncludeUnknownTags | OrthancPluginDicomToJsonFlags_SkipGroupLengths | OrthancPluginDicomToJsonFlags_IncludePixelData),
                0));
    
    str.ToJson(receivedDicomAsJson);

    if (receivedDicomAsJson["0008,0080"] != "My Institution")
    {
        receivedDicomAsJson["0008,0080"] = "My Institution";

        OrthancPluginMemoryBuffer modifiedDicom;
        std::string serializedModifiedDicomAsJson;
        OrthancPlugins::WriteFastJson(serializedModifiedDicomAsJson, receivedDicomAsJson);
        OrthancPluginErrorCode createResult = OrthancPluginCreateDicom(OrthancPlugins::GetGlobalContext(), 
                                                                       &modifiedDicom, 
                                                                       serializedModifiedDicomAsJson.c_str(), 
                                                                       NULL, 
                                                                       OrthancPluginCreateDicomFlags_DecodeDataUriScheme);

        if (createResult == OrthancPluginErrorCode_Success)
        {    
            *modifiedDicomBuffer = modifiedDicom.data;
            *modifiedDicomBufferSize = modifiedDicom.size;
        
            return OrthancPluginReceivedInstanceCallbackResult_Modified;
        }
        else
        {
            return OrthancPluginReceivedInstanceCallbackResult_KeepAsIs;
        }
    }

    return OrthancPluginReceivedInstanceCallbackResult_KeepAsIs;
}


extern "C"
{
  ORTHANC_PLUGINS_API int32_t OrthancPluginInitialize(OrthancPluginContext* c)
  {
    OrthancPlugins::SetGlobalContext(c);

    /* Check the version of the Orthanc core */
    // if (OrthancPluginCheckVersion(c) == 0)
    // {
    //   OrthancPlugins::ReportMinimalOrthancVersion(ORTHANC_PLUGINS_MINIMAL_MAJOR_NUMBER,
    //                                               ORTHANC_PLUGINS_MINIMAL_MINOR_NUMBER,
    //                                               ORTHANC_PLUGINS_MINIMAL_REVISION_NUMBER);
    //   return -1;
    // }

    OrthancPlugins::LogWarning("Sanitizer plugin is initializing");
    OrthancPluginSetDescription(c, "Sample plugin to sanitize incoming DICOM instances.");

    OrthancPluginRegisterReceivedInstanceCallback(c, ReceivedInstanceCallback);

    return 0;
  }


  ORTHANC_PLUGINS_API void OrthancPluginFinalize()
  {
    OrthancPlugins::LogWarning("Sanitizer plugin is finalizing");
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetName()
  {
    return "sanitizer";
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetVersion()
  {
    return "0.1";
  }
}
