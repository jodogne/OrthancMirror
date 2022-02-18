/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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
#include "RemoteModalityParameters.h"

#include "../Logging.h"
#include "../OrthancException.h"
#include "../SerializationToolbox.h"

#include <boost/lexical_cast.hpp>
#include <stdexcept>


static const char* KEY_AET = "AET";
static const char* KEY_ALLOW_ECHO = "AllowEcho";
static const char* KEY_ALLOW_FIND = "AllowFind";
static const char* KEY_ALLOW_FIND_WORKLIST = "AllowFindWorklist";
static const char* KEY_ALLOW_GET = "AllowGet";
static const char* KEY_ALLOW_MOVE = "AllowMove";
static const char* KEY_ALLOW_N_ACTION = "AllowNAction";
static const char* KEY_ALLOW_N_EVENT_REPORT = "AllowEventReport";
static const char* KEY_ALLOW_STORAGE_COMMITMENT = "AllowStorageCommitment";
static const char* KEY_ALLOW_STORE = "AllowStore";
static const char* KEY_ALLOW_TRANSCODING = "AllowTranscoding";
static const char* KEY_HOST = "Host";
static const char* KEY_MANUFACTURER = "Manufacturer";
static const char* KEY_PORT = "Port";
static const char* KEY_USE_DICOM_TLS = "UseDicomTls";
static const char* KEY_LOCAL_AET = "LocalAet";
static const char* KEY_TIMEOUT = "Timeout";


namespace Orthanc
{
  void RemoteModalityParameters::Clear()
  {
    aet_ = "ORTHANC";
    host_ = "127.0.0.1";
    port_ = 104;
    manufacturer_ = ModalityManufacturer_Generic;
    allowEcho_ = true;
    allowStore_ = true;
    allowFind_ = true;
    allowFindWorklist_ = true;
    allowMove_ = true;
    allowGet_ = true;
    allowNAction_ = true;  // For storage commitment
    allowNEventReport_ = true;  // For storage commitment
    allowTranscoding_ = true;
    useDicomTls_ = false;
    localAet_.clear();
    timeout_ = 0;
  }


  RemoteModalityParameters::RemoteModalityParameters()
  {
    Clear();
  }

  RemoteModalityParameters::RemoteModalityParameters(const Json::Value &serialized)
  {
    Unserialize(serialized);
  }

  RemoteModalityParameters::RemoteModalityParameters(const std::string& aet,
                                                     const std::string& host,
                                                     uint16_t port,
                                                     ModalityManufacturer manufacturer)
  {
    Clear();
    SetApplicationEntityTitle(aet);
    SetHost(host);
    SetPortNumber(port);
    SetManufacturer(manufacturer);
  }

  const std::string &RemoteModalityParameters::GetApplicationEntityTitle() const
  {
    return aet_;
  }

  void RemoteModalityParameters::SetApplicationEntityTitle(const std::string &aet)
  {
    aet_ = aet;
  }

  const std::string &RemoteModalityParameters::GetHost() const
  {
    return host_;
  }

  void RemoteModalityParameters::SetHost(const std::string &host)
  {
    host_ = host;
  }

  uint16_t RemoteModalityParameters::GetPortNumber() const
  {
    return port_;
  }


