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

#include "HttpToolbox.h"

namespace Orthanc
{
  class IIncomingHttpRequestFilter : public boost::noncopyable
  {
  public:
    virtual ~IIncomingHttpRequestFilter()
    {
    }

    // New in Orthanc 1.8.1
    virtual bool IsValidBearerToken(const std::string& token) = 0;
    
    virtual bool IsAllowed(HttpMethod method,
                           const char* uri,
                           const char* ip,
                           const char* username,
                           const HttpToolbox::Arguments& httpHeaders,
                           const HttpToolbox::GetArguments& getArguments) = 0;
  };
}
