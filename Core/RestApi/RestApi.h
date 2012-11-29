/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012 Medical Physics Department, CHU of Liege,
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
  class RestApi : public HttpHandler
  {
  private:
    class SharedCall
    {
      friend class RestApi;

    private:
      RestApiOutput* output_;
      RestApi* context_;
      const HttpHandler::Arguments* httpHeaders_;
      const RestApiPath::Components* uriComponents_;
      const UriComponents* trailing_;
      const UriComponents* fullUri_;

    public:
      RestApiOutput& GetOutput()
      {
        return *output_;
      }

      RestApi& GetContext()
      {
        return *context_;
      }
    
      const UriComponents& GetFullUri() const
      {
        return *fullUri_;
      }

      std::string GetUriComponent(const std::string& name,
                                  const std::string& defaultValue)
      {
        return HttpHandler::GetArgument(*uriComponents_, name, defaultValue);
      }

      std::string GetHttpHeader(const std::string& name,
                                const std::string& defaultValue)
      {
        return HttpHandler::GetArgument(*httpHeaders_, name, defaultValue);
      }
    };

 
  public:
    class GetCall : public SharedCall
    {
      friend class RestApi;

    private:
      const HttpHandler::Arguments* getArguments_;

    public:
      std::string GetArgument(const std::string& name,
                              const std::string& defaultValue)
      {
        return HttpHandler::GetArgument(*getArguments_, name, defaultValue);
      }
    };

    class PutCall : public SharedCall
    {
      friend class RestApi;

    private:
      const std::string* data_;

    public:
      const std::string& GetData()
      {
        return *data_;
      }
    };

    class PostCall : public SharedCall
    {
      friend class RestApi;

    private:
      const std::string* data_;

    public:
      const std::string& GetData()
      {
        return *data_;
      }
    };

    class DeleteCall : public SharedCall
    {
    };

    typedef void (*GetHandler) (GetCall& call);
    
    typedef void (*DeleteHandler) (DeleteCall& call);
    
    typedef void (*PutHandler) (PutCall& call);
    
    typedef void (*PostHandler) (PostCall& call);
    
  private:
    typedef std::list< std::pair<RestApiPath*, GetHandler> > GetHandlers;
    typedef std::list< std::pair<RestApiPath*, PutHandler> > PutHandlers;
    typedef std::list< std::pair<RestApiPath*, PostHandler> > PostHandlers;
    typedef std::list< std::pair<RestApiPath*, DeleteHandler> > DeleteHandlers;

    GetHandlers  getHandlers_;
    PutHandlers  putHandlers_;
    PostHandlers  postHandlers_;
    DeleteHandlers  deleteHandlers_;

    bool IsGetAccepted(const UriComponents& uri);
    bool IsPutAccepted(const UriComponents& uri);
    bool IsPostAccepted(const UriComponents& uri);
    bool IsDeleteAccepted(const UriComponents& uri);

    std::string  GetAcceptedMethods(const UriComponents& uri);

  public:
    RestApi()
    {
    }

    ~RestApi();

    virtual bool IsServedUri(const UriComponents& uri);

    virtual void Handle(HttpOutput& output,
                        const std::string& method,
                        const UriComponents& uri,
                        const Arguments& headers,
                        const Arguments& getArguments,
                        const std::string& postData);

    void Register(const std::string& path,
                  GetHandler handler);

    void Register(const std::string& path,
                  PutHandler handler);

    void Register(const std::string& path,
                  PostHandler handler);

    void Register(const std::string& path,
                  DeleteHandler handler);
  };
}
