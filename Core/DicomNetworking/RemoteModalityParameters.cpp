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
 * In addition, as a special exception, the copyright holders of this
 * program give permission to link the code of its release with the
 * OpenSSL project's "OpenSSL" library (or with modified versions of it
 * that use the same license as the "OpenSSL" library), and distribute
 * the linked executables. You must obey the GNU General Public License
 * in all respects for all of the code used other than "OpenSSL". If you
 * modify file(s) with this exception, you may extend this exception to
 * your version of the file(s), but you are not obligated to do so. If
 * you do not wish to do so, delete this exception statement from your
 * version. If you delete this exception statement from all source files
 * in the program, then also delete it here.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#include "../PrecompiledHeaders.h"
#include "RemoteModalityParameters.h"

#include "../Logging.h"
#include "../OrthancException.h"
#include "../SerializationToolbox.h"

#include <boost/lexical_cast.hpp>
#include <stdexcept>


static const char* KEY_AET = "AET";
static const char* KEY_MANUFACTURER = "Manufacturer";
static const char* KEY_PORT = "Port";
static const char* KEY_HOST = "Host";


namespace Orthanc
{
  RemoteModalityParameters::RemoteModalityParameters() :
    aet_("ORTHANC"),
    host_("127.0.0.1"),
    port_(104),
    manufacturer_(ModalityManufacturer_Generic)
  {
  }

  RemoteModalityParameters::RemoteModalityParameters(const std::string& aet,
                                                     const std::string& host,
                                                     uint16_t port,
                                                     ModalityManufacturer manufacturer)
  {
    SetApplicationEntityTitle(aet);
    SetHost(host);
    SetPortNumber(port);
    SetManufacturer(manufacturer);
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

    if (tmp <= 0 || tmp >= 65535)
    {
      LOG(ERROR) << "A TCP port number must be in range [1..65534], but found: " << tmp;
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
    else
    {
      return static_cast<uint16_t>(tmp);
    }
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
  }


  bool RemoteModalityParameters::IsAdvancedFormatNeeded() const
  {
    return false; // TODO
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
}
