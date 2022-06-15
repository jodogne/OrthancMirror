/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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
#include "HttpClient.h"

#include "Toolbox.h"
#include "OrthancException.h"
#include "Logging.h"
#include "ChunkedBuffer.h"
#include "SystemToolbox.h"

#include <string.h>
#include <curl/curl.h>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/thread/mutex.hpp>

// Default timeout = 60 seconds (in Orthanc <= 1.5.6, it was 10 seconds)
static const unsigned int DEFAULT_HTTP_TIMEOUT = 60;


#if ORTHANC_ENABLE_PKCS11 == 1
#  include "Pkcs11.h"
#endif


extern "C"
{
  static CURLcode GetHttpStatus(CURLcode code, CURL* curl, long* status)
  {
    if (code == CURLE_OK)
    {
      code = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, status);
      return code;
    }
    else
    {
      LOG(ERROR) << "Error code " << static_cast<int>(code)
                 << " in libcurl: " << curl_easy_strerror(code);
      *status = 0;
      return code;
    }
  }
}

// This is a dummy wrapper function to suppress any OpenSSL-related
// problem in valgrind. Inlining is prevented.
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline)) 
#endif
static CURLcode OrthancHttpClientPerformSSL(CURL* curl, long* status)
{
#if ORTHANC_ENABLE_SSL == 1
  return GetHttpStatus(curl_easy_perform(curl), curl, status);
#else
  throw Orthanc::OrthancException(Orthanc::ErrorCode_InternalError,
                                  "Orthanc was compiled without SSL support, "
                                  "cannot make HTTPS request");
#endif
}



namespace Orthanc
{
  static CURLcode CheckCode(CURLcode code)
  {
    if (code == CURLE_NOT_BUILT_IN)
    {
      throw OrthancException(ErrorCode_InternalError,
                             "Your libcurl does not contain a required feature, "
                             "please recompile Orthanc with -DUSE_SYSTEM_CURL=OFF");
    }

    if (code != CURLE_OK)
    {
      throw OrthancException(ErrorCode_NetworkProtocol,
                             "libCURL error: " + std::string(curl_easy_strerror(code)));
    }

    return code;
  }


  // RAII pattern around a "curl_slist"
  class HttpClient::CurlHeaders : public boost::noncopyable
  {
  private:
    struct curl_slist *content_;
    bool               isChunkedTransfer_;
    bool               hasExpect_;

  public:
    CurlHeaders() :
      content_(NULL),
      isChunkedTransfer_(false),
      hasExpect_(false)
    {
    }

    explicit CurlHeaders(const HttpClient::HttpHeaders& headers)
    {
      for (HttpClient::HttpHeaders::const_iterator
             it = headers.begin(); it != headers.end(); ++it)
      {
        AddHeader(it->first, it->second);
      }
    }

    ~CurlHeaders()
    {
      Clear();
    }

    bool IsEmpty() const
    {
      return content_ == NULL;
    }

    void Clear()
    {
      if (content_ != NULL)
      {
        curl_slist_free_all(content_);
        content_ = NULL;
      }

      isChunkedTransfer_ = false;
      hasExpect_ = false;
    }

    void AddHeader(const std::string& key,
                   const std::string& value)
    {
      if (boost::iequals(key, "Expect"))
      {
        hasExpect_ = true;
      }

      if (boost::iequals(key, "Transfer-Encoding") &&
          value == "chunked")
      {
        isChunkedTransfer_ = true;
      }
        
      std::string item = key + ": " + value;

      struct curl_slist *tmp = curl_slist_append(content_, item.c_str());
        
      if (tmp == NULL)
      {
        throw OrthancException(ErrorCode_NotEnoughMemory);
      }
      else
      {
        content_ = tmp;
      }
    }

    void Assign(CURL* curl) const
    {
      CheckCode(curl_easy_setopt(curl, CURLOPT_HTTPHEADER, content_));
    }

    bool HasExpect() const
    {
      return hasExpect_;
    }

    bool IsChunkedTransfer() const
    {
      return isChunkedTransfer_;
    }
  };


  class HttpClient::CurlRequestBody : public boost::noncopyable
  {
  private:
    HttpClient::IRequestBody*  body_;
    std::string                pending_;
    size_t                     pendingPos_;

    size_t CallbackInternal(char* curlBuffer,
                            size_t curlBufferSize)
    {
      if (body_ == NULL)
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }

