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

#include "OrthancFramework.h"

#if !defined(ORTHANC_SANDBOXED)
#  error The macro ORTHANC_SANDBOXED must be defined
#endif

#include <map>
#include <set>
#include <stdint.h>
#include <string>
#include <json/value.h>

namespace Orthanc
{
  class ORTHANC_PUBLIC WebServiceParameters
  {
  public:
    typedef std::map<std::string, std::string>  Dictionary;

  private:
    std::string  url_;
    std::string  username_;
    std::string  password_;
    std::string  certificateFile_;
    std::string  certificateKeyFile_;
    std::string  certificateKeyPassword_;
    bool         pkcs11Enabled_;
    Dictionary   headers_;
    Dictionary   userProperties_;
    unsigned int timeout_;

    void FromSimpleFormat(const Json::Value& peer);

    void FromAdvancedFormat(const Json::Value& peer);

  public:
    WebServiceParameters();

    explicit WebServiceParameters(const Json::Value& serialized);

    const std::string& GetUrl() const;

    void SetUrl(const std::string& url);

    void ClearCredentials();

    void SetCredentials(const std::string& username,
                        const std::string& password);
    
    const std::string& GetUsername() const;

    const std::string& GetPassword() const;

    void ClearClientCertificate();

    void SetClientCertificate(const std::string& certificateFile,
                              const std::string& certificateKeyFile,
                              const std::string& certificateKeyPassword);

    const std::string& GetCertificateFile() const;

    const std::string& GetCertificateKeyFile() const;

    const std::string& GetCertificateKeyPassword() const;

    void SetPkcs11Enabled(bool enabled);

    bool IsPkcs11Enabled() const;

    void AddHttpHeader(const std::string& key,
                       const std::string& value);

    void ClearHttpHeaders();

    const Dictionary& GetHttpHeaders() const;

    void ListHttpHeaders(std::set<std::string>& target) const; 

    bool LookupHttpHeader(std::string& value,
                          const std::string& key) const; 

    void AddUserProperty(const std::string& key,
                         const std::string& value);

    void ClearUserProperties();

    const Dictionary& GetUserProperties() const;

    void ListUserProperties(std::set<std::string>& target) const; 

    bool LookupUserProperty(std::string& value,
                            const std::string& key) const;

    bool GetBooleanUserProperty(const std::string& key,
                                bool defaultValue) const;

    bool IsAdvancedFormatNeeded() const;

    void Unserialize(const Json::Value& peer);

    void Serialize(Json::Value& value,
                   bool forceAdvancedFormat,
                   bool includePasswords) const;

#if ORTHANC_SANDBOXED == 0
    void CheckClientCertificate() const;
#endif

    void FormatPublic(Json::Value& target) const;

    // Setting it to "0" will use "HttpClient::SetDefaultTimeout()"
    void SetTimeout(uint32_t seconds);

    uint32_t GetTimeout() const;

    bool HasTimeout() const;    
  };
}
