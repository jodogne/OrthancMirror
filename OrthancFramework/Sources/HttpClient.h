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

#include "Enumerations.h"
#include "OrthancFramework.h"
#include "WebServiceParameters.h"

#include <string>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <json/value.h>

#if !defined(ORTHANC_ENABLE_CURL)
#  error The macro ORTHANC_ENABLE_CURL must be defined
#endif

#if ORTHANC_ENABLE_CURL != 1
#  error Support for curl is disabled, cannot use this file
#endif

#if !defined(ORTHANC_ENABLE_SSL)
#  error The macro ORTHANC_ENABLE_SSL must be defined
#endif

#if !defined(ORTHANC_ENABLE_PKCS11)
#  error The macro ORTHANC_ENABLE_PKCS11 must be defined
#endif


namespace Orthanc
{
  class ORTHANC_PUBLIC HttpClient : public boost::noncopyable
  {
  public:
    typedef std::map<std::string, std::string>  HttpHeaders;

    class IRequestBody : public boost::noncopyable
    {
    public:
      virtual ~IRequestBody()
      {
      }
      
      virtual bool ReadNextChunk(std::string& chunk) = 0;
    };

    class IAnswer : public boost::noncopyable
    {
    public:
      virtual ~IAnswer()
      {
      }

      virtual void AddHeader(const std::string& key,
                             const std::string& value) = 0;
      
      virtual void AddChunk(const void* data,
                            size_t size) = 0;
    };

  private:
    class CurlHeaders;
    class CurlRequestBody;
    class CurlAnswer;
    class DefaultAnswer;
    class GlobalParameters;

    struct PImpl;
    boost::shared_ptr<PImpl> pimpl_;

    std::string url_;
    std::string credentials_;
    HttpMethod method_;
    HttpStatus lastStatus_;
    std::string body_;  // This only makes sense for POST and PUT requests
    bool isVerbose_;
    long timeout_;
    std::string proxy_;
    bool verifyPeers_;
    std::string caCertificates_;
    std::string clientCertificateFile_;
    std::string clientCertificateKeyFile_;
    std::string clientCertificateKeyPassword_;
    bool pkcs11Enabled_;
    bool headersToLowerCase_;
    bool redirectionFollowed_;

    void Setup();

    void operator= (const HttpClient&);  // Assignment forbidden
    HttpClient(const HttpClient& base);  // Copy forbidden

    bool ApplyInternal(CurlAnswer& answer);

    bool ApplyInternal(std::string& answerBody,
                       HttpHeaders* answerHeaders);

    bool ApplyInternal(Json::Value& answerBody,
                       HttpHeaders* answerHeaders);

  public:
    HttpClient();

    HttpClient(const WebServiceParameters& service,
               const std::string& uri);

    ~HttpClient();

    void SetUrl(const char* url);

    void SetUrl(const std::string& url);

    const std::string& GetUrl() const;

    void SetMethod(HttpMethod method);

    HttpMethod GetMethod() const;

    void SetTimeout(long seconds);

    long GetTimeout() const;

    void SetBody(const std::string& data);

    std::string& GetBody();

    const std::string& GetBody() const;

    void SetBody(IRequestBody& body);

    void ClearBody();

    void SetVerbose(bool isVerbose);

    bool IsVerbose() const;

    void AddHeader(const std::string& key,
                   const std::string& value);

    void ClearHeaders();

    bool Apply(IAnswer& answer);

    bool Apply(std::string& answerBody);

    bool Apply(Json::Value& answerBody);

    bool Apply(std::string& answerBody,
               HttpHeaders& answerHeaders);

    bool Apply(Json::Value& answerBody,
               HttpHeaders& answerHeaders);

    HttpStatus GetLastStatus() const;

    void SetCredentials(const char* username,
                        const char* password);

    void SetProxy(const std::string& proxy);

    void SetHttpsVerifyPeers(bool verify);

    bool IsHttpsVerifyPeers() const;

    void SetHttpsCACertificates(const std::string& certificates);

    const std::string& GetHttpsCACertificates() const;

    void SetClientCertificate(const std::string& certificateFile,
                              const std::string& certificateKeyFile,
                              const std::string& certificateKeyPassword);

    void SetPkcs11Enabled(bool enabled);

    bool IsPkcs11Enabled() const;

    const std::string& GetClientCertificateFile() const;

    const std::string& GetClientCertificateKeyFile() const;

    const std::string& GetClientCertificateKeyPassword() const;

    void SetConvertHeadersToLowerCase(bool lowerCase);

    bool IsConvertHeadersToLowerCase() const;

    void SetRedirectionFollowed(bool follow);

    bool IsRedirectionFollowed() const;

    static void GlobalInitialize();
  
    static void GlobalFinalize();

    static void InitializePkcs11(const std::string& module,
                                 const std::string& pin,
                                 bool verbose);

    static void ConfigureSsl(bool httpsVerifyPeers,
                             const std::string& httpsCACertificates);

    static void SetDefaultVerbose(bool verbose);

    static void SetDefaultProxy(const std::string& proxy);

    static void SetDefaultTimeout(long timeout);

    void ApplyAndThrowException(IAnswer& answer);

    void ApplyAndThrowException(std::string& answerBody);

    void ApplyAndThrowException(Json::Value& answerBody);

    void ApplyAndThrowException(std::string& answerBody,
                                HttpHeaders& answerHeaders);

    void ApplyAndThrowException(Json::Value& answerBody,
                                HttpHeaders& answerHeaders);

    static void ThrowException(HttpStatus status);
  };
}
