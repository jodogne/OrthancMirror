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


#if ORTHANC_ENABLE_SSL == 1
// For OpenSSL initialization and finalization
#  include <openssl/conf.h>
#  include <openssl/engine.h>
#  include <openssl/err.h>
#  include <openssl/evp.h>
#  include <openssl/ssl.h>
#endif


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
      *status = 0;
      return code;
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
    LOG(ERROR) << "Orthanc was compiled without SSL support, cannot make HTTPS request";
    throw Orthanc::OrthancException(Orthanc::ErrorCode_InternalError);
#endif
  }
}



namespace Orthanc
{
  class HttpClient::GlobalParameters
  {
  private:
    boost::mutex    mutex_;
    bool            httpsVerifyPeers_;
    std::string     httpsCACertificates_;
    std::string     proxy_;
    long            timeout_;

    GlobalParameters() : 
      httpsVerifyPeers_(true),
      timeout_(0)
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
      LOG(INFO) << "Setting the default proxy for HTTP client connections: " << proxy;

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
      LOG(INFO) << "Setting the default timeout for HTTP client connections: " << seconds << " seconds";

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
  };


  struct HttpClient::PImpl
  {
    CURL* curl_;
    struct curl_slist *defaultPostHeaders_;
    struct curl_slist *userHeaders_;
  };


  static void ThrowException(HttpStatus status)
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


  static CURLcode CheckCode(CURLcode code)
  {
    if (code == CURLE_NOT_BUILT_IN)
    {
      LOG(ERROR) << "Your libcurl does not contain a required feature, "
                 << "please recompile Orthanc with -DUSE_SYSTEM_CURL=OFF";
      throw OrthancException(ErrorCode_InternalError);
    }

    if (code != CURLE_OK)
    {
      LOG(ERROR) << "libCURL error: " + std::string(curl_easy_strerror(code));
      throw OrthancException(ErrorCode_NetworkProtocol);
    }

    return code;
  }


  static size_t CurlBodyCallback(void *buffer, size_t size, size_t nmemb, void *payload)
  {
    ChunkedBuffer& target = *(static_cast<ChunkedBuffer*>(payload));

    size_t length = size * nmemb;
    if (length == 0)
    {
      return 0;
    }
    else
    {
      target.AddChunk(buffer, length);
      return length;
    }
  }


  struct CurlHeaderParameters
  {
    bool lowerCase_;
    HttpClient::HttpHeaders* headers_;
  };


  static size_t CurlHeaderCallback(void *buffer, size_t size, size_t nmemb, void *payload)
  {
    CurlHeaderParameters& parameters = *(static_cast<CurlHeaderParameters*>(payload));
    assert(parameters.headers_ != NULL);

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
        std::string tmp(s.substr(0, colon));

        if (parameters.lowerCase_)
        {
          Toolbox::ToLowerCase(tmp);
        }

        std::string key = Toolbox::StripSpaces(tmp);

        if (!key.empty())
        {
          std::string value = Toolbox::StripSpaces(s.substr(colon + 1, eol));
          (*parameters.headers_) [key] = value;
        }
      }

