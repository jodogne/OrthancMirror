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
  class RestApiDeleteCall : public RestApiCall
  {
  public:
    typedef void (*Handler) (RestApiDeleteCall& call);
    
    RestApiDeleteCall(RestApiOutput& output,
                      RestApi& context,
                      RequestOrigin origin,
                      const char* remoteIp,
                      const char* username,
                      const HttpToolbox::Arguments& httpHeaders,
                      const HttpToolbox::Arguments& uriComponents,
                      const UriComponents& trailing,
                      const UriComponents& fullUri) :
      RestApiCall(output, context, origin, remoteIp, username, HttpMethod_Delete,
                  httpHeaders, uriComponents, trailing, fullUri)
    {
    }

    virtual bool ParseJsonRequest(Json::Value& result) const ORTHANC_OVERRIDE
    {
      result.clear();
      return true;
    }
  };
}
