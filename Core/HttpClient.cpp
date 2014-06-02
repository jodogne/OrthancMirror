/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2014 Medical Physics Department, CHU of Liege,
 * Belgium
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

#include "../Core/Toolbox.h"
#include "../Core/OrthancException.h"

#include <string.h>
#include <curl/curl.h>


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
      throw OrthancException("libCURL error: " + std::string(curl_easy_strerror(code)));
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

#if ORTHANC_SSL_ENABLED == 1
    CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_SSL_VERIFYPEER, 0)); 
#endif

    // This fixes the "longjmp causes uninitialized stack frame" crash
    // that happens on modern Linux versions.
    // http://stackoverflow.com/questions/9191668/error-longjmp-causes-uninitialized-stack-frame
    CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_NOSIGNAL, 1));

    url_ = "";
    method_ = HttpMethod_Get;
    lastStatus_ = HttpStatus_200_Ok;
    isVerbose_ = false;
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
    CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_HTTPHEADER, NULL));

    if (credentials_.size() != 0)
    {
      CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_USERPWD, credentials_.c_str()));
    }

    switch (method_)
    {
    case HttpMethod_Get:
      CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_HTTPGET, 1L));
      break;

    case HttpMethod_Post:
      CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_POST, 1L));
      CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_HTTPHEADER, pimpl_->postHeaders_));

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

      break;

    case HttpMethod_Delete:
      CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_NOBODY, 1L));
      CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_CUSTOMREQUEST, "DELETE"));
      break;

    case HttpMethod_Put:
      CheckCode(curl_easy_setopt(pimpl_->curl_, CURLOPT_PUT, 1L));
      break;

    default:
      throw OrthancException(ErrorCode_InternalError);
    }

    // Do the actual request
    CheckCode(curl_easy_perform(pimpl_->curl_));

    long status;
    CheckCode(curl_easy_getinfo(pimpl_->curl_, CURLINFO_RESPONSE_CODE, &status));

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

  
  void HttpClient::GlobalInitialize()
  {
    CheckCode(curl_global_init(CURL_GLOBAL_DEFAULT));
  }
  
  void HttpClient::GlobalFinalize()
  {
    curl_global_cleanup();
  }
}
