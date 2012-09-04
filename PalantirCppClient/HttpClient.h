/**
 * Palantir - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012 Medical Physics Department, CHU of Liege,
 * Belgium
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 **/


#pragma once

#include "HttpEnumerations.h"
#include "HttpException.h"

#include <string>
#include <boost/shared_ptr.hpp>
#include <json/json.h>

namespace Palantir
{
  class HttpClient
  {
  private:
    struct PImpl;
    boost::shared_ptr<PImpl> pimpl_;

    std::string url_;
    Palantir_HttpMethod method_;
    Palantir_HttpStatus lastStatus_;
    std::string postData_;
    bool isVerbose_;

  public:
    HttpClient();

    ~HttpClient();

    void SetUrl(const char* url)
    {
      url_ = std::string(url);
    }

    void SetUrl(const std::string& url)
    {
      url_ = url;
    }

    const std::string& GetUrl() const
    {
      return url_;
    }

    void SetMethod(Palantir_HttpMethod method)
    {
      method_ = method;
    }

    Palantir_HttpMethod GetMethod() const
    {
      return method_;
    }

    std::string& AccessPostData()
    {
      return postData_;
    }

    const std::string& AccessPostData() const
    {
      return postData_;
    }

    void SetVerbose(bool isVerbose);

    bool IsVerbose() const
    {
      return isVerbose_;
    }

    bool Apply(std::string& answer);

    bool Apply(Json::Value& answer);

    Palantir_HttpStatus GetLastStatus() const
    {
      return lastStatus_;
    }

    const char* GetLastStatusText() const
    {
      return HttpException::GetDescription(lastStatus_);
    }

  };
}