      if (curlBufferSize == 0)
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      if (pendingPos_ + curlBufferSize <= pending_.size())
      {
        assert(sizeof(char) == 1);
        memcpy(curlBuffer, &pending_[pendingPos_], curlBufferSize);
        pendingPos_ += curlBufferSize;
        return curlBufferSize;
      }
      else
      {
        ChunkedBuffer buffer;
        buffer.SetPendingBufferSize(curlBufferSize);

        if (pendingPos_ < pending_.size())
        {
          buffer.AddChunk(&pending_[pendingPos_], pending_.size() - pendingPos_);
        }
        
        // Read chunks from the body stream so as to fill the target buffer
        std::string chunk;
        
        while (buffer.GetNumBytes() < curlBufferSize &&
               body_->ReadNextChunk(chunk))
        {
          buffer.AddChunk(chunk);
        }

        buffer.Flatten(pending_);
        pendingPos_ = std::min(pending_.size(), curlBufferSize);

        if (pendingPos_ != 0)
        {
          memcpy(curlBuffer, pending_.c_str(), pendingPos_);
        }

        return pendingPos_;
      }
    }
    
  public:
    CurlRequestBody() :
      body_(NULL),
      pendingPos_(0)
    {
    }

    void SetBody(HttpClient::IRequestBody& body)
    {
      body_ = &body;
      pending_.clear();
      pendingPos_ = 0;
    }

    void Clear()
    {
      body_ = NULL;
      pending_.clear();
      pendingPos_ = 0;
    }

    bool IsValid() const
    {
      return body_ != NULL;
    }

