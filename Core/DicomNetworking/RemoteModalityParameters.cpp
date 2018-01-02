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

#include <boost/lexical_cast.hpp>
#include <stdexcept>

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
    SetPort(port);
    SetManufacturer(manufacturer);
  }


  void RemoteModalityParameters::FromJson(const Json::Value& modality)
  {
    if (!modality.isArray() ||
        (modality.size() != 3 && modality.size() != 4))
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    SetApplicationEntityTitle(modality.get(0u, "").asString());
    SetHost(modality.get(1u, "").asString());

    const Json::Value& portValue = modality.get(2u, "");
    try
    {
      int tmp = portValue.asInt();

      if (tmp <= 0 || tmp >= 65535)
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange);
      }

      SetPort(static_cast<uint16_t>(tmp));
    }
    catch (std::runtime_error /* error inside JsonCpp */)
    {
      try
      {
        SetPort(boost::lexical_cast<uint16_t>(portValue.asString()));
      }
      catch (boost::bad_lexical_cast)
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }
    }

    if (modality.size() == 4)
    {
      const std::string& manufacturer = modality.get(3u, "").asString();

      try
      {
        SetManufacturer(manufacturer);
      }
      catch (OrthancException&)
      {
        LOG(ERROR) << "Unknown modality manufacturer: \"" << manufacturer << "\"";
        throw;
      }
    }
    else
    {
      SetManufacturer(ModalityManufacturer_Generic);
    }
  }

  void RemoteModalityParameters::ToJson(Json::Value& value) const
  {
    value = Json::arrayValue;
    value.append(GetApplicationEntityTitle());
    value.append(GetHost());
    value.append(GetPort());
    value.append(EnumerationToString(GetManufacturer()));
  }
}
