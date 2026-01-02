/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2026 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2026 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#include "../PrecompiledHeaders.h"
#include "DicomConnectionInfo.h"

#include "../Compatibility.h"
#include "../Logging.h"
#include "../OrthancException.h"
#include "../SerializationToolbox.h"
#include "../SystemToolbox.h"

namespace Orthanc
{
  static const char* const CALLED_AET = "CalledAet";
  static const char* const REMOTE_AET = "RemoteAet";
  static const char* const REMOTE_IP = "RemoteIp";
  static const char* const MANUFACTURER = "Manufacturer";

  void DicomConnectionInfo::Serialize(Json::Value& target) const
  {
    if (target.type() != Json::objectValue)
    {
      throw OrthancException(ErrorCode_InternalError);
    }
    else
    {
      target[CALLED_AET] = calledAet_;
      target[REMOTE_AET] = remoteAet_;
      target[REMOTE_IP] = remoteIp_;
      target[MANUFACTURER] = EnumerationToString(manufacturer_);
    }
  }


  DicomConnectionInfo::DicomConnectionInfo(const Json::Value& serialized) :
    remoteIp_(SerializationToolbox::ReadString(serialized, REMOTE_IP)),
    remoteAet_(SerializationToolbox::ReadString(serialized, REMOTE_AET)),
    calledAet_(SerializationToolbox::ReadString(serialized, CALLED_AET))
  {
    std::string manufacturer = SerializationToolbox::ReadString(serialized, MANUFACTURER);
    manufacturer_ = StringToModalityManufacturer(manufacturer);
  }
}