    static size_t Callback(char *buffer, size_t size, size_t nitems, void *userdata)
    {
      try
      {
        assert(userdata != NULL);
        return reinterpret_cast<HttpClient::CurlRequestBody*>(userdata)->
          CallbackInternal(buffer, size * nitems);
      }
      catch (OrthancException& e)
      {
        LOG(ERROR) << "Exception while streaming HTTP body: " << e.What();
        return CURL_READFUNC_ABORT;
      }
      catch (...)
      {
        LOG(ERROR) << "Native exception while streaming HTTP body";
        return CURL_READFUNC_ABORT;
      }
    }
  };


  class HttpClient::CurlAnswer : public boost::noncopyable
  {
  private:
    HttpClient::IAnswer&  answer_;
    bool                  headersLowerCase_;

  public:
    CurlAnswer(HttpClient::IAnswer& answer,
               bool headersLowerCase) :
      answer_(answer),
      headersLowerCase_(headersLowerCase)
    {
    }

    static size_t HeaderCallback(void *buffer, size_t size, size_t nmemb, void *userdata)
    {
      try
      {
        assert(userdata != NULL);

        size_t length = size * nmemb;
        if (length == 0)
        {
          return 0;
        }
        else
        {
          std::string s(reinterpret_cast<const char*>(buffer), length);
          std::size_t colon = s.find(':');
          std::size_t eol = s.find("\r\n");
          if (colon != std::string::npos &&
              eol != std::string::npos)
          {
            CurlAnswer& that = *(static_cast<CurlAnswer*>(userdata));
            std::string tmp(s.substr(0, colon));

            if (that.headersLowerCase_)
            {
              Toolbox::ToLowerCase(tmp);
            }

            std::string key = Toolbox::StripSpaces(tmp);

            if (!key.empty())
            {
              std::string value = Toolbox::StripSpaces(s.substr(colon + 1, eol));

              that.answer_.AddHeader(key, value);
            }
          }

          return length;
        }
      }
      catch (OrthancException& e)
      {
        LOG(ERROR) << "Exception while streaming HTTP body: " << e.What();
        return CURL_READFUNC_ABORT;
      }
      catch (...)
      {
        LOG(ERROR) << "Native exception while streaming HTTP body";
        return CURL_READFUNC_ABORT;
      }
    }

    static size_t BodyCallback(void *buffer, size_t size, size_t nmemb, void *userdata)
    {
      try
      {
        assert(userdata != NULL);

        size_t length = size * nmemb;
        if (length == 0)
        {
          return 0;
        }
        else
        {
          CurlAnswer& that = *(static_cast<CurlAnswer*>(userdata));
          that.answer_.AddChunk(buffer, length);
          return length;
        }
      }
      catch (OrthancException& e)
      {
        LOG(ERROR) << "Exception while streaming HTTP body: " << e.What();
        return CURL_READFUNC_ABORT;
      }
      catch (...)
      {
        LOG(ERROR) << "Native exception while streaming HTTP body";
        return CURL_READFUNC_ABORT;
      }
    }
  };


  class HttpClient::DefaultAnswer : public HttpClient::IAnswer
  {
  private:
    ChunkedBuffer   answer_;
    HttpHeaders*    headers_;

  public:
    DefaultAnswer() : headers_(NULL)
    {
    }

    void SetHeaders(HttpHeaders& headers)
    {
      headers_ = &headers;
      headers_->clear();
    }

    void FlattenBody(std::string& target)
    {
      answer_.Flatten(target);
    }

    virtual void AddHeader(const std::string& key,
                           const std::string& value) ORTHANC_OVERRIDE
    {
      if (headers_ != NULL)
      {
        (*headers_) [key] = value;
      }
    }
      
    virtual void AddChunk(const void* data,
                          size_t size) ORTHANC_OVERRIDE
    {
      answer_.AddChunk(data, size);
    }
  };


  class HttpClient::GlobalParameters
  {
  private:
    boost::mutex    mutex_;
    bool            httpsVerifyPeers_;
    std::string     httpsCACertificates_;
    std::string     proxy_;
    long            timeout_;
    bool            verbose_;

    GlobalParameters() : 
      httpsVerifyPeers_(true),
      timeout_(0),
      verbose_(false)
    {
    }

  public:
    // Singleton pattern
    static GlobalParameters& GetInstance()
    {
      static GlobalParameters parameters;
      return parameters;
    }

    void ConfigureSsl(bool httpsVerifyPeers,
                      const std::string& httpsCACertificates)
    {
      boost::mutex::scoped_lock lock(mutex_);
      httpsVerifyPeers_ = httpsVerifyPeers;
      httpsCACertificates_ = httpsCACertificates;
    }

    void GetSslConfiguration(bool& httpsVerifyPeers,
                             std::string& httpsCACertificates)
    {
      boost::mutex::scoped_lock lock(mutex_);
      httpsVerifyPeers = httpsVerifyPeers_;
      httpsCACertificates = httpsCACertificates_;
    }

    void SetDefaultProxy(const std::string& proxy)
    {
      CLOG(INFO, HTTP) << "Setting the default proxy for HTTP client connections: " << proxy;

      {
        boost::mutex::scoped_lock lock(mutex_);
        proxy_ = proxy;
      }
    }

    void GetDefaultProxy(std::string& target)
    {
      boost::mutex::scoped_lock lock(mutex_);
      target = proxy_;
    }

    void SetDefaultTimeout(long seconds)
    {
      CLOG(INFO, HTTP) << "Setting the default timeout for HTTP client connections: " << seconds << " seconds";

      {
        boost::mutex::scoped_lock lock(mutex_);
        timeout_ = seconds;
      }
    }

    long GetDefaultTimeout()
    {
      boost::mutex::scoped_lock lock(mutex_);
      return timeout_;
    }

#if ORTHANC_ENABLE_PKCS11 == 1
    bool IsPkcs11Initialized()
    {
      boost::mutex::scoped_lock lock(mutex_);
      return Pkcs11::IsInitialized();
    }

    void InitializePkcs11(const std::string& module,
                          const std::string& pin,
                          bool verbose)
    {
      boost::mutex::scoped_lock lock(mutex_);
      Pkcs11::Initialize(module, pin, verbose);
    }
#endif

    bool IsDefaultVerbose() const
    {
      return verbose_;
    }

    void SetDefaultVerbose(bool verbose) 
    {
      verbose_ = verbose;
    }
  };


  struct HttpClient::PImpl
  {
    CURL* curl_;
    CurlHeaders defaultPostHeaders_;
    CurlHeaders defaultChunkedHeaders_;
    CurlHeaders userHeaders_;
    CurlRequestBody requestBody_;
  };


  void HttpClient::ThrowException(HttpStatus status)
  {
    switch (status)
    {
      case HttpStatus_400_BadRequest:
        throw OrthancException(ErrorCode_BadRequest);

      case HttpStatus_401_Unauthorized:
      case HttpStatus_403_Forbidden:
        throw OrthancException(ErrorCode_Unauthorized);

      case HttpStatus_404_NotFound:
        throw OrthancException(ErrorCode_UnknownResource);

      default:
        throw OrthancException(ErrorCode_NetworkProtocol);
    }
  }


  /*static int CurlDebugCallback(CURL *handle,
    curl_infotype type,
    char *data,
    size_t size,
    void *userptr)
    {
    switch (type)
    {
    case CURLINFO_TEXT:
    case CURLINFO_HEADER_IN:
    case CURLINFO_HEADER_OUT:
    case CURLINFO_SSL_DATA_IN:
    case CURLINFO_SSL_DATA_OUT:
    case CURLINFO_END:
    case CURLINFO_DATA_IN:
    case CURLINFO_DATA_OUT:
    {
    std::string s(data, size);
    CLOG(INFO, INFO) << "libcurl: " << s;
    break;
    }

    default:
    break;
    }

    return 0;
    }*/


  void HttpClient::Setup()
  {
    pimpl_->defaultPostHeaders_.AddHeader("Expect", "");
    pimpl_->defaultChunkedHeaders_.AddHeader("Expect", "");
    pimpl_->defaultChunkedHeaders_.AddHeader("Transfer-Encoding", "chunked");

    pimpl_->curl_ = curl_easy_init();

    CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_HEADERFUNCTION, &CurlAnswer::HeaderCallback));
    CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_WRITEFUNCTION, &CurlAnswer::BodyCallback));
    CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_HEADER, 0));
    CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_FOLLOWLOCATION, 1));

    // This fixes the "longjmp causes uninitialized stack frame" crash
    // that happens on modern Linux versions.
    // http://stackoverflow.com/questions/9191668/error-longjmp-causes-uninitialized-stack-frame
    CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_NOSIGNAL, 1));

    url_ = "";
    method_ = HttpMethod_Get;
    lastStatus_ = HttpStatus_None;
    SetVerbose(GlobalParameters::GetInstance().IsDefaultVerbose());
    timeout_ = GlobalParameters::GetInstance().GetDefaultTimeout();
    GlobalParameters::GetInstance().GetDefaultProxy(proxy_);
    GlobalParameters::GetInstance().GetSslConfiguration(verifyPeers_, caCertificates_);    

    hasExternalBody_ = false;
    externalBodyData_ = NULL;
    externalBodySize_ = 0;
  }


  HttpClient::HttpClient() : 
    pimpl_(new PImpl),
    verifyPeers_(true),
    pkcs11Enabled_(false),
    headersToLowerCase_(true),
    redirectionFollowed_(true)
  {
    Setup();
  }


  HttpClient::HttpClient(const WebServiceParameters& service,
                         const std::string& uri) : 
    pimpl_(new PImpl),
    verifyPeers_(true),
    headersToLowerCase_(true),
    redirectionFollowed_(true)
  {
    Setup();

    if (service.GetUsername().size() != 0 && 
        service.GetPassword().size() != 0)
    {
      SetCredentials(service.GetUsername().c_str(), 
                     service.GetPassword().c_str());
    }

    if (!service.GetCertificateFile().empty())
    {
      SetClientCertificate(service.GetCertificateFile(),
                           service.GetCertificateKeyFile(),
                           service.GetCertificateKeyPassword());
    }

    SetPkcs11Enabled(service.IsPkcs11Enabled());

    SetUrl(service.GetUrl() + uri);

    for (WebServiceParameters::Dictionary::const_iterator 
           it = service.GetHttpHeaders().begin();
         it != service.GetHttpHeaders().end(); ++it)
    {
      AddHeader(it->first, it->second);
    }

    if (service.HasTimeout())
    {
      SetTimeout(service.GetTimeout());
    }
  }


  HttpClient::~HttpClient()
  {
    curl_easy_cleanup(pimpl_->curl_);
  }

  void HttpClient::SetUrl(const char *url)
  {
    url_ = std::string(url);
  }

  void HttpClient::SetUrl(const std::string &url)
  {
    url_ = url;
  }

  const std::string &HttpClient::GetUrl() const
  {
    return url_;
  }

  void HttpClient::SetMethod(HttpMethod method)
  {
    method_ = method;
  }

  HttpMethod HttpClient::GetMethod() const
  {
    return method_;
  }

  void HttpClient::SetTimeout(long seconds)
  {
    timeout_ = seconds;
  }

  long HttpClient::GetTimeout() const
  {
    return timeout_;
  }


  void HttpClient::AssignBody(const std::string& data)
  {
    body_ = data;
    pimpl_->requestBody_.Clear();
    hasExternalBody_ = false;
  }


  void HttpClient::AssignBody(const void* data,
                              size_t size)
  {
    if (size != 0 &&
        data == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
    else
    {
      body_.assign(reinterpret_cast<const char*>(data), size);
      pimpl_->requestBody_.Clear();
      hasExternalBody_ = false;
    }
  }


  void HttpClient::SetBody(IRequestBody& body)
  {
    body_.clear();
    pimpl_->requestBody_.SetBody(body);
    hasExternalBody_ = false;
  }

  
  void HttpClient::SetExternalBody(const void* data,
                                   size_t size)
  {
    if (size != 0 &&
        data == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
    else
    {
      body_.clear();
      pimpl_->requestBody_.Clear();
      hasExternalBody_ = true;
      externalBodyData_ = data;
      externalBodySize_ = size;
    }
  }
  

  void HttpClient::SetExternalBody(const std::string& data)
  {
    SetExternalBody(data.empty() ? NULL : data.c_str(), data.size());
  }


  void HttpClient::ClearBody()
  {
    body_.clear();
    pimpl_->requestBody_.Clear();
    hasExternalBody_ = false;
  }


  void HttpClient::SetVerbose(bool isVerbose)
  {
    isVerbose_ = isVerbose;

    if (isVerbose_)
    {
      CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_VERBOSE, 1));
      //CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_DEBUGFUNCTION, &CurlDebugCallback));
    }
    else
    {
      CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_VERBOSE, 0));
    }
  }

  bool HttpClient::IsVerbose() const
  {
    return isVerbose_;
  }


  void HttpClient::AddHeader(const std::string& key,
                             const std::string& value)
  {
    if (key.empty())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
    else
    {
      pimpl_->userHeaders_.AddHeader(key, value);
    }
  }


  void HttpClient::ClearHeaders()
  {
    pimpl_->userHeaders_.Clear();
  }


  bool HttpClient::ApplyInternal(CurlAnswer& answer)
  {
    CLOG(INFO, HTTP) << "New HTTP request to: " << url_ << " (timeout: "
                     << boost::lexical_cast<std::string>(timeout_ <= 0 ? DEFAULT_HTTP_TIMEOUT : timeout_) << "s)";
    
    CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_URL, url_.c_str()));
    CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_HEADERDATA, &answer));