      return length;
    }
  }


  void HttpClient::Setup()
  {
    pimpl_->userHeaders_ = NULL;
    pimpl_->defaultPostHeaders_ = NULL;
    if ((pimpl_->defaultPostHeaders_ = curl_slist_append(pimpl_->defaultPostHeaders_, "Expect:")) == NULL)
    {
      throw OrthancException(ErrorCode_NotEnoughMemory);
    }

    pimpl_->curl_ = curl_easy_init();
    if (!pimpl_->curl_)
    {
      curl_slist_free_all(pimpl_->defaultPostHeaders_);
      throw OrthancException(ErrorCode_NotEnoughMemory);
    }

    CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_WRITEFUNCTION, &CurlBodyCallback));
    CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_HEADER, 0));
    CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_FOLLOWLOCATION, 1));

    // This fixes the "longjmp causes uninitialized stack frame" crash
    // that happens on modern Linux versions.
    // http://stackoverflow.com/questions/9191668/error-longjmp-causes-uninitialized-stack-frame
    CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_NOSIGNAL, 1));

    url_ = "";
    method_ = HttpMethod_Get;
    lastStatus_ = HttpStatus_200_Ok;
    SetVerbose(false);
    timeout_ = GlobalParameters::GetInstance().GetDefaultTimeout();
    GlobalParameters::GetInstance().GetDefaultProxy(proxy_);
    GlobalParameters::GetInstance().GetSslConfiguration(verifyPeers_, caCertificates_);    
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
  }


  HttpClient::~HttpClient()
  {
    curl_easy_cleanup(pimpl_->curl_);
    curl_slist_free_all(pimpl_->defaultPostHeaders_);
    ClearHeaders();
  }


  void HttpClient::SetVerbose(bool isVerbose)
  {
    isVerbose_ = isVerbose;

    if (isVerbose_)
    {
      CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_VERBOSE, 1));
    }
    else
    {
      CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_VERBOSE, 0));
    }
  }


  void HttpClient::AddHeader(const std::string& key,
                             const std::string& value)
  {
    if (key.empty())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    std::string s = key + ": " + value;

    if ((pimpl_->userHeaders_ = curl_slist_append(pimpl_->userHeaders_, s.c_str())) == NULL)
    {
      throw OrthancException(ErrorCode_NotEnoughMemory);
    }
  }


  void HttpClient::ClearHeaders()
  {
    if (pimpl_->userHeaders_ != NULL)
    {
      curl_slist_free_all(pimpl_->userHeaders_);
      pimpl_->userHeaders_ = NULL;
    }
  }


  bool HttpClient::ApplyInternal(std::string& answerBody,
                                 HttpHeaders* answerHeaders)
  {
    answerBody.clear();
    CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_URL, url_.c_str()));

    CurlHeaderParameters headerParameters;

    if (answerHeaders == NULL)
    {
      CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_HEADERFUNCTION, NULL));
      CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_HEADERDATA, NULL));
    }
    else
    {
      headerParameters.lowerCase_ = headersToLowerCase_;
      headerParameters.headers_ = answerHeaders;
      CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_HEADERFUNCTION, &CurlHeaderCallback));
      CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_HEADERDATA, &headerParameters));
    }

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
      LOG(ERROR) << "Cannot enable both client certificates and PKCS#11 authentication";
      throw OrthancException(ErrorCode_ParameterOutOfRange);
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
        LOG(ERROR) << "Cannot use PKCS#11 for a HTTPS request, because it has not been initialized";
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }
#else
      LOG(ERROR) << "This version of Orthanc is compiled without support for PKCS#11";
      throw OrthancException(ErrorCode_InternalError);
#endif
    }
    else if (!clientCertificateFile_.empty())
    {
#if ORTHANC_ENABLE_SSL == 1
      CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_SSLCERTTYPE, "PEM"));
      CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_SSLCERT, clientCertificateFile_.c_str()));

      if (!clientCertificateKeyPassword_.empty())
      {
        CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_KEYPASSWD, clientCertificateKeyPassword_.c_str()));
      }

      // NB: If no "clientKeyFile_" is provided, the key must be
      // prepended to the certificate file
      if (!clientCertificateKeyFile_.empty())
      {
        CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_SSLKEYTYPE, "PEM"));
        CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_SSLKEY, clientCertificateKeyFile_.c_str()));
      }
#else
      LOG(ERROR) << "This version of Orthanc is compiled without OpenSSL support, cannot use HTTPS client authentication";
      throw OrthancException(ErrorCode_InternalError);
