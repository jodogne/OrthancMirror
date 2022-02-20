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

#include "RestApiCall.h"

namespace Orthanc
{
  class RestApiPostCall : public RestApiCall
  {
  private:
    const void* bodyData_;
    size_t bodySize_;

  public:
    typedef void (*Handler) (RestApiPostCall& call);
    
    RestApiPostCall(RestApiOutput& output,
                    RestApi& context,
                    RequestOrigin origin,
                    const char* remoteIp,
                    const char* username,
                    const HttpToolbox::Arguments& httpHeaders,
                    const HttpToolbox::Arguments& uriComponents,
                    const UriComponents& trailing,
                    const UriComponents& fullUri,
                    const void* bodyData,
                    size_t bodySize) :
      RestApiCall(output, context, origin, remoteIp, username, HttpMethod_Post,
                  httpHeaders, uriComponents, trailing, fullUri),
      bodyData_(bodyData),
      bodySize_(bodySize)
    {
    }

    const void* GetBodyData() const
    {
      return bodyData_;
    }

    size_t GetBodySize() const
    {
      return bodySize_;
    }

    void BodyToString(std::string& result) const
    {
      result.assign(reinterpret_cast<const char*>(bodyData_), bodySize_);
    }

    virtual bool ParseJsonRequest(Json::Value& result) const ORTHANC_OVERRIDE
    {
      return Toolbox::ReadJson(result, bodyData_, bodySize_);
    }

    bool ParseBooleanBody() const
    {
      std::string s;
      BodyToString(s);
      return RestApiCall::ParseBoolean(s);
    }
  };
}