#if ORTHANC_ENABLE_SSL == 1
    // Setup HTTPS-related options

    if (verifyPeers_)
    {
      CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_CAINFO, caCertificates_.c_str()));
      CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_SSL_VERIFYHOST, 2));  // libcurl default is strict verifyhost
      CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_SSL_VERIFYPEER, 1)); 
    }
    else
    {
      CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_SSL_VERIFYHOST, 0)); 
      CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_SSL_VERIFYPEER, 0)); 
    }
#endif

    // Setup the HTTPS client certificate
    if (!clientCertificateFile_.empty() &&
        pkcs11Enabled_)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange,
                             "Cannot enable both client certificates and PKCS#11 authentication");
    }

    if (pkcs11Enabled_)
    {
#if ORTHANC_ENABLE_PKCS11 == 1
      if (GlobalParameters::GetInstance().IsPkcs11Initialized())
      {
        CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_SSLENGINE, Pkcs11::GetEngineIdentifier()));
        CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_SSLKEYTYPE, "ENG"));
        CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_SSLCERTTYPE, "ENG"));
      }
      else
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls,
                               "Cannot use PKCS#11 for a HTTPS request, "
                               "because it has not been initialized");
      }
#else
      throw OrthancException(ErrorCode_InternalError,
                             "This version of Orthanc is compiled without support for PKCS#11");