#endif
    }

    // Reset the parameters from previous calls to Apply()
    CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_HTTPHEADER, pimpl_->userHeaders_));
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
      CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_TIMEOUT, 10));  /* default: 10 seconds */
      CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_CONNECTTIMEOUT, 10));  /* default: 10 seconds */
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

      if (pimpl_->userHeaders_ == NULL)
      {
        CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_HTTPHEADER, pimpl_->defaultPostHeaders_));
      }

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

      if (pimpl_->userHeaders_ == NULL)
      {
        CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_HTTPHEADER, pimpl_->defaultPostHeaders_));
      }

      break;

    default:
      throw OrthancException(ErrorCode_InternalError);
    }


    if (method_ == HttpMethod_Post ||
        method_ == HttpMethod_Put)
    {
      if (body_.size() > 0)
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


    // Do the actual request
    CURLcode code;
    long status = 0;

    ChunkedBuffer buffer;
    CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_WRITEDATA, &buffer));

    if (boost::starts_with(url_, "https://"))
    {
      code = OrthancHttpClientPerformSSL(pimpl_->curl_, &status);
    }
    else
    {
      code = GetHttpStatus(curl_easy_perform(pimpl_->curl_), pimpl_->curl_, &status);
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

    bool success = (status >= 200 && status < 300);

    if (success)
    {
      buffer.Flatten(answerBody);
    }
    else
    {
      answerBody.clear();
      LOG(INFO) << "Error in HTTP request, received HTTP status " << status 
                << " (" << EnumerationToString(lastStatus_) << ")";
    }

    return success;
  }


  bool HttpClient::ApplyInternal(Json::Value& answerBody,
                                 HttpClient::HttpHeaders* answerHeaders)
  {
    std::string s;
    if (ApplyInternal(s, answerHeaders))
    {
      Json::Reader reader;
      return reader.parse(s, answerBody);
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
  

  void HttpClient::SetDefaultProxy(const std::string& proxy)
  {
    GlobalParameters::GetInstance().SetDefaultProxy(proxy);
  }


  void HttpClient::SetDefaultTimeout(long timeout)
  {
    GlobalParameters::GetInstance().SetDefaultTimeout(timeout);
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
      LOG(ERROR) << "Cannot open certificate file: " << certificateFile;
      throw OrthancException(ErrorCode_InexistentFile);
    }

    if (!certificateKeyFile.empty() && 
        !SystemToolbox::IsRegularFile(certificateKeyFile))
    {
      LOG(ERROR) << "Cannot open key file: " << certificateKeyFile;
      throw OrthancException(ErrorCode_InexistentFile);
    }

    clientCertificateFile_ = certificateFile;
    clientCertificateKeyFile_ = certificateKeyFile;
    clientCertificateKeyPassword_ = certificateKeyPassword;
  }


  void HttpClient::InitializePkcs11(const std::string& module,
                                    const std::string& pin,
                                    bool verbose)
  {
#if ORTHANC_ENABLE_PKCS11 == 1
    LOG(INFO) << "Initializing PKCS#11 using " << module 
              << (pin.empty() ? " (no PIN provided)" : " (PIN is provided)");
    GlobalParameters::GetInstance().InitializePkcs11(module, pin, verbose);    
#else
    LOG(ERROR) << "This version of Orthanc is compiled without support for PKCS#11";
    throw OrthancException(ErrorCode_InternalError);
#endif
  }


  void HttpClient::InitializeOpenSsl()
  {
#if ORTHANC_ENABLE_SSL == 1
    // https://wiki.openssl.org/index.php/Library_Initialization
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();
#endif
  }


  void HttpClient::FinalizeOpenSsl()
  {
#if ORTHANC_ENABLE_SSL == 1
    // Finalize OpenSSL
    // https://wiki.openssl.org/index.php/Library_Initialization#Cleanup
#ifdef FIPS_mode_set
    FIPS_mode_set(0);
#endif
    ENGINE_cleanup();
    CONF_modules_unload(1);
    EVP_cleanup();
    CRYPTO_cleanup_all_ex_data();
    ERR_remove_state(0);
    ERR_free_strings();
#endif
  }
}