  static void CheckPortNumber(int value)
  {
    if (value <= 0 ||
        value >= 65535)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange,
                             "A TCP port number must be in range [1..65534], but found: " +
                             boost::lexical_cast<std::string>(value));
    }
  }


  static uint16_t ReadPortNumber(const Json::Value& value)
  {
    int tmp;

    switch (value.type())
    {
      case Json::intValue:
      case Json::uintValue:
        tmp = value.asInt();
        break;

      case Json::stringValue:
        try
        {
          tmp = boost::lexical_cast<int>(value.asString());
        }
        catch (boost::bad_lexical_cast&)
        {
          throw OrthancException(ErrorCode_BadFileFormat);
        }
        break;

      default:
        throw OrthancException(ErrorCode_BadFileFormat);
    }

    CheckPortNumber(tmp);
    return static_cast<uint16_t>(tmp);
  }


  void RemoteModalityParameters::SetPortNumber(uint16_t port)
  {
    CheckPortNumber(port);
    port_ = port;
  }

  ModalityManufacturer RemoteModalityParameters::GetManufacturer() const
  {
    return manufacturer_;
  }

  void RemoteModalityParameters::SetManufacturer(ModalityManufacturer manufacturer)
  {
    manufacturer_ = manufacturer;
  }

  void RemoteModalityParameters::SetManufacturer(const std::string &manufacturer)
  {
    manufacturer_ = StringToModalityManufacturer(manufacturer);
  }


  void RemoteModalityParameters::UnserializeArray(const Json::Value& serialized)
  {
    assert(serialized.type() == Json::arrayValue);

    if ((serialized.size() != 3 &&
         serialized.size() != 4) ||
        serialized[0].type() != Json::stringValue ||
        serialized[1].type() != Json::stringValue ||
        (serialized.size() == 4 &&
         serialized[3].type() != Json::stringValue))
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    aet_ = serialized[0].asString();
    host_ = serialized[1].asString();
    port_ = ReadPortNumber(serialized[2]);

    if (serialized.size() == 4)
    {
      manufacturer_ = StringToModalityManufacturer(serialized[3].asString());
    }
    else
    {
      manufacturer_ = ModalityManufacturer_Generic;
    }
  }

  
  void RemoteModalityParameters::UnserializeObject(const Json::Value& serialized)
  {
    assert(serialized.type() == Json::objectValue);

    aet_ = SerializationToolbox::ReadString(serialized, KEY_AET);
    host_ = SerializationToolbox::ReadString(serialized, KEY_HOST);

    if (serialized.isMember(KEY_PORT))
    {
      port_ = ReadPortNumber(serialized[KEY_PORT]);
    }
    else
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    if (serialized.isMember(KEY_MANUFACTURER))
    {
      manufacturer_ = StringToModalityManufacturer
        (SerializationToolbox::ReadString(serialized, KEY_MANUFACTURER));
    }   
    else
    {
      manufacturer_ = ModalityManufacturer_Generic;
    }

    if (serialized.isMember(KEY_ALLOW_ECHO))
    {
      allowEcho_ = SerializationToolbox::ReadBoolean(serialized, KEY_ALLOW_ECHO);
    }

    if (serialized.isMember(KEY_ALLOW_FIND))
    {
      allowFind_ = SerializationToolbox::ReadBoolean(serialized, KEY_ALLOW_FIND);
    }

    if (serialized.isMember(KEY_ALLOW_FIND_WORKLIST))
    {
      allowFindWorklist_ = SerializationToolbox::ReadBoolean(serialized, KEY_ALLOW_FIND_WORKLIST);
    }

    if (serialized.isMember(KEY_ALLOW_STORE))
    {
      allowStore_ = SerializationToolbox::ReadBoolean(serialized, KEY_ALLOW_STORE);
    }

    if (serialized.isMember(KEY_ALLOW_GET))
    {
      allowGet_ = SerializationToolbox::ReadBoolean(serialized, KEY_ALLOW_GET);
    }

    if (serialized.isMember(KEY_ALLOW_MOVE))
    {
      allowMove_ = SerializationToolbox::ReadBoolean(serialized, KEY_ALLOW_MOVE);
    }

    if (serialized.isMember(KEY_ALLOW_N_ACTION))
    {
      allowNAction_ = SerializationToolbox::ReadBoolean(serialized, KEY_ALLOW_N_ACTION);
    }

    if (serialized.isMember(KEY_ALLOW_N_EVENT_REPORT))
    {
      allowNEventReport_ = SerializationToolbox::ReadBoolean(serialized, KEY_ALLOW_N_EVENT_REPORT);
    }

    if (serialized.isMember(KEY_ALLOW_STORAGE_COMMITMENT))
    {
      bool allow = SerializationToolbox::ReadBoolean(serialized, KEY_ALLOW_STORAGE_COMMITMENT);
      allowNAction_ = allow;
      allowNEventReport_ = allow;
    }

    if (serialized.isMember(KEY_ALLOW_TRANSCODING))
    {
      allowTranscoding_ = SerializationToolbox::ReadBoolean(serialized, KEY_ALLOW_TRANSCODING);
    }

    if (serialized.isMember(KEY_USE_DICOM_TLS))
    {
      useDicomTls_ = SerializationToolbox::ReadBoolean(serialized, KEY_USE_DICOM_TLS);
    }

    if (serialized.isMember(KEY_LOCAL_AET))
    {
      localAet_ = SerializationToolbox::ReadString(serialized, KEY_LOCAL_AET);
    }

    if (serialized.isMember(KEY_TIMEOUT))
    {
      timeout_ = SerializationToolbox::ReadUnsignedInteger(serialized, KEY_TIMEOUT);
    }
  }


  bool RemoteModalityParameters::IsRequestAllowed(DicomRequestType type) const
  {
    switch (type)
    {
    case DicomRequestType_Echo:
        return allowEcho_;

      case DicomRequestType_Find:
        return allowFind_;

      case DicomRequestType_FindWorklist:
        return allowFindWorklist_;

      case DicomRequestType_Get:
        return allowGet_;

      case DicomRequestType_Move:
        return allowMove_;

      case DicomRequestType_Store:
        return allowStore_;

      case DicomRequestType_NAction:
        return allowNAction_;

      case DicomRequestType_NEventReport:
        return allowNEventReport_;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }


  void RemoteModalityParameters::SetRequestAllowed(DicomRequestType type,
                                                   bool allowed)
  {
    switch (type)
    {
      case DicomRequestType_Echo:
        allowEcho_ = allowed;
        break;

      case DicomRequestType_Find:
        allowFind_ = allowed;
        break;

      case DicomRequestType_FindWorklist:
        allowFindWorklist_ = allowed;
        break;

      case DicomRequestType_Get:
        allowGet_ = allowed;
        break;

      case DicomRequestType_Move:
        allowMove_ = allowed;
        break;

      case DicomRequestType_Store:
        allowStore_ = allowed;
        break;

      case DicomRequestType_NAction:
        allowNAction_ = allowed;
        break;

      case DicomRequestType_NEventReport:
        allowNEventReport_ = allowed;
        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }


  bool RemoteModalityParameters::IsAdvancedFormatNeeded() const
  {
    return (!allowEcho_ ||
            !allowStore_ ||
            !allowFind_ ||
            !allowFindWorklist_ ||
            !allowGet_ ||
            !allowMove_ ||
            !allowNAction_ ||
            !allowNEventReport_ ||
            !allowTranscoding_ ||
            useDicomTls_ ||
            HasLocalAet());
  }

  
  void RemoteModalityParameters::Serialize(Json::Value& target,
                                           bool forceAdvancedFormat) const
  {
    if (forceAdvancedFormat ||
        IsAdvancedFormatNeeded())
    {
      target = Json::objectValue;
      target[KEY_AET] = aet_;
      target[KEY_HOST] = host_;
      target[KEY_PORT] = port_;
      target[KEY_MANUFACTURER] = EnumerationToString(manufacturer_);
      target[KEY_ALLOW_ECHO] = allowEcho_;
      target[KEY_ALLOW_STORE] = allowStore_;
      target[KEY_ALLOW_FIND] = allowFind_;
      target[KEY_ALLOW_FIND_WORKLIST] = allowFindWorklist_;
      target[KEY_ALLOW_GET] = allowGet_;
      target[KEY_ALLOW_MOVE] = allowMove_;
      target[KEY_ALLOW_N_ACTION] = allowNAction_;
      target[KEY_ALLOW_N_EVENT_REPORT] = allowNEventReport_;
      target[KEY_ALLOW_TRANSCODING] = allowTranscoding_;
      target[KEY_USE_DICOM_TLS] = useDicomTls_;
      target[KEY_LOCAL_AET] = localAet_;
      target[KEY_TIMEOUT] = timeout_;
    }
    else
    {
      target = Json::arrayValue;
      target.append(GetApplicationEntityTitle());
      target.append(GetHost());
      target.append(GetPortNumber());
      target.append(EnumerationToString(GetManufacturer()));
    }
  }

  
  void RemoteModalityParameters::Unserialize(const Json::Value& serialized)
  {
    Clear();

    switch (serialized.type())
    {
      case Json::objectValue:
        UnserializeObject(serialized);
        break;

      case Json::arrayValue:
        UnserializeArray(serialized);
        break;

      default:
        throw OrthancException(ErrorCode_BadFileFormat);
    }
  }

  bool RemoteModalityParameters::IsTranscodingAllowed() const
  {
    return allowTranscoding_;
  }

  void RemoteModalityParameters::SetTranscodingAllowed(bool allowed)
  {
    allowTranscoding_ = allowed;
  }

  bool RemoteModalityParameters::IsDicomTlsEnabled() const
  {
    return useDicomTls_;
  }

  void RemoteModalityParameters::SetDicomTlsEnabled(bool enabled)
  {
    useDicomTls_ = enabled;
  }

  bool RemoteModalityParameters::HasLocalAet() const
  {
    return !localAet_.empty();
  }

  const std::string& RemoteModalityParameters::GetLocalAet() const
  {
    if (localAet_.empty())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls, "You should have called HasLocalAet()");
    }
    else
    {
      return localAet_;
    }
  }

  void RemoteModalityParameters::SetLocalAet(const std::string& aet)
  {
    if (aet.empty())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
    else
    {
      localAet_ = aet;
    }
  }

  void RemoteModalityParameters::SetTimeout(uint32_t seconds)
  {
    timeout_ = seconds;
  }

  uint32_t RemoteModalityParameters::GetTimeout() const
  {
    return timeout_;
  }

  bool RemoteModalityParameters::HasTimeout() const
  {
    return timeout_ != 0;
  }
}
