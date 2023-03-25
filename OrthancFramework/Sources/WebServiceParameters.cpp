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


#include "PrecompiledHeaders.h"
#include "WebServiceParameters.h"

#include "Logging.h"
#include "OrthancException.h"
#include "SerializationToolbox.h"
#include "Toolbox.h"

#if ORTHANC_SANDBOXED == 0
#  include "SystemToolbox.h"
#endif

#include <boost/algorithm/string/find.hpp>
#include <cassert>

namespace Orthanc
{
  static const char* KEY_CERTIFICATE_FILE = "CertificateFile";
  static const char* KEY_CERTIFICATE_KEY_FILE = "CertificateKeyFile";
  static const char* KEY_CERTIFICATE_KEY_PASSWORD = "CertificateKeyPassword";
  static const char* KEY_HTTP_HEADERS = "HttpHeaders";
  static const char* KEY_PASSWORD = "Password";
  static const char* KEY_PKCS11 = "Pkcs11";
  static const char* KEY_URL = "Url";
  static const char* KEY_URL_2 = "URL";
  static const char* KEY_USERNAME = "Username";
  static const char* KEY_TIMEOUT = "Timeout";


  static bool IsReservedKey(const std::string& key)
  {
    return (key == KEY_CERTIFICATE_FILE ||
            key == KEY_CERTIFICATE_KEY_FILE ||
            key == KEY_CERTIFICATE_KEY_PASSWORD ||
            key == KEY_HTTP_HEADERS ||
            key == KEY_PASSWORD ||
            key == KEY_PKCS11 ||
            key == KEY_URL ||
            key == KEY_URL_2 ||
            key == KEY_USERNAME ||
            key == KEY_TIMEOUT);
  }


  WebServiceParameters::WebServiceParameters() : 
    pkcs11Enabled_(false),
    timeout_(0)
  {
    SetUrl("http://127.0.0.1:8042/");
  }

  WebServiceParameters::WebServiceParameters(const Json::Value &serialized)
  {
    Unserialize(serialized);
  }

  const std::string &WebServiceParameters::GetUrl() const
  {
    return url_;
  }


  void WebServiceParameters::ClearClientCertificate()
  {
    certificateFile_.clear();
    certificateKeyFile_.clear();
    certificateKeyPassword_.clear();
  }


  void WebServiceParameters::SetUrl(const std::string& url)
  {
    if (boost::find_first(url, "://"))
    {
      // Only allow the HTTP and HTTPS protocols
      if (!Toolbox::StartsWith(url, "http://") &&
          !Toolbox::StartsWith(url, "https://"))
      {
        throw OrthancException(ErrorCode_BadFileFormat, "Bad URL: " + url);
      }
    }

    if (url.empty())
    {
      throw OrthancException(ErrorCode_BadFileFormat, "Empty URL");
    }

    // Add trailing slash if needed
    if (url[url.size() - 1] == '/')
    {
      url_ = url;
    }
    else
    {
      url_ = url + '/';
    }
  }


  void WebServiceParameters::ClearCredentials()
  {
    username_.clear();
    password_.clear();
  }


