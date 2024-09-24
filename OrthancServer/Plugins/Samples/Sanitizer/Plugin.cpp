/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2024 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2024 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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
#include "../../../../OrthancFramework/Sources/DicomParsing/ParsedDicomFile.h"
#include "../../../../OrthancFramework/Sources/OrthancFramework.h"
#include "../Common/OrthancPluginCppWrapper.h"

#include <boost/filesystem.hpp>
#include <json/value.h> 
#include <string.h>
#include <iostream>

#define ORTHANC_PLUGIN_NAME  "sanitizer"


OrthancPluginReceivedInstanceAction ReceivedInstanceCallback(OrthancPluginMemoryBuffer64* modifiedDicomBuffer,
                                                             const void* receivedDicomBuffer,
                                                             uint64_t receivedDicomBufferSize,
                                                             OrthancPluginInstanceOrigin origin)
{
  Orthanc::ParsedDicomFile dicom(receivedDicomBuffer, receivedDicomBufferSize);
  std::string institutionName = "My institution";

  dicom.Replace(Orthanc::DICOM_TAG_INSTITUTION_NAME, institutionName, false, Orthanc::DicomReplaceMode_InsertIfAbsent, "");
  
  std::string modifiedDicom;
  dicom.SaveToMemoryBuffer(modifiedDicom);
  
  OrthancPluginCreateMemoryBuffer64(OrthancPlugins::GetGlobalContext(), modifiedDicomBuffer, modifiedDicom.size());
  memcpy(modifiedDicomBuffer->data, modifiedDicom.c_str(), modifiedDicom.size());
  
  return OrthancPluginReceivedInstanceAction_Modify;
}


extern "C"
{
  ORTHANC_PLUGINS_API int32_t OrthancPluginInitialize(OrthancPluginContext* c)
  {
    OrthancPlugins::SetGlobalContext(c);

    Orthanc::InitializeFramework("", true);

    /* Check the version of the Orthanc core */
    if (OrthancPluginCheckVersion(c) == 0)
    {
      OrthancPlugins::ReportMinimalOrthancVersion(ORTHANC_PLUGINS_MINIMAL_MAJOR_NUMBER,
                                                  ORTHANC_PLUGINS_MINIMAL_MINOR_NUMBER,
                                                  ORTHANC_PLUGINS_MINIMAL_REVISION_NUMBER);
      return -1;
    }
    
    ORTHANC_PLUGINS_LOG_WARNING("Sanitizer plugin is initializing");
    OrthancPlugins::SetDescription(ORTHANC_PLUGIN_NAME, "Sample plugin to sanitize incoming DICOM instances.");

    OrthancPluginRegisterReceivedInstanceCallback(c, ReceivedInstanceCallback);

    return 0;
  }


  ORTHANC_PLUGINS_API void OrthancPluginFinalize()
  {
    ORTHANC_PLUGINS_LOG_WARNING("Sanitizer plugin is finalizing");
    Orthanc::FinalizeFramework();
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetName()
  {
    return ORTHANC_PLUGIN_NAME;
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetVersion()
  {
    return "0.1";
  }
}
