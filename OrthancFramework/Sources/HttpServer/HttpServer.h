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

// To have ORTHANC_ENABLE_xxx defined if using the shared library
#include "../OrthancFramework.h"

#if !defined(ORTHANC_ENABLE_MONGOOSE)
#  error Macro ORTHANC_ENABLE_MONGOOSE must be defined to include this file
#endif

#if !defined(ORTHANC_ENABLE_CIVETWEB)
#  error Macro ORTHANC_ENABLE_CIVETWEB must be defined to include this file
#endif

#if (ORTHANC_ENABLE_MONGOOSE == 0 &&            \
     ORTHANC_ENABLE_CIVETWEB == 0)
#  error Either ORTHANC_ENABLE_MONGOOSE or ORTHANC_ENABLE_CIVETWEB must be set to 1
#endif

#if !defined(ORTHANC_ENABLE_PUGIXML)
#  error The macro ORTHANC_ENABLE_PUGIXML must be defined
#endif

#if ORTHANC_ENABLE_PUGIXML == 1
#  include "IWebDavBucket.h"
#endif


#include "IIncomingHttpRequestFilter.h"

#include <list>
#include <map>
#include <set>
#include <stdint.h>
#include <boost/shared_ptr.hpp>

namespace Orthanc
{
  class ChunkStore;
  class OrthancException;

  class IHttpExceptionFormatter : public boost::noncopyable
  {
  public:
    virtual ~IHttpExceptionFormatter()
    {
    }

    virtual void Format(HttpOutput& output,
                        const OrthancException& exception,
                        HttpMethod method,
                        const char* uri) = 0;
  };


  class ORTHANC_PUBLIC HttpServer : public boost::noncopyable
  {
  public:
#if ORTHANC_ENABLE_PUGIXML == 1
    typedef std::map<std::string, IWebDavBucket*>  WebDavBuckets;
#endif
    
  private:
    // http://stackoverflow.com/questions/311166/stdauto-ptr-or-boostshared-ptr-for-pimpl-idiom
    struct PImpl;
    boost::shared_ptr<PImpl> pimpl_;

    IHttpHandler *handler_;

    typedef std::set<std::string> RegisteredUsers;
    RegisteredUsers registeredUsers_;

    bool remoteAllowed_;
    bool authentication_;
    bool sslVerifyPeers_;
    std::string trustedClientCertificates_;
    bool ssl_;
    std::string certificate_;
    uint16_t port_;
    IIncomingHttpRequestFilter* filter_;
    bool keepAlive_;
    bool httpCompression_;
    IHttpExceptionFormatter* exceptionFormatter_;
    std::string realm_;
    unsigned int threadsCount_;
    bool tcpNoDelay_;
    unsigned int requestTimeout_;  // In seconds

#if ORTHANC_ENABLE_PUGIXML == 1
    WebDavBuckets webDavBuckets_;
#endif
    
    bool IsRunning() const;

  public:
    HttpServer();

    ~HttpServer();

    void SetPortNumber(uint16_t port);

    uint16_t GetPortNumber() const
    {
      return port_;
    }

    void Start();

    void Stop();

    void ClearUsers();

    void RegisterUser(const char* username,
                      const char* password);

    bool IsAuthenticationEnabled() const
    {
      return authentication_;
    }

    void SetAuthenticationEnabled(bool enabled);

    bool IsSslEnabled() const
    {
      return ssl_;
    }

    void SetSslEnabled(bool enabled);

    void SetSslVerifyPeers(bool enabled);

    void SetSslTrustedClientCertificates(const char* path);

    bool IsKeepAliveEnabled() const
    {
      return keepAlive_;
    }

    void SetKeepAliveEnabled(bool enabled);

    const std::string& GetSslCertificate() const
    {
      return certificate_;
    }

    void SetSslCertificate(const char* path);

    bool IsRemoteAccessAllowed() const
    {
      return remoteAllowed_;
    }

    void SetRemoteAccessAllowed(bool allowed);

    bool IsHttpCompressionEnabled() const
    {
      return httpCompression_;;
    }

    void SetHttpCompressionEnabled(bool enabled);

    IIncomingHttpRequestFilter* GetIncomingHttpRequestFilter() const
    {
      return filter_;
    }

    void SetIncomingHttpRequestFilter(IIncomingHttpRequestFilter& filter);

    ChunkStore& GetChunkStore();

    bool IsValidBasicHttpAuthentication(const std::string& basic) const;

    void Register(IHttpHandler& handler);

    bool HasHandler() const
    {
      return handler_ != NULL;
    }

    IHttpHandler& GetHandler() const;

    void SetHttpExceptionFormatter(IHttpExceptionFormatter& formatter);

    IHttpExceptionFormatter* GetExceptionFormatter()
    {
      return exceptionFormatter_;
    }

    const std::string& GetRealm() const
    {
      return realm_;
    }

    void SetRealm(const std::string& realm)
    {
      realm_ = realm;
    }

    void SetThreadsCount(unsigned int threads);

    unsigned int GetThreadsCount() const
    {
      return threadsCount_;
    }

    // New in Orthanc 1.5.2, not available for Mongoose
    void SetTcpNoDelay(bool tcpNoDelay);

    bool IsTcpNoDelay() const
    {
      return tcpNoDelay_;
    }

    void SetRequestTimeout(unsigned int seconds);

    unsigned int GetRequestTimeout() const
    {
      return requestTimeout_;
    }

#if ORTHANC_ENABLE_PUGIXML == 1
    WebDavBuckets& GetWebDavBuckets()
    {
      return webDavBuckets_;
    }      
#endif

#if ORTHANC_ENABLE_PUGIXML == 1
    void Register(const std::vector<std::string>& root,
                  IWebDavBucket* bucket); // Takes ownership
#endif
  };
}
