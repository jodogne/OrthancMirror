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


#pragma once

#include "../HttpServer/HttpHandler.h"
#include "RestApiPath.h"
#include "RestApiOutput.h"

namespace Orthanc
{
  class RestApi;

  class RestApiCall
  {
  private:
    RestApiOutput& output_;
    RestApi& context_;
    const HttpHandler::Arguments& httpHeaders_;
    const RestApiPath::Components& uriComponents_;
    const UriComponents& trailing_;
    const UriComponents& fullUri_;

  protected:
    static bool ParseJsonRequestInternal(Json::Value& result,
                                         const char* request);

  public:
    RestApiCall(RestApiOutput& output,
                RestApi& context,
                const HttpHandler::Arguments& httpHeaders,
                const RestApiPath::Components& uriComponents,
                const UriComponents& trailing,
                const UriComponents& fullUri) :
      output_(output),
      context_(context),
      httpHeaders_(httpHeaders),
      uriComponents_(uriComponents),
      trailing_(trailing),
      fullUri_(fullUri)
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

    std::string GetUriComponent(const std::string& name,
                                const std::string& defaultValue) const
    {
      return HttpHandler::GetArgument(uriComponents_, name, defaultValue);
    }

    std::string GetHttpHeader(const std::string& name,
                              const std::string& defaultValue) const
    {
      return HttpHandler::GetArgument(httpHeaders_, name, defaultValue);
    }

    const HttpHandler::Arguments& GetHttpHeaders() const
    {
      return httpHeaders_;
    }

    void ParseCookies(HttpHandler::Arguments& result) const
    {
      HttpHandler::ParseCookies(result, httpHeaders_);
    }

    virtual bool ParseJsonRequest(Json::Value& result) const = 0;
  };
}
