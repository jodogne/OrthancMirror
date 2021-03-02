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

#if ORTHANC_SANDBOXED == 1
#  error This file cannot be used in sandboxed environments
#endif

#include "../Compatibility.h"
#include "../Toolbox.h"
#include "HttpOutput.h"
#include "HttpToolbox.h"

#include <map>
#include <set>
#include <vector>
#include <string>
#include <memory>

namespace Orthanc
{
  class IHttpHandler : public boost::noncopyable
  {
  public:
    class IChunkedRequestReader : public boost::noncopyable
    {
    public:
      virtual ~IChunkedRequestReader()
      {
      }

      virtual void AddBodyChunk(const void* data,
                                size_t size) = 0;

      virtual void Execute(HttpOutput& output) = 0;
    };


    virtual ~IHttpHandler()
    {
    }

    /**
     * This function allows one to deal with chunked transfers (new in
     * Orthanc 1.5.7). It is only called if "method" is POST or PUT.
     **/
    virtual bool CreateChunkedRequestReader(std::unique_ptr<IChunkedRequestReader>& target,
                                            RequestOrigin origin,
                                            const char* remoteIp,
                                            const char* username,
                                            HttpMethod method,
                                            const UriComponents& uri,
                                            const HttpToolbox::Arguments& headers) = 0;

    virtual bool Handle(HttpOutput& output,
                        RequestOrigin origin,
                        const char* remoteIp,
                        const char* username,
                        HttpMethod method,
                        const UriComponents& uri,
                        const HttpToolbox::Arguments& headers,
                        const HttpToolbox::GetArguments& getArguments,
                        const void* bodyData,
                        size_t bodySize) = 0;

    static bool SimpleGet(std::string& result,
                          IHttpHandler& handler,
                          RequestOrigin origin,
                          const std::string& uri,
                          const HttpToolbox::Arguments& httpHeaders);

    static bool SimplePost(std::string& result,
                           IHttpHandler& handler,
                           RequestOrigin origin,
                           const std::string& uri,
                           const void* bodyData,
                           size_t bodySize,
                           const HttpToolbox::Arguments& httpHeaders);

    static bool SimplePut(std::string& result,
                          IHttpHandler& handler,
                          RequestOrigin origin,
                          const std::string& uri,
                          const void* bodyData,
                          size_t bodySize,
                          const HttpToolbox::Arguments& httpHeaders);

    static bool SimpleDelete(IHttpHandler& handler,
                             RequestOrigin origin,
                             const std::string& uri,
                             const HttpToolbox::Arguments& httpHeaders);
  };
}
