/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
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
#include <json/value.h>

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
    bool                  useDicomTls_;
    std::string           localAet_;
    uint32_t              timeout_;
    
    void Clear();

    void UnserializeArray(const Json::Value& serialized);

    void UnserializeObject(const Json::Value& serialized);

  public:
    RemoteModalityParameters();

    explicit RemoteModalityParameters(const Json::Value& serialized);

    RemoteModalityParameters(const std::string& aet,
                             const std::string& host,
                             uint16_t port,
                             ModalityManufacturer manufacturer);

    const std::string& GetApplicationEntityTitle() const;

    void SetApplicationEntityTitle(const std::string& aet);

    const std::string& GetHost() const;

    void SetHost(const std::string& host);
    
    uint16_t GetPortNumber() const;

    void SetPortNumber(uint16_t port);

    ModalityManufacturer GetManufacturer() const;

    void SetManufacturer(ModalityManufacturer manufacturer);

    void SetManufacturer(const std::string& manufacturer);

    bool IsRequestAllowed(DicomRequestType type) const;

    void SetRequestAllowed(DicomRequestType type,
                           bool allowed);

    void Unserialize(const Json::Value& modality);

    bool IsAdvancedFormatNeeded() const;

    void Serialize(Json::Value& target,
                   bool forceAdvancedFormat) const;

    bool IsTranscodingAllowed() const;

    void SetTranscodingAllowed(bool allowed);

    bool IsDicomTlsEnabled() const;

    void SetDicomTlsEnabled(bool enabled);

    bool HasLocalAet() const;

    const std::string& GetLocalAet() const;

    void SetLocalAet(const std::string& aet);

    // Setting it to "0" will use "DicomAssociationParameters::GetDefaultTimeout()"
    void SetTimeout(uint32_t seconds);

    uint32_t GetTimeout() const;

    bool HasTimeout() const;    
  };
}
