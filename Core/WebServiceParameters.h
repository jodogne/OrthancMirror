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


#pragma once

#if !defined(ORTHANC_SANDBOXED)
#  error The macro ORTHANC_SANDBOXED must be defined
#endif

#include <string>
#include <json/json.h>

namespace Orthanc
{
  class WebServiceParameters
  {
  private:
    bool        advancedFormat_;
    std::string url_;
    std::string username_;
    std::string password_;
    std::string certificateFile_;
    std::string certificateKeyFile_;
    std::string certificateKeyPassword_;
    bool        pkcs11Enabled_;

    void FromJsonArray(const Json::Value& peer);

    void FromJsonObject(const Json::Value& peer);

  public:
    WebServiceParameters();

    const std::string& GetUrl() const
    {
      return url_;
    }

    void SetUrl(const std::string& url)
    {
      url_ = url;
    }

    const std::string& GetUsername() const
    {
      return username_;
    }

    void SetUsername(const std::string& username)
    {
      username_ = username;
    }
    
    const std::string& GetPassword() const
    {
      return password_;
    }

    void SetPassword(const std::string& password)
    {
      password_ = password;
    }

    void ClearClientCertificate();

#if ORTHANC_SANDBOXED == 0
    void SetClientCertificate(const std::string& certificateFile,
                              const std::string& certificateKeyFile,
                              const std::string& certificateKeyPassword);
#endif

    const std::string& GetCertificateFile() const
    {
      return certificateFile_;
    }

    const std::string& GetCertificateKeyFile() const
    {
      return certificateKeyFile_;
    }

    const std::string& GetCertificateKeyPassword() const
    {
      return certificateKeyPassword_;
    }

    void SetPkcs11Enabled(bool pkcs11Enabled)
    {
      pkcs11Enabled_ = pkcs11Enabled;
    }

    bool IsPkcs11Enabled() const
    {
      return pkcs11Enabled_;
    }

    void FromJson(const Json::Value& peer);

    void ToJson(Json::Value& value) const;
  };
}
