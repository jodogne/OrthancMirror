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


#pragma once

#include "../../OrthancFramework/Sources/HttpServer/IHttpHandler.h"

namespace Orthanc
{
  class OrthancHttpHandler : public IHttpHandler
  {
  private:
    typedef std::list<IHttpHandler*> Handlers;

    Handlers      handlers_;
    IHttpHandler *orthancRestApi_;

  public:
    OrthancHttpHandler() : orthancRestApi_(NULL)
    {
    }

    virtual bool CreateChunkedRequestReader(std::unique_ptr<IChunkedRequestReader>& target,
                                            RequestOrigin origin,
                                            const char* remoteIp,
                                            const char* username,
                                            HttpMethod method,
                                            const UriComponents& uri,
                                            const HttpToolbox::Arguments& headers) ORTHANC_OVERRIDE;

    virtual bool Handle(HttpOutput& output,
                        RequestOrigin origin,
                        const char* remoteIp,
                        const char* username,
                        HttpMethod method,
                        const UriComponents& uri,
                        const HttpToolbox::Arguments& headers,
                        const HttpToolbox::GetArguments& getArguments,
                        const void* bodyData,
                        size_t bodySize) ORTHANC_OVERRIDE;

    void Register(IHttpHandler& handler,
                  bool isOrthancRestApi);

    bool HasOrthancRestApi() const
    {
      return orthancRestApi_ != NULL;
    }

    IHttpHandler& RestrictToOrthancRestApi(bool restrict);
  };
}
