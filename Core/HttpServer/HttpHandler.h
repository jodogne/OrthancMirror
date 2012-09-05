/**
 * Palanthir - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012 Medical Physics Department, CHU of Liege,
 * Belgium
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

#include <map>
#include <vector>
#include <stdint.h>
#include "HttpOutput.h"
#include "../Toolbox.h"

namespace Palanthir
{
  class HttpHandler
  {
  public:
    typedef std::map<std::string, std::string> Arguments;

    virtual ~HttpHandler()
    {
    }

    virtual bool IsServedUri(const UriComponents& uri) = 0;

    virtual void Handle(HttpOutput& output,
                        const std::string& method,
                        const UriComponents& uri,
                        const Arguments& headers,
                        const Arguments& arguments,
                        const std::string& postData) = 0;

    static void ParseGetQuery(HttpHandler::Arguments& result, 
                              const char* query);

    static std::string GetArgument(const Arguments& arguments,
                                   const std::string& name,
                                   const std::string& defaultValue);
  };
}
