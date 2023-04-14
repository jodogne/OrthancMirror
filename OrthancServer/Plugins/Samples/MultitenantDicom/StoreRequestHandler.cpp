/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2021-2023 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 **/


#include "StoreRequestHandler.h"

#include "../../../../OrthancFramework/Sources/DicomParsing/FromDcmtkBridge.h"
#include "../../../../OrthancFramework/Sources/Logging.h"
#include "../../../../OrthancFramework/Sources/OrthancException.h"
#include "../../../../OrthancFramework/Sources/SerializationToolbox.h"

#include "../Common/OrthancPluginCppWrapper.h"

#include <dcmtk/dcmnet/diutil.h>


uint16_t StoreRequestHandler::Handle(DcmDataset& dicom,
                                     const std::string& remoteIp,
                                     const std::string& remoteAet,
                                     const std::string& calledAet)
{
  std::string buffer;

  if (!Orthanc::FromDcmtkBridge::SaveToMemoryBuffer(buffer, dicom))
  {
    LOG(ERROR) << "Cannot write DICOM file to memory";
    return STATUS_STORE_Error_CannotUnderstand;
  }

  Json::Value info;
  if (!OrthancPlugins::RestApiPost(info, "/instances", buffer, false))
  {
    LOG(ERROR) << "Cannot store the DICOM file";
    return STATUS_STORE_Refused_OutOfResources;
  }

  for (std::set<Orthanc::ResourceType>::const_iterator level = levels_.begin(); level != levels_.end(); ++level)
  {
    for (std::set<std::string>::const_iterator label = labels_.begin(); label != labels_.end(); ++label)
    {
      std::string uri;
      switch (*level)
      {
        case Orthanc::ResourceType_Patient:
          uri = "/patients/" + Orthanc::SerializationToolbox::ReadString(info, "ParentPatient") + "/labels/" + *label;
          break;

        case Orthanc::ResourceType_Study:
          uri = "/studies/" + Orthanc::SerializationToolbox::ReadString(info, "ParentStudy") + "/labels/" + *label;
          break;

        case Orthanc::ResourceType_Series:
          uri = "/series/" + Orthanc::SerializationToolbox::ReadString(info, "ParentSeries") + "/labels/" + *label;
          break;

        case Orthanc::ResourceType_Instance:
          uri = "/instances/" + Orthanc::SerializationToolbox::ReadString(info, "ID") + "/labels/" + *label;
          break;

        default:
          throw Orthanc::OrthancException(Orthanc::ErrorCode_InternalError);
      }

      Json::Value tmp;
      if (!OrthancPlugins::RestApiPut(tmp, uri, std::string(""), false))
      {
        throw Orthanc::OrthancException(Orthanc::ErrorCode_InternalError, "Cannot set label");
      }
    }
  }

  return STATUS_Success;
}
