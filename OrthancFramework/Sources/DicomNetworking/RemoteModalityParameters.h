/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
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


#pragma once

#include "../Enumerations.h"

#include <stdint.h>
#include <string>
#include <json/json.h>

namespace Orthanc
{
  class ORTHANC_PUBLIC RemoteModalityParameters
  {
  private:
    std::string           aet_;
    std::string           host_;
    uint16_t              port_;
    ModalityManufacturer  manufacturer_;
    bool                  allowEcho_;
    bool                  allowStore_;
    bool                  allowFind_;
    bool                  allowMove_;
    bool                  allowGet_;
    bool                  allowNAction_;
    bool                  allowNEventReport_;
    bool                  allowTranscoding_;
    
    void Clear();

    void UnserializeArray(const Json::Value& serialized);

    void UnserializeObject(const Json::Value& serialized);

  public:
    RemoteModalityParameters()
    {
      Clear();
    }

    explicit RemoteModalityParameters(const Json::Value& serialized)
    {
      Unserialize(serialized);
    }

    RemoteModalityParameters(const std::string& aet,
                             const std::string& host,
                             uint16_t port,
                             ModalityManufacturer manufacturer);

    const std::string& GetApplicationEntityTitle() const
    {
      return aet_;
    }

    void SetApplicationEntityTitle(const std::string& aet)
    {
      aet_ = aet;
    }

    const std::string& GetHost() const
    {
      return host_;
    }

    void SetHost(const std::string& host)
    {
      host_ = host;
    }
    
    uint16_t GetPortNumber() const
    {
      return port_;
    }

    void SetPortNumber(uint16_t port);

    ModalityManufacturer GetManufacturer() const
    {
      return manufacturer_;
    }

    void SetManufacturer(ModalityManufacturer manufacturer)
    {
      manufacturer_ = manufacturer;
    }    

    void SetManufacturer(const std::string& manufacturer)
    {
      manufacturer_ = StringToModalityManufacturer(manufacturer);
    }

    bool IsRequestAllowed(DicomRequestType type) const;

    void SetRequestAllowed(DicomRequestType type,
                           bool allowed);

    void Unserialize(const Json::Value& modality);

    bool IsAdvancedFormatNeeded() const;

    void Serialize(Json::Value& target,
                   bool forceAdvancedFormat) const;

    bool IsTranscodingAllowed() const
    {
      return allowTranscoding_;
    }

    void SetTranscodingAllowed(bool allowed)
    {
      allowTranscoding_ = allowed;
    }
  };
}