  void WebServiceParameters::SetCredentials(const std::string& username,
                                            const std::string& password)
  {
    if (username.empty() && 
        !password.empty())
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }
    else
    {
      username_ = username;
      password_ = password;
    }
  }

  const std::string &WebServiceParameters::GetUsername() const
  {
    return username_;
  }

  const std::string &WebServiceParameters::GetPassword() const
  {
    return password_;
  }


  void WebServiceParameters::SetClientCertificate(const std::string& certificateFile,
                                                  const std::string& certificateKeyFile,
                                                  const std::string& certificateKeyPassword)
  {
    if (certificateFile.empty())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    if (certificateKeyPassword.empty())
    {
      LOG(WARNING) << "No password specified for certificate key file: " << certificateKeyFile;
    }

    certificateFile_ = certificateFile;
    certificateKeyFile_ = certificateKeyFile;
    certificateKeyPassword_ = certificateKeyPassword;
  }

  const std::string &WebServiceParameters::GetCertificateFile() const
  {
    return certificateFile_;
  }

  const std::string &WebServiceParameters::GetCertificateKeyFile() const
  {
    return certificateKeyFile_;
  }

  const std::string &WebServiceParameters::GetCertificateKeyPassword() const
  {
    return certificateKeyPassword_;
  }

  void WebServiceParameters::SetPkcs11Enabled(bool enabled)
  {
    pkcs11Enabled_ = enabled;
  }

  bool WebServiceParameters::IsPkcs11Enabled() const
  {
    return pkcs11Enabled_;
  }

  void WebServiceParameters::AddHttpHeader(const std::string &key, const std::string &value)
  {
    headers_[key] = value;
  }

  void WebServiceParameters::ClearHttpHeaders()
  {
    headers_.clear();
  }

  const WebServiceParameters::Dictionary &WebServiceParameters::GetHttpHeaders() const
  {
    return headers_;
  }


  void WebServiceParameters::FromSimpleFormat(const Json::Value& peer)
  {
    assert(peer.isArray());

    pkcs11Enabled_ = false;
    timeout_ = 0;
    ClearClientCertificate();

    if (peer.size() != 1 && 
        peer.size() != 3)
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    SetUrl(peer.get(0u, "").asString());

    if (peer.size() == 1)
    {
      ClearCredentials();
    }
    else if (peer.size() == 2)
    {
      throw OrthancException(ErrorCode_BadFileFormat,
                             "The HTTP password is not provided");
    }
    else if (peer.size() == 3)
    {
      SetCredentials(peer.get(1u, "").asString(),
                     peer.get(2u, "").asString());
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


  void WebServiceParameters::FromAdvancedFormat(const Json::Value& peer)
  {
    assert(peer.isObject());

    std::string url = GetStringMember(peer, KEY_URL, "");
    if (url.empty())
    {
      SetUrl(GetStringMember(peer, KEY_URL_2, ""));
    }
    else
    {
      SetUrl(url);
    }

    SetCredentials(GetStringMember(peer, KEY_USERNAME, ""),
                   GetStringMember(peer, KEY_PASSWORD, ""));

    std::string file = GetStringMember(peer, KEY_CERTIFICATE_FILE, "");
    if (!file.empty())
    {
      SetClientCertificate(file, GetStringMember(peer, KEY_CERTIFICATE_KEY_FILE, ""),
                           GetStringMember(peer, KEY_CERTIFICATE_KEY_PASSWORD, ""));
    }
    else
    {
      ClearClientCertificate();
    }

    if (peer.isMember(KEY_PKCS11))
    {
      if (peer[KEY_PKCS11].type() == Json::booleanValue)
      {
        pkcs11Enabled_ = peer[KEY_PKCS11].asBool();
      }
      else
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }
    }
    else
    {
      pkcs11Enabled_ = false;
    }


    headers_.clear();

    if (peer.isMember(KEY_HTTP_HEADERS))
    {
      const Json::Value& h = peer[KEY_HTTP_HEADERS];
      if (h.type() != Json::objectValue)
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }
      else
      {
        Json::Value::Members keys = h.getMemberNames();
        for (size_t i = 0; i < keys.size(); i++)
        {
          const Json::Value& value = h[keys[i]];
          if (value.type() != Json::stringValue)
          {
            throw OrthancException(ErrorCode_BadFileFormat);
          }
          else
          {
            headers_[keys[i]] = value.asString();
          }
        }
      }
    }


    userProperties_.clear();

    const Json::Value::Members members = peer.getMemberNames();

    for (Json::Value::Members::const_iterator it = members.begin(); 
         it != members.end(); ++it)
    {
      if (!IsReservedKey(*it))
      {
        switch (peer[*it].type())
        {
          case Json::stringValue:
            userProperties_[*it] = peer[*it].asString();
            break;

          case Json::booleanValue:
            userProperties_[*it] = peer[*it].asBool() ? "1" : "0";
            break;

          case Json::intValue:
            userProperties_[*it] = boost::lexical_cast<std::string>(peer[*it].asInt());
            break;

          default:
            throw OrthancException(ErrorCode_BadFileFormat,
                                   "User-defined properties associated with a Web service must be strings: " + *it);
        }
      }
    }


    if (peer.isMember(KEY_TIMEOUT))
    {
      timeout_ = SerializationToolbox::ReadUnsignedInteger(peer, KEY_TIMEOUT);      
    }
    else
    {
      timeout_ = 0;
    }
  }


  void WebServiceParameters::Unserialize(const Json::Value& peer)
  {
    try
    {
      if (peer.isArray())
      {
        FromSimpleFormat(peer);
      }
      else if (peer.isObject())
      {
        FromAdvancedFormat(peer);
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


  void WebServiceParameters::ListHttpHeaders(std::set<std::string>& target) const
  {
    target.clear();

    for (Dictionary::const_iterator it = headers_.begin();
         it != headers_.end(); ++it)
    {
      target.insert(it->first);
    }
  }


  bool WebServiceParameters::LookupHttpHeader(std::string& value,
                                              const std::string& key) const
  {
    Dictionary::const_iterator found = headers_.find(key);

    if (found == headers_.end())
    {
      return false;
    }
    else
    {
      value = found->second;
      return true;
    }
  }


  void WebServiceParameters::AddUserProperty(const std::string& key,
                                             const std::string& value)
  {
    if (IsReservedKey(key))
    {
      throw OrthancException(
        ErrorCode_ParameterOutOfRange,
        "Cannot use this reserved key to name an user property: " + key);
    }
    else
    {
      userProperties_[key] = value;
    }
  }

  void WebServiceParameters::ClearUserProperties()
  {
    userProperties_.clear();
  }

  const WebServiceParameters::Dictionary &WebServiceParameters::GetUserProperties() const
  {
    return userProperties_;
  }


  void WebServiceParameters::ListUserProperties(std::set<std::string>& target) const
  {
    target.clear();

    for (Dictionary::const_iterator it = userProperties_.begin();
         it != userProperties_.end(); ++it)
    {
      target.insert(it->first);
    }
  }


  bool WebServiceParameters::LookupUserProperty(std::string& value,
                                                const std::string& key) const
  {
    Dictionary::const_iterator found = userProperties_.find(key);

    if (found == userProperties_.end())
    {
      return false;
    }
    else
    {
      value = found->second;
      return true;
    }
  }
  

  bool WebServiceParameters::GetBooleanUserProperty(const std::string& key,
                                                    bool defaultValue) const
  {
    Dictionary::const_iterator found = userProperties_.find(key);

    if (found == userProperties_.end())
    {
      return defaultValue;
    }
    else
    {
      bool value;
      if (SerializationToolbox::ParseBoolean(value, found->second))
      {
        return value;
      }
      else
      {
        throw OrthancException(ErrorCode_BadFileFormat, "Bad value for a Boolean user property in the parameters "
                               "of a Web service: Property \"" + key + "\" equals: " + found->second);
      }
    }
  }


  bool WebServiceParameters::IsAdvancedFormatNeeded() const
  {
    return (!certificateFile_.empty() ||
            !certificateKeyFile_.empty() ||
            !certificateKeyPassword_.empty() ||
            pkcs11Enabled_ ||
            !headers_.empty() ||
            !userProperties_.empty() ||
            timeout_ != 0);
  }


  void WebServiceParameters::Serialize(Json::Value& value,
                                       bool forceAdvancedFormat,
                                       bool includePasswords) const
  {
    if (forceAdvancedFormat ||
        IsAdvancedFormatNeeded())
    {
      value = Json::objectValue;
      value[KEY_URL] = url_;

      if (!username_.empty() ||
          !password_.empty())
      {
        value[KEY_USERNAME] = username_;

        if (includePasswords)
        {
          value[KEY_PASSWORD] = password_;
        }
      }

      if (!certificateFile_.empty())
      {
        value[KEY_CERTIFICATE_FILE] = certificateFile_;
      }

      if (!certificateKeyFile_.empty())
      {
        value[KEY_CERTIFICATE_KEY_FILE] = certificateKeyFile_;
      }

      if (!certificateKeyPassword_.empty() &&
          includePasswords)
      {
        value[KEY_CERTIFICATE_KEY_PASSWORD] = certificateKeyPassword_;
      }

      value[KEY_PKCS11] = pkcs11Enabled_;
      value[KEY_TIMEOUT] = timeout_;

      value[KEY_HTTP_HEADERS] = Json::objectValue;
      for (Dictionary::const_iterator it = headers_.begin();
           it != headers_.end(); ++it)
      {
        value[KEY_HTTP_HEADERS][it->first] = it->second;
      }

      for (Dictionary::const_iterator it = userProperties_.begin();
           it != userProperties_.end(); ++it)
      {
        value[it->first] = it->second;
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
        value.append(includePasswords ? password_ : "");
      }
    }
  }


#if ORTHANC_SANDBOXED == 0
  void WebServiceParameters::CheckClientCertificate() const
  {
    if (!certificateFile_.empty())
    {
      if (!SystemToolbox::IsRegularFile(certificateFile_))
      {
        throw OrthancException(ErrorCode_InexistentFile,
                               "Cannot open certificate file: " + certificateFile_);
      }

      if (!certificateKeyFile_.empty() && 
          !SystemToolbox::IsRegularFile(certificateKeyFile_))
      {
        throw OrthancException(ErrorCode_InexistentFile,
                               "Cannot open key file: " + certificateKeyFile_);
      }
    }
  }
#endif


  void WebServiceParameters::FormatPublic(Json::Value& target) const
  {
    target = Json::objectValue;

    // Only return the public information identifying the destination.
    // "Security"-related information such as passwords and HTTP
    // headers are shown as "null" values.
    target[KEY_URL] = url_;

    if (!username_.empty())
    {
      target[KEY_USERNAME] = username_;
      target[KEY_PASSWORD] = Json::nullValue;
    }

    if (!certificateFile_.empty())
    {
      target[KEY_CERTIFICATE_FILE] = certificateFile_;
      target[KEY_CERTIFICATE_KEY_FILE] = Json::nullValue;
      target[KEY_CERTIFICATE_KEY_PASSWORD] = Json::nullValue;      
    }

    target[KEY_PKCS11] = pkcs11Enabled_;
    target[KEY_TIMEOUT] = timeout_;

    Json::Value headers = Json::arrayValue;
      
    for (Dictionary::const_iterator it = headers_.begin();
         it != headers_.end(); ++it)
    {
      // Only list the HTTP headers, not their value
      headers.append(it->first);
    }

    target[KEY_HTTP_HEADERS] = headers;

    for (Dictionary::const_iterator it = userProperties_.begin();
         it != userProperties_.end(); ++it)
    {
      target[it->first] = it->second;
    }
  }


  void WebServiceParameters::SetTimeout(uint32_t seconds)
  {
    timeout_ = seconds;
  }

  uint32_t WebServiceParameters::GetTimeout() const
  {
    return timeout_;
  }

  bool WebServiceParameters::HasTimeout() const
  {
    return (timeout_ != 0);
  }
}
