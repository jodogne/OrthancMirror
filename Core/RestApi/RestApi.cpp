/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2018 Osimis S.A., Belgium
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


#include "../PrecompiledHeaders.h"
#include "RestApi.h"

#include "../Logging.h"

#include <stdlib.h>   // To define "_exit()" under Windows
#include <stdio.h>

namespace Orthanc
{
  namespace
  {
    // Anonymous namespace to avoid clashes between compilation modules
    class HttpHandlerVisitor : public RestApiHierarchy::IVisitor
    {
    private:
      RestApi& api_;
      RestApiOutput& output_;
      RequestOrigin origin_;
      const char* remoteIp_;
      const char* username_;
      HttpMethod method_;
      const IHttpHandler::Arguments& headers_;
      const IHttpHandler::Arguments& getArguments_;
      const char* bodyData_;
      size_t bodySize_;

    public:
      HttpHandlerVisitor(RestApi& api,
                         RestApiOutput& output,
                         RequestOrigin origin,
                         const char* remoteIp,
                         const char* username,
                         HttpMethod method,
                         const IHttpHandler::Arguments& headers,
                         const IHttpHandler::Arguments& getArguments,
                         const char* bodyData,
                         size_t bodySize) :
        api_(api),
        output_(output),
        origin_(origin),
        remoteIp_(remoteIp),
        username_(username),
        method_(method),
        headers_(headers),
        getArguments_(getArguments),
        bodyData_(bodyData),
        bodySize_(bodySize)
      {
      }

      virtual bool Visit(const RestApiHierarchy::Resource& resource,
                         const UriComponents& uri,
                         const IHttpHandler::Arguments& components,
                         const UriComponents& trailing)
      {
        if (resource.HasHandler(method_))
        {
          switch (method_)
          {
            case HttpMethod_Get:
            {
              RestApiGetCall call(output_, api_, origin_, remoteIp_, username_, 
                                  headers_, components, trailing, uri, getArguments_);
              resource.Handle(call);
              return true;
            }

            case HttpMethod_Post:
            {
              RestApiPostCall call(output_, api_, origin_, remoteIp_, username_, 
                                   headers_, components, trailing, uri, bodyData_, bodySize_);
              resource.Handle(call);
              return true;
            }

            case HttpMethod_Delete:
            {
              RestApiDeleteCall call(output_, api_, origin_, remoteIp_, username_, 
                                     headers_, components, trailing, uri);
              resource.Handle(call);
              return true;
            }

            case HttpMethod_Put:
            {
              RestApiPutCall call(output_, api_, origin_, remoteIp_, username_, 
                                  headers_, components, trailing, uri, bodyData_, bodySize_);
              resource.Handle(call);
              return true;
            }

            default:
              return false;
          }
        }

        return false;
      }
    };
  }



  static void AddMethod(std::string& target,
                        const std::string& method)
  {
    if (target.size() > 0)
      target += "," + method;
    else
      target = method;
  }

  static std::string  MethodsToString(const std::set<HttpMethod>& methods)
  {
    std::string s;

    if (methods.find(HttpMethod_Get) != methods.end())
    {
      AddMethod(s, "GET");
    }

    if (methods.find(HttpMethod_Post) != methods.end())
    {
      AddMethod(s, "POST");
    }

    if (methods.find(HttpMethod_Put) != methods.end())
    {
      AddMethod(s, "PUT");
    }

    if (methods.find(HttpMethod_Delete) != methods.end())
    {
      AddMethod(s, "DELETE");
    }

    return s;
  }



  bool RestApi::Handle(HttpOutput& output,
                       RequestOrigin origin,
                       const char* remoteIp,
                       const char* username,
                       HttpMethod method,
                       const UriComponents& uri,
                       const Arguments& headers,
                       const GetArguments& getArguments,
                       const char* bodyData,
                       size_t bodySize)
  {
    RestApiOutput wrappedOutput(output, method);

#if ORTHANC_ENABLE_PUGIXML == 1
    {
      // Look if the client wishes XML answers instead of JSON
      // http://www.w3.org/Protocols/HTTP/HTRQ_Headers.html#z3
      Arguments::const_iterator it = headers.find("accept");
      if (it != headers.end())
      {
        std::vector<std::string> accepted;
        Toolbox::TokenizeString(accepted, it->second, ';');
        for (size_t i = 0; i < accepted.size(); i++)
        {
          if (accepted[i] == "application/xml")
          {
            wrappedOutput.SetConvertJsonToXml(true);
          }

          if (accepted[i] == "application/json")
          {
            wrappedOutput.SetConvertJsonToXml(false);
          }
        }
      }
    }
#endif

    Arguments compiled;
    HttpToolbox::CompileGetArguments(compiled, getArguments);

    HttpHandlerVisitor visitor(*this, wrappedOutput, origin, remoteIp, username, 
                               method, headers, compiled, bodyData, bodySize);

    if (root_.LookupResource(uri, visitor))
    {
      wrappedOutput.Finalize();
      return true;
    }

    std::set<HttpMethod> methods;
    root_.GetAcceptedMethods(methods, uri);

    if (methods.empty())
    {
      return false;  // This URI is not served by this REST API
    }
    else
    {
      LOG(INFO) << "REST method " << EnumerationToString(method) 
                << " not allowed on: " << Toolbox::FlattenUri(uri);

      output.SendMethodNotAllowed(MethodsToString(methods));

      return true;
    }
  }

  void RestApi::Register(const std::string& path,
                         RestApiGetCall::Handler handler)
  {
    root_.Register(path, handler);
  }

  void RestApi::Register(const std::string& path,
                         RestApiPutCall::Handler handler)
  {
    root_.Register(path, handler);
  }

  void RestApi::Register(const std::string& path,
                         RestApiPostCall::Handler handler)
  {
    root_.Register(path, handler);
  }

  void RestApi::Register(const std::string& path,
                         RestApiDeleteCall::Handler handler)
  {
    root_.Register(path, handler);
  }
  
  void RestApi::AutoListChildren(RestApiGetCall& call)
  {
    RestApi& context = call.GetContext();

    Json::Value directory;
    if (context.root_.GetDirectory(directory, call.GetFullUri()))
    {
      call.GetOutput().AnswerJson(directory);
    }    
  }
}