#endif
    }
    else if (!clientCertificateFile_.empty())
    {
#if ORTHANC_ENABLE_SSL == 1
      CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_SSLCERTTYPE, "PEM"));
      CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_SSLCERT, clientCertificateFile_.c_str()));
      CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_KEYPASSWD, clientCertificateKeyPassword_.c_str()));

      // NB: If no "clientKeyFile_" is provided, the key must be
      // prepended to the certificate file
      if (!clientCertificateKeyFile_.empty())
      {
        CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_SSLKEYTYPE, "PEM"));
        CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_SSLKEY, clientCertificateKeyFile_.c_str()));
      }
#else
      throw OrthancException(ErrorCode_InternalError,
                             "This version of Orthanc is compiled without OpenSSL support, "
                             "cannot use HTTPS client authentication");
#endif
    }

    // Reset the parameters from previous calls to Apply()
    pimpl_->userHeaders_.Assign(pimpl_->curl_);
    CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_HTTPGET, 0L));
    CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_POST, 0L));
    CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_NOBODY, 0L));
    CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_CUSTOMREQUEST, NULL));
    CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_POSTFIELDS, NULL));
    CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_POSTFIELDSIZE, 0L));
    CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_PROXY, NULL));

    if (redirectionFollowed_)
    {
      CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_FOLLOWLOCATION, 1L));
    }
    else
    {
      CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_FOLLOWLOCATION, 0L));
    }

    // Set timeouts
    if (timeout_ <= 0)
    {
      CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_TIMEOUT, DEFAULT_HTTP_TIMEOUT));
      CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_CONNECTTIMEOUT, DEFAULT_HTTP_TIMEOUT));
    }
    else
    {
      CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_TIMEOUT, timeout_));
      CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_CONNECTTIMEOUT, timeout_));
    }

    if (credentials_.size() != 0)
    {
      CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_USERPWD, credentials_.c_str()));
    }

    if (proxy_.size() != 0)
    {
      CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_PROXY, proxy_.c_str()));
    }

    switch (method_)
    {
      case HttpMethod_Get:
        CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_HTTPGET, 1L));
        break;

      case HttpMethod_Post:
        CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_POST, 1L));

        break;

      case HttpMethod_Delete:
        CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_NOBODY, 1L));
        CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_CUSTOMREQUEST, "DELETE"));
        break;

      case HttpMethod_Put:
        // http://stackoverflow.com/a/7570281/881731: Don't use
        // CURLOPT_PUT if there is a body

        // CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_PUT, 1L));

        curl_easy_setopt(pimpl_->curl_, CURLOPT_CUSTOMREQUEST, "PUT"); /* !!! */
        break;

      default:
        throw OrthancException(ErrorCode_InternalError);
    }

    if (method_ == HttpMethod_Post ||
        method_ == HttpMethod_Put)
    {
      if (!pimpl_->userHeaders_.IsEmpty() &&
          !pimpl_->userHeaders_.HasExpect())
      {
        CLOG(INFO, HTTP) << "For performance, the HTTP header \"Expect\" should be set to empty string in POST/PUT requests";
      }

      if (pimpl_->requestBody_.IsValid())
      {
        CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_READFUNCTION, CurlRequestBody::Callback));
        CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_READDATA, &pimpl_->requestBody_));
        CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_POST, 1L));
        CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_POSTFIELDSIZE, -1L));
    
        if (pimpl_->userHeaders_.IsEmpty())
        {
          pimpl_->defaultChunkedHeaders_.Assign(pimpl_->curl_);
        }
        else if (!pimpl_->userHeaders_.IsChunkedTransfer())
        {
          LOG(WARNING) << "The HTTP header \"Transfer-Encoding\" must be set to \"chunked\" "
                       << "if streaming a chunked body in POST/PUT requests";
        }
      }
      else
      {
        // Disable possible previous stream transfers
        CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_READFUNCTION, NULL));
        CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_UPLOAD, 0));

        if (pimpl_->userHeaders_.IsChunkedTransfer())
        {
          LOG(WARNING) << "The HTTP header \"Transfer-Encoding\" must only be set "
                       << "if streaming a chunked body in POST/PUT requests";
        }

        if (pimpl_->userHeaders_.IsEmpty())
        {
          pimpl_->defaultPostHeaders_.Assign(pimpl_->curl_);
        }

        if (hasExternalBody_)
        {
          CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_POSTFIELDS, externalBodyData_));
          CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_POSTFIELDSIZE, externalBodySize_));
        }
        else if (body_.size() > 0)
        {
          CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_POSTFIELDS, body_.c_str()));
          CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_POSTFIELDSIZE, body_.size()));
        }
        else
        {
          CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_POSTFIELDS, NULL));
          CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_POSTFIELDSIZE, 0));
        }
      }
    }


    // Do the actual request
    CURLcode code;
    long status = 0;

    CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_WRITEDATA, &answer));

    const boost::posix_time::ptime start = boost::posix_time::microsec_clock::universal_time();
    
    if (boost::starts_with(url_, "https://"))
    {
      code = OrthancHttpClientPerformSSL(pimpl_->curl_, &status);
    }
    else
    {
      code = GetHttpStatus(curl_easy_perform(pimpl_->curl_), pimpl_->curl_, &status);
    }

    const boost::posix_time::ptime end = boost::posix_time::microsec_clock::universal_time();
    
    CLOG(INFO, HTTP) << "HTTP status code " << status << " in "
                     << ((end - start).total_milliseconds()) << " ms after "
                     << EnumerationToString(method_) << " request on: " << url_;

    if (isVerbose_)
    {
      CLOG(INFO, HTTP) << "cURL status code: " << code;
    }

    CheckCode(code);

    if (status == 0)
    {
      // This corresponds to a call to an inexistent host
      lastStatus_ = HttpStatus_500_InternalServerError;
    }
    else
    {
      lastStatus_ = static_cast<HttpStatus>(status);
    }

    if (status >= 200 && status < 300)
    {
      return true;   // Success
    }
    else
    {
      LOG(ERROR) << "Error in HTTP request, received HTTP status " << status 
                 << " (" << EnumerationToString(lastStatus_) << ") after "
                 << EnumerationToString(method_) << " request on: " << url_;
      return false;
    }
  }


  bool HttpClient::ApplyInternal(std::string& answerBody,
                                 HttpHeaders* answerHeaders)
  {
    answerBody.clear();

    DefaultAnswer answer;

    if (answerHeaders != NULL)
    {
      answer.SetHeaders(*answerHeaders);
    }

    CurlAnswer wrapper(answer, headersToLowerCase_);

    if (ApplyInternal(wrapper))
    {
      answer.FlattenBody(answerBody);
      return true;
    }
    else
    {
      return false;
    }
  }


  bool HttpClient::ApplyInternal(Json::Value& answerBody,
                                 HttpClient::HttpHeaders* answerHeaders)
  {
    std::string s;
    if (ApplyInternal(s, answerHeaders))
    {
      return Toolbox::ReadJson(answerBody, s);
    }
    else
    {
      return false;
    }
  }


  void HttpClient::SetCredentials(const char* username,
                                  const char* password)
  {
    credentials_ = std::string(username) + ":" + std::string(password);
  }

  void HttpClient::SetProxy(const std::string &proxy)
  {
    proxy_ = proxy;
  }

  void HttpClient::SetHttpsVerifyPeers(bool verify)
  {
    verifyPeers_ = verify;
  }

  bool HttpClient::IsHttpsVerifyPeers() const
  {
    return verifyPeers_;
  }

  void HttpClient::SetHttpsCACertificates(const std::string &certificates)
  {
    caCertificates_ = certificates;
  }

  const std::string &HttpClient::GetHttpsCACertificates() const
  {
    return caCertificates_;
  }


  void HttpClient::ConfigureSsl(bool httpsVerifyPeers,
                                const std::string& httpsVerifyCertificates)
  {
#if ORTHANC_ENABLE_SSL == 1
    if (httpsVerifyPeers)
    {
      if (httpsVerifyCertificates.empty())
      {
        LOG(WARNING) << "No certificates are provided to validate peers, "
                     << "set \"HttpsCACertificates\" if you need to do HTTPS requests";
      }
      else
      {
        LOG(WARNING) << "HTTPS will use the CA certificates from this file: " << httpsVerifyCertificates;
      }
    }
    else
    {
      LOG(WARNING) << "The verification of the peers in HTTPS requests is disabled";
    }
#endif

    GlobalParameters::GetInstance().ConfigureSsl(httpsVerifyPeers, httpsVerifyCertificates);
  }

  
  void HttpClient::GlobalInitialize()
  {
#if ORTHANC_ENABLE_SSL == 1
    CheckCode(curl_global_init(CURL_GLOBAL_ALL));
#else
    CheckCode(curl_global_init(CURL_GLOBAL_ALL & ~CURL_GLOBAL_SSL));
#endif
  }


  void HttpClient::GlobalFinalize()
  {
    curl_global_cleanup();

#if ORTHANC_ENABLE_PKCS11 == 1
    Pkcs11::Finalize();
#endif
  }
  

  void HttpClient::SetDefaultVerbose(bool verbose)
  {
    GlobalParameters::GetInstance().SetDefaultVerbose(verbose);
  }


  void HttpClient::SetDefaultProxy(const std::string& proxy)
  {
    GlobalParameters::GetInstance().SetDefaultProxy(proxy);
  }


  void HttpClient::SetDefaultTimeout(long timeout)
  {
    GlobalParameters::GetInstance().SetDefaultTimeout(timeout);
  }


  bool HttpClient::Apply(IAnswer& answer)
  {
    CurlAnswer wrapper(answer, headersToLowerCase_);
    return ApplyInternal(wrapper);
  }

  bool HttpClient::Apply(std::string &answerBody)
  {
    return ApplyInternal(answerBody, NULL);
  }

  bool HttpClient::Apply(Json::Value &answerBody)
  {
    return ApplyInternal(answerBody, NULL);
  }

  bool HttpClient::Apply(std::string &answerBody,
                         HttpClient::HttpHeaders &answerHeaders)
  {
    return ApplyInternal(answerBody, &answerHeaders);
  }

  bool HttpClient::Apply(Json::Value &answerBody,
                         HttpClient::HttpHeaders &answerHeaders)
  {
    return ApplyInternal(answerBody, &answerHeaders);
  }

  HttpStatus HttpClient::GetLastStatus() const
  {
    return lastStatus_;
  }


  void HttpClient::ApplyAndThrowException(IAnswer& answer)
  {
    CurlAnswer wrapper(answer, headersToLowerCase_);

    if (!ApplyInternal(wrapper))
    {
      ThrowException(GetLastStatus());
    }
  }


  void HttpClient::ApplyAndThrowException(std::string& answerBody)
  {
    if (!Apply(answerBody))
    {
      ThrowException(GetLastStatus());
    }
  }

  
  void HttpClient::ApplyAndThrowException(Json::Value& answerBody)
  {
    if (!Apply(answerBody))
    {
      ThrowException(GetLastStatus());
    }
  }


  void HttpClient::ApplyAndThrowException(std::string& answerBody,
                                          HttpHeaders& answerHeaders)
  {
    if (!Apply(answerBody, answerHeaders))
    {
      ThrowException(GetLastStatus());
    }
  }
  

  void HttpClient::ApplyAndThrowException(Json::Value& answerBody,
                                          HttpHeaders& answerHeaders)
  {
    if (!Apply(answerBody, answerHeaders))
    {
      ThrowException(GetLastStatus());
    }
  }


  void HttpClient::SetClientCertificate(const std::string& certificateFile,
                                        const std::string& certificateKeyFile,
                                        const std::string& certificateKeyPassword)
  {
    if (certificateFile.empty())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    if (!SystemToolbox::IsRegularFile(certificateFile))
    {
      throw OrthancException(ErrorCode_InexistentFile,
                             "Cannot open certificate file: " + certificateFile);
    }

    if (!certificateKeyFile.empty() && 
        !SystemToolbox::IsRegularFile(certificateKeyFile))
    {
      throw OrthancException(ErrorCode_InexistentFile,
                             "Cannot open key file: " + certificateKeyFile);
    }

    clientCertificateFile_ = certificateFile;
    clientCertificateKeyFile_ = certificateKeyFile;
    clientCertificateKeyPassword_ = certificateKeyPassword;
  }

  void HttpClient::SetPkcs11Enabled(bool enabled)
  {
    pkcs11Enabled_ = enabled;
  }

  bool HttpClient::IsPkcs11Enabled() const
  {
    return pkcs11Enabled_;
  }

  const std::string &HttpClient::GetClientCertificateFile() const
  {
    return clientCertificateFile_;
  }

  const std::string &HttpClient::GetClientCertificateKeyFile() const
  {
    return clientCertificateKeyFile_;
  }

  const std::string &HttpClient::GetClientCertificateKeyPassword() const
  {
    return clientCertificateKeyPassword_;
  }

  void HttpClient::SetConvertHeadersToLowerCase(bool lowerCase)
  {
    headersToLowerCase_ = lowerCase;
  }

  bool HttpClient::IsConvertHeadersToLowerCase() const
  {
    return headersToLowerCase_;
  }

  void HttpClient::SetRedirectionFollowed(bool follow)
  {
    redirectionFollowed_ = follow;
  }

  bool HttpClient::IsRedirectionFollowed() const
  {
    return redirectionFollowed_;
  }


  void HttpClient::InitializePkcs11(const std::string& module,
                                    const std::string& pin,
                                    bool verbose)
  {
#if ORTHANC_ENABLE_PKCS11 == 1
    CLOG(INFO, HTTP) << "Initializing PKCS#11 using " << module 
                     << (pin.empty() ? " (no PIN provided)" : " (PIN is provided)");
    GlobalParameters::GetInstance().InitializePkcs11(module, pin, verbose);    
#else
    throw OrthancException(ErrorCode_InternalError,
                           "This version of Orthanc is compiled without support for PKCS#11");
#endif
  }
}
