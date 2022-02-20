/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#include "PrecompiledHeadersServer.h"
#include "OrthancHttpHandler.h"

#include "../../OrthancFramework/Sources/OrthancException.h"


namespace Orthanc
{
  bool OrthancHttpHandler::CreateChunkedRequestReader(
    std::unique_ptr<IHttpHandler::IChunkedRequestReader>& target,
    RequestOrigin origin,
    const char* remoteIp,
    const char* username,
    HttpMethod method,
    const UriComponents& uri,
    const HttpToolbox::Arguments& headers)
  {
    if (method != HttpMethod_Post &&
        method != HttpMethod_Put)
    {
      throw OrthancException(ErrorCode_InternalError);
    }

    for (Handlers::const_iterator it = handlers_.begin(); it != handlers_.end(); ++it) 
    {
      if ((*it)->CreateChunkedRequestReader
          (target, origin, remoteIp, username, method, uri, headers))
      {
        if (target.get() == NULL)
        {
          throw OrthancException(ErrorCode_InternalError);
        }

        return true;
      }
    }

    return false;
  }


  bool OrthancHttpHandler::Handle(HttpOutput& output,
                                  RequestOrigin origin,
                                  const char* remoteIp,
                                  const char* username,
                                  HttpMethod method,
                                  const UriComponents& uri,
                                  const HttpToolbox::Arguments& headers,
                                  const HttpToolbox::GetArguments& getArguments,
                                  const void* bodyData,
                                  size_t bodySize)
  {
    for (Handlers::const_iterator it = handlers_.begin(); it != handlers_.end(); ++it) 
    {
      if ((*it)->Handle(output, origin, remoteIp, username, method, uri, 
                        headers, getArguments, bodyData, bodySize))
      {
        return true;
      }
    }

    return false;
  }


  void OrthancHttpHandler::Register(IHttpHandler& handler,
                                    bool isOrthancRestApi)
  {
    handlers_.push_back(&handler);

    if (isOrthancRestApi)
    {
      orthancRestApi_ = &handler;
    }
  }


  IHttpHandler& OrthancHttpHandler::RestrictToOrthancRestApi(bool restrict)
  {
    if (restrict)
    {
      if (orthancRestApi_ == NULL)
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      return *orthancRestApi_;
    }
    else
    {
      return *this;
    }
  }
}
