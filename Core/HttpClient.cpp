/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2015 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
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

#include <string.h>
#include <curl/curl.h>
#include <boost/algorithm/string/predicate.hpp>


static std::string globalCACertificates_;
static bool globalVerifyPeers_ = true;

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
    return GetHttpStatus(curl_easy_perform(curl), curl, status);
  }
}



namespace Orthanc
{
  struct HttpClient::PImpl
  {
    CURL* curl_;
    struct curl_slist *postHeaders_;
  };


  static CURLcode CheckCode(CURLcode code)
  {
    if (code != CURLE_OK)
    {
      LOG(ERROR) << "libCURL error: " + std::string(curl_easy_strerror(code));
      throw OrthancException(ErrorCode_NetworkProtocol);
    }

    return code;
  }


  static size_t CurlCallback(void *buffer, size_t size, size_t nmemb, void *payload)
  {
    std::string& target = *(static_cast<std::string*>(payload));

    size_t length = size * nmemb;
    if (length == 0)
      return 0;

    size_t pos = target.size();

    target.resize(pos + length);
    memcpy(&target.at(pos), buffer, length);

    return length;
  }


  void HttpClient::Setup()
  {
    pimpl_->postHeaders_ = NULL;
    if ((pimpl_->postHeaders_ = curl_slist_append(pimpl_->postHeaders_, "Expect:")) == NULL)
    {
      throw OrthancException(ErrorCode_NotEnoughMemory);
    }

    pimpl_->curl_ = curl_easy_init();
    if (!pimpl_->curl_)
    {
      curl_slist_free_all(pimpl_->postHeaders_);
      throw OrthancException(ErrorCode_NotEnoughMemory);
    }

    CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_WRITEFUNCTION, &CurlCallback));
    CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_HEADER, 0));
    CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_FOLLOWLOCATION, 1));

    // This fixes the "longjmp causes uninitialized stack frame" crash
    // that happens on modern Linux versions.
    // http://stackoverflow.com/questions/9191668/error-longjmp-causes-uninitialized-stack-frame
    CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_NOSIGNAL, 1));

    url_ = "";
    method_ = HttpMethod_Get;
    lastStatus_ = HttpStatus_200_Ok;
    isVerbose_ = false;
    timeout_ = 0;
    verifyPeers_ = globalVerifyPeers_;
  }


  HttpClient::HttpClient() : pimpl_(new PImpl)
  {
    Setup();
  }


  HttpClient::HttpClient(const HttpClient& other) : pimpl_(new PImpl)
  {
    Setup();

    if (other.IsVerbose())
    {
      SetVerbose(true);
    }

    if (other.credentials_.size() != 0)
    {
      credentials_ = other.credentials_;
    }
  }


  HttpClient::~HttpClient()
  {
    curl_easy_cleanup(pimpl_->curl_);
    curl_slist_free_all(pimpl_->postHeaders_);
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


  bool HttpClient::Apply(std::string& answer)
  {
    answer.clear();
    CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_URL, url_.c_str()));
    CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_WRITEDATA, &answer));

    // Setup HTTPS-related options
#if ORTHANC_SSL_ENABLED == 1
    if (IsHttpsVerifyPeers())
    {
      CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_CAINFO, GetHttpsCACertificates().c_str()));
      CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_SSL_VERIFYPEER, 1)); 
    }
    else
    {
      CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_SSL_VERIFYPEER, 0)); 
    }
#endif

    // Reset the parameters from previous calls to Apply()
    CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_HTTPHEADER, NULL));
    CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_HTTPGET, 0L));
    CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_POST, 0L));
    CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_NOBODY, 0L));
    CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_CUSTOMREQUEST, NULL));
    CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_POSTFIELDS, NULL));
    CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_POSTFIELDSIZE, 0));
    CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_PROXY, NULL));

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
      CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_HTTPHEADER, pimpl_->postHeaders_));
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
      CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_HTTPHEADER, pimpl_->postHeaders_));      
      break;

    default:
      throw OrthancException(ErrorCode_InternalError);
    }


    if (method_ == HttpMethod_Post ||
        method_ == HttpMethod_Put)
    {
      if (postData_.size() > 0)
      {
        CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_POSTFIELDS, postData_.c_str()));
        CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_POSTFIELDSIZE, postData_.size()));
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

    return (status >= 200 && status < 300);
  }


  bool HttpClient::Apply(Json::Value& answer)
  {
    std::string s;
    if (Apply(s))
    {
      Json::Reader reader;
      return reader.parse(s, answer);
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

  
  const std::string& HttpClient::GetHttpsCACertificates() const
  {
    if (caCertificates_.empty())
    {
      return globalCACertificates_;
    }
    else
    {
      return caCertificates_;
    }
  }


  void HttpClient::GlobalInitialize(bool httpsVerifyPeers,
                                    const std::string& httpsVerifyCertificates)
  {
    globalVerifyPeers_ = httpsVerifyPeers;
    globalCACertificates_ = httpsVerifyCertificates;

#if ORTHANC_SSL_ENABLED == 1
    if (httpsVerifyPeers)
    {
      if (globalCACertificates_.empty())
      {
        LOG(WARNING) << "No certificates are provided to validate peers, "
                     << "set \"HttpsCACertificates\" if you need to do HTTPS requests";
      }
      else
      {
        LOG(WARNING) << "HTTPS will use the CA certificates from this file: " << globalCACertificates_;
      }
    }
    else
    {
      LOG(WARNING) << "The verification of the peers in HTTPS requests is disabled!";
    }
#endif

    CheckCode(curl_global_init(CURL_GLOBAL_DEFAULT));
  }

  
  void HttpClient::GlobalFinalize()
  {
    curl_global_cleanup();
  }
}
