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


#include "PrecompiledHeaders.h"
#include "WebServiceParameters.h"

#include "Logging.h"
#include "OrthancException.h"
#include "SerializationToolbox.h"

#if ORTHANC_SANDBOXED == 0
#  include "../Core/SystemToolbox.h"
#endif

#include <cassert>

namespace Orthanc
{
  WebServiceParameters::WebServiceParameters() : 
    advancedFormat_(false),
    url_("http://127.0.0.1:8042/"),
    pkcs11Enabled_(false)
  {
  }


  void WebServiceParameters::ClearClientCertificate()
  {
    certificateFile_.clear();
    certificateKeyFile_.clear();
    certificateKeyPassword_.clear();
  }


#if ORTHANC_SANDBOXED == 0
  void WebServiceParameters::SetClientCertificate(const std::string& certificateFile,
                                                  const std::string& certificateKeyFile,
                                                  const std::string& certificateKeyPassword)
  {
    if (certificateFile.empty())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    if (!SystemToolbox::IsRegularFile(certificateFile))
    {
      LOG(ERROR) << "Cannot open certificate file: " << certificateFile;
      throw OrthancException(ErrorCode_InexistentFile);
    }

    if (!certificateKeyFile.empty() && 
        !SystemToolbox::IsRegularFile(certificateKeyFile))
    {
      LOG(ERROR) << "Cannot open key file: " << certificateKeyFile;
      throw OrthancException(ErrorCode_InexistentFile);
    }

    advancedFormat_ = true;
    certificateFile_ = certificateFile;
    certificateKeyFile_ = certificateKeyFile;
    certificateKeyPassword_ = certificateKeyPassword;
  }
#endif


  static void AddTrailingSlash(std::string& url)
  {
    if (url.size() != 0 && 
        url[url.size() - 1] != '/')
    {
      url += '/';
    }
  }


  void WebServiceParameters::FromJsonArray(const Json::Value& peer)
  {
    assert(peer.isArray());

    advancedFormat_ = false;
    pkcs11Enabled_ = false;

    if (peer.size() != 1 && 
        peer.size() != 3)
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    std::string url = peer.get(0u, "").asString();
    if (url.empty())
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    AddTrailingSlash(url);
    SetUrl(url);

    if (peer.size() == 1)
    {
      SetUsername("");
      SetPassword("");
    }
    else if (peer.size() == 2)
    {
      LOG(ERROR) << "The HTTP password is not provided";
      throw OrthancException(ErrorCode_BadFileFormat);
    }
    else if (peer.size() == 3)
    {
      SetUsername(peer.get(1u, "").asString());
      SetPassword(peer.get(2u, "").asString());
    }
    else
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }
  }


  static std::string GetStringMember(const Json::Value& peer,
                                     const std::string& key,
                                     const std::string& defaultValue)
  {
    if (!peer.isMember(key))
    {
      return defaultValue;
    }
    else if (peer[key].type() != Json::stringValue)
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }
    else
    {
      return peer[key].asString();
    }
  }


  void WebServiceParameters::FromJsonObject(const Json::Value& peer)
  {
    assert(peer.isObject());
    advancedFormat_ = true;

    std::string url = GetStringMember(peer, "Url", "");
    if (url.empty())
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    AddTrailingSlash(url);
    SetUrl(url);

    SetUsername(GetStringMember(peer, "Username", ""));
    SetPassword(GetStringMember(peer, "Password", ""));

    if (!username_.empty() &&
        !peer.isMember("Password"))
    {
      LOG(ERROR) << "The HTTP password is not provided";
      throw OrthancException(ErrorCode_BadFileFormat);      
    }

#if ORTHANC_SANDBOXED == 0
    if (peer.isMember("CertificateFile"))
    {
      SetClientCertificate(GetStringMember(peer, "CertificateFile", ""),
                           GetStringMember(peer, "CertificateKeyFile", ""),
                           GetStringMember(peer, "CertificateKeyPassword", ""));

      if (!peer.isMember("CertificateKeyPassword"))
      {
        LOG(ERROR) << "The password for the HTTPS certificate is not provided";
        throw OrthancException(ErrorCode_BadFileFormat);      
      }
    }
#endif

    if (peer.isMember("Pkcs11"))
    {
      if (peer["Pkcs11"].type() == Json::booleanValue)
      {
        pkcs11Enabled_ = peer["Pkcs11"].asBool();
      }
      else
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }
    }
  }


  void WebServiceParameters::FromJson(const Json::Value& peer)
  {
    try
    {
      if (peer.isArray())
      {
        FromJsonArray(peer);
      }
      else if (peer.isObject())
      {
        FromJsonObject(peer);
      }
      else
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }
    }
    catch (OrthancException&)
    {
      throw;
    }
    catch (...)
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }
  }


  void WebServiceParameters::ToJson(Json::Value& value,
                                    bool includePasswords) const
  {
    if (advancedFormat_)
    {
      value = Json::objectValue;
      value["Url"] = url_;

      if (!username_.empty() ||
          !password_.empty())
      {
        value["Username"] = username_;

        if (includePasswords)
        {
          value["Password"] = password_;
        }
      }

      if (!certificateFile_.empty())
      {
        value["CertificateFile"] = certificateFile_;
      }

      if (!certificateKeyFile_.empty())
      {
        value["CertificateKeyFile"] = certificateKeyFile_;
      }

      if (!certificateKeyPassword_.empty() &&
          includePasswords)
      {
        value["CertificateKeyPassword"] = certificateKeyPassword_;
      }
    }
    else
    {
      value = Json::arrayValue;
      value.append(url_);

      if (!username_.empty() ||
          !password_.empty())
      {
        value.append(username_);

        if (includePasswords)
        {
          value.append(password_);
        }
      }
    }
  }

  
  void WebServiceParameters::Serialize(Json::Value& target) const
  {
    target = Json::objectValue;
    target["URL"] = url_;
    target["Username"] = username_;
    target["Password"] = password_;
    target["CertificateFile"] = certificateFile_;
    target["CertificateKeyFile"] = certificateKeyFile_;
    target["CertificateKeyPassword"] = certificateKeyPassword_;
    target["PKCS11"] = pkcs11Enabled_;
    target["AdvancedFormat"] = advancedFormat_;
  }

  
  WebServiceParameters::WebServiceParameters(const Json::Value& serialized) :
    advancedFormat_(true)
  {
    url_ = SerializationToolbox::ReadString(serialized, "URL");
    username_ = SerializationToolbox::ReadString(serialized, "Username");
    password_ = SerializationToolbox::ReadString(serialized, "Password");

    std::string a, b, c;
    a = SerializationToolbox::ReadString(serialized, "CertificateFile");
    b = SerializationToolbox::ReadString(serialized, "CertificateKeyFile");
    c = SerializationToolbox::ReadString(serialized, "CertificateKeyPassword");

    if (!a.empty())
    {
      SetClientCertificate(a, b, c);
    }
    
    pkcs11Enabled_ = SerializationToolbox::ReadBoolean(serialized, "PKCS11");
    advancedFormat_ = SerializationToolbox::ReadBoolean(serialized, "AdvancedFormat");
  }
}
