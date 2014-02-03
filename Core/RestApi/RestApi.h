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

#include <list>

namespace Orthanc
{
  class RestApi : public HttpHandler
  {
  public:
    class Call
    {
      friend class RestApi;

    private:
      RestApiOutput& output_;
      RestApi& context_;
      const HttpHandler::Arguments& httpHeaders_;
      const RestApiPath::Components& uriComponents_;
      const UriComponents& trailing_;
      const UriComponents& fullUri_;

      Call(RestApiOutput& output,
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

    protected:
      static bool ParseJsonRequestInternal(Json::Value& result,
                                           const char* request);

    public:
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

 
    class GetCall : public Call
    {
      friend class RestApi;

    private:
      const HttpHandler::Arguments& getArguments_;

    public:
      GetCall(RestApiOutput& output,
              RestApi& context,
              const HttpHandler::Arguments& httpHeaders,
              const RestApiPath::Components& uriComponents,
              const UriComponents& trailing,
              const UriComponents& fullUri,
              const HttpHandler::Arguments& getArguments) :
        Call(output, context, httpHeaders, uriComponents, trailing, fullUri),
        getArguments_(getArguments)
      {
      }

      std::string GetArgument(const std::string& name,
                              const std::string& defaultValue) const
      {
        return HttpHandler::GetArgument(getArguments_, name, defaultValue);
      }

      bool HasArgument(const std::string& name) const
      {
        return getArguments_.find(name) != getArguments_.end();
      }

      virtual bool ParseJsonRequest(Json::Value& result) const;
    };


    class PutCall : public Call
    {
      friend class RestApi;

    private:
      const std::string& data_;

    public:
      PutCall(RestApiOutput& output,
              RestApi& context,
              const HttpHandler::Arguments& httpHeaders,
              const RestApiPath::Components& uriComponents,
              const UriComponents& trailing,
              const UriComponents& fullUri,
              const std::string& data) :
        Call(output, context, httpHeaders, uriComponents, trailing, fullUri),
        data_(data)
      {
      }

      const std::string& GetPutBody() const
      {
        return data_;
      }

      virtual bool ParseJsonRequest(Json::Value& result) const
      {
        return ParseJsonRequestInternal(result, GetPutBody().c_str());
      }      
    };

    class PostCall : public Call
    {
      friend class RestApi;

    private:
      const std::string& data_;

    public:
      PostCall(RestApiOutput& output,
               RestApi& context,
               const HttpHandler::Arguments& httpHeaders,
               const RestApiPath::Components& uriComponents,
               const UriComponents& trailing,
               const UriComponents& fullUri,
               const std::string& data) :
        Call(output, context, httpHeaders, uriComponents, trailing, fullUri),
        data_(data)
      {
      }

      const std::string& GetPostBody() const
      {
        return data_;
      }

      virtual bool ParseJsonRequest(Json::Value& result) const
      {
        return ParseJsonRequestInternal(result, GetPostBody().c_str());
      }      
    };

    class DeleteCall : public Call
    {
    public:
      DeleteCall(RestApiOutput& output,
                 RestApi& context,
                 const HttpHandler::Arguments& httpHeaders,
                 const RestApiPath::Components& uriComponents,
                 const UriComponents& trailing,
                 const UriComponents& fullUri) :
        Call(output, context, httpHeaders, uriComponents, trailing, fullUri)
      {
      }

      virtual bool ParseJsonRequest(Json::Value& result) const
      {
        result.clear();
        return true;
      }
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
                        HttpMethod method,
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
