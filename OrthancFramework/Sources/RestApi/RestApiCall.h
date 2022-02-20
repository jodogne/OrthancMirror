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


#pragma once

#include "../HttpServer/HttpToolbox.h"
#include "RestApiCallDocumentation.h"
#include "RestApiPath.h"
#include "RestApiOutput.h"

#include <boost/noncopyable.hpp>
#include <set>

namespace Orthanc
{
  class RestApi;

  class RestApiCall : public boost::noncopyable
  {
  private:
    RestApiOutput& output_;
    RestApi& context_;
    RequestOrigin origin_;
    const char* remoteIp_;
    const char* username_;
    const HttpToolbox::Arguments& httpHeaders_;
    const HttpToolbox::Arguments& uriComponents_;
    const UriComponents& trailing_;
    const UriComponents& fullUri_;
    HttpMethod method_;  // To create RestApiCallDocumentation on demand
    std::unique_ptr<RestApiCallDocumentation>  documentation_;  // Lazy creation

  public:
    RestApiCall(RestApiOutput& output,
                RestApi& context,
                RequestOrigin origin,
                const char* remoteIp,
                const char* username,
                HttpMethod method,
                const HttpToolbox::Arguments& httpHeaders,
                const HttpToolbox::Arguments& uriComponents,
                const UriComponents& trailing,
                const UriComponents& fullUri) :
      output_(output),
      context_(context),
      origin_(origin),
      remoteIp_(remoteIp),
      username_(username),
      httpHeaders_(httpHeaders),
      uriComponents_(uriComponents),
      trailing_(trailing),
      fullUri_(fullUri),
      method_(method)
    {
    }

    RestApiOutput& GetOutput()
    {
      return output_;
    }

    RestApi& GetContext()
    {
      return context_;
    }
    
    const UriComponents& GetFullUri() const
    {
      return fullUri_;
    }
    
    const UriComponents& GetTrailingUri() const
    {
      return trailing_;
    }

    void GetUriComponentsNames(std::set<std::string>& components) const;

    bool HasUriComponent(const std::string& name) const
    {
      return (uriComponents_.find(name) != uriComponents_.end());
    }

    std::string GetUriComponent(const std::string& name,
                                const std::string& defaultValue) const
    {
      return HttpToolbox::GetArgument(uriComponents_, name, defaultValue);
    }

    std::string GetHttpHeader(const std::string& name,
                              const std::string& defaultValue) const
    {
      return HttpToolbox::GetArgument(httpHeaders_, name, defaultValue);
    }

    const HttpToolbox::Arguments& GetHttpHeaders() const
    {
      return httpHeaders_;
    }

    void ParseCookies(HttpToolbox::Arguments& result) const
    {
      HttpToolbox::ParseCookies(result, httpHeaders_);
    }

    std::string FlattenUri() const;

    RequestOrigin GetRequestOrigin() const
    {
      return origin_;
    }

    const char* GetRemoteIp() const
    {
      return remoteIp_;
    }

    const char* GetUsername() const
    {
      return username_;
    }

    virtual bool ParseJsonRequest(Json::Value& result) const = 0;

    RestApiCallDocumentation& GetDocumentation();

    HttpMethod GetMethod() const
    {
      return method_;
    }

    bool IsDocumentation() const
    {
      return (origin_ == RequestOrigin_Documentation);
    }

    static bool ParseBoolean(const std::string& value);
  };
}
