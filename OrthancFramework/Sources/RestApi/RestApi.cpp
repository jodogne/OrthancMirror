/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
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


#include "../PrecompiledHeaders.h"
#include "RestApi.h"

#include "../HttpServer/StringHttpOutput.h"
#include "../Logging.h"
#include "../OrthancException.h"

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
      const HttpToolbox::Arguments& headers_;
      const HttpToolbox::Arguments& getArguments_;
      const void* bodyData_;
      size_t bodySize_;

    public:
      HttpHandlerVisitor(RestApi& api,
                         RestApiOutput& output,
                         RequestOrigin origin,
                         const char* remoteIp,
                         const char* username,
                         HttpMethod method,
                         const HttpToolbox::Arguments& headers,
                         const HttpToolbox::Arguments& getArguments,
                         const void* bodyData,
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
                         bool hasTrailing,
                         const HttpToolbox::Arguments& components,
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



    class OpenApiVisitor : public RestApiHierarchy::IVisitor
    {
    private:
      RestApi&    restApi_;
      Json::Value paths_;
  
    public:
      explicit OpenApiVisitor(RestApi& restApi) :
        restApi_(restApi)
      {
      }
  
      virtual bool Visit(const RestApiHierarchy::Resource& resource,
                         const UriComponents& uri,
                         bool hasTrailing,
                         const HttpToolbox::Arguments& components,
                         const UriComponents& trailing)
      {
        const std::string path = Toolbox::FlattenUri(uri);

        if (paths_.isMember(path))
        {
          throw OrthancException(ErrorCode_InternalError);
        }

        if (resource.HasHandler(HttpMethod_Get))
        {
          StringHttpOutput o1;
          HttpOutput o2(o1, false);
          RestApiOutput o3(o2, HttpMethod_Get);
          RestApiGetCall call(o3, restApi_, RequestOrigin_Documentation, "" /* remote IP */,
                              "" /* username */, HttpToolbox::Arguments() /* HTTP headers */,
                              HttpToolbox::Arguments() /* URI components */,
                              UriComponents() /* trailing */,
                              uri, HttpToolbox::Arguments() /* GET arguments */);

          bool ok = false;
          Json::Value v;
      
          try
          {
            ok = (resource.Handle(call) &&
                  call.GetDocumentation().FormatOpenApi(v));
          }
          catch (OrthancException&)
          {
          }
          catch (boost::bad_lexical_cast&)
          {
          }

          if (ok)
          {
            paths_[path]["get"] = v;
          }
          else
          {
            LOG(WARNING) << "Ignoring URI without API documentation: GET " << path;
          }
        }
    
        if (resource.HasHandler(HttpMethod_Post))
        {
          StringHttpOutput o1;
          HttpOutput o2(o1, false);
          RestApiOutput o3(o2, HttpMethod_Post);
          RestApiPostCall call(o3, restApi_, RequestOrigin_Documentation, "" /* remote IP */,
                               "" /* username */, HttpToolbox::Arguments() /* HTTP headers */,
                               HttpToolbox::Arguments() /* URI components */,
                               UriComponents() /* trailing */, uri, NULL /* body */, 0 /* body size */);

          bool ok = false;
          Json::Value v;
      
          try
          {
            ok = (resource.Handle(call) &&
                  call.GetDocumentation().FormatOpenApi(v));
          }
          catch (OrthancException&)
          {
          }
          catch (boost::bad_lexical_cast&)
          {
          }

          if (ok)
          {
            paths_[path]["post"] = v;
          }
          else
          {
            LOG(WARNING) << "Ignoring URI without API documentation: POST " << path;
          }
        }
    
        if (resource.HasHandler(HttpMethod_Delete))
        {
          StringHttpOutput o1;
          HttpOutput o2(o1, false);
          RestApiOutput o3(o2, HttpMethod_Delete);
          RestApiDeleteCall call(o3, restApi_, RequestOrigin_Documentation, "" /* remote IP */,
                                 "" /* username */, HttpToolbox::Arguments() /* HTTP headers */,
                                 HttpToolbox::Arguments() /* URI components */,
                                 UriComponents() /* trailing */, uri);

          bool ok = false;
          Json::Value v;
      
          try
          {
            ok = (resource.Handle(call) &&
                  call.GetDocumentation().FormatOpenApi(v));
          }
          catch (OrthancException&)
          {
          }
          catch (boost::bad_lexical_cast&)
          {
          }

          if (ok)
          {
            paths_[path]["delete"] = v;
          }
          else
          {
            LOG(WARNING) << "Ignoring URI without API documentation: DELETE " << path;
          }
        }

        if (resource.HasHandler(HttpMethod_Put))
        {
          StringHttpOutput o1;
          HttpOutput o2(o1, false);
          RestApiOutput o3(o2, HttpMethod_Put);
          RestApiPutCall call(o3, restApi_, RequestOrigin_Documentation, "" /* remote IP */,
                              "" /* username */, HttpToolbox::Arguments() /* HTTP headers */,
                              HttpToolbox::Arguments() /* URI components */,
                              UriComponents() /* trailing */, uri, NULL /* body */, 0 /* body size */);

          bool ok = false;
          Json::Value v;
      
          try
          {
            ok = (resource.Handle(call) &&
                  call.GetDocumentation().FormatOpenApi(v));
          }
          catch (OrthancException&)
          {
          }
          catch (boost::bad_lexical_cast&)
          {
          }

          if (ok)
          {
            paths_[path]["put"] = v;
          }
          else
          {
            LOG(WARNING) << "Ignoring URI without API documentation: PUT " << path;
          }
        }
    
        return true;
      }


      const Json::Value& GetPaths() const
      {
        return paths_;
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



  bool RestApi::CreateChunkedRequestReader(std::unique_ptr<IChunkedRequestReader>& target,
                                           RequestOrigin origin,
                                           const char* remoteIp,
                                           const char* username,
                                           HttpMethod method,
                                           const UriComponents& uri,
                                           const HttpToolbox::Arguments& headers)
  {
    return false;
  }


  bool RestApi::Handle(HttpOutput& output,
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
    RestApiOutput wrappedOutput(output, method);

#if ORTHANC_ENABLE_PUGIXML == 1
    {
      // Look if the client wishes XML answers instead of JSON
      // http://www.w3.org/Protocols/HTTP/HTRQ_Headers.html#z3
      HttpToolbox::Arguments::const_iterator it = headers.find("accept");
      if (it != headers.end())
      {
        std::vector<std::string> accepted;
        Toolbox::TokenizeString(accepted, it->second, ';');
        for (size_t i = 0; i < accepted.size(); i++)
        {
          if (accepted[i] == MIME_XML)
          {
            wrappedOutput.SetConvertJsonToXml(true);
          }

          if (accepted[i] == MIME_JSON)
          {
            wrappedOutput.SetConvertJsonToXml(false);
          }
        }
      }
    }
#endif

    HttpToolbox::Arguments compiled;
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
    call.GetDocumentation()
      .SetTag("Other")
      .SetSummary("List of operations")
      .SetDescription("List the available operations under URI: " + call.FlattenUri())
      .AddAnswerType(MimeType_Json, "List of the available operations");

    RestApi& context = call.GetContext();

    Json::Value directory;
    if (context.root_.GetDirectory(directory, call.GetFullUri()))
    {
      if (call.IsDocumentation())
      {
        call.GetDocumentation().SetSample(directory);
      }
      else
      {
        call.GetOutput().AnswerJson(directory);
      }
    }    
  }


  void RestApi::GenerateOpenApiDocumentation(Json::Value& target)
  {
    OpenApiVisitor visitor(*this);
    
    UriComponents root;
    root_.ExploreAllResources(visitor, root);

    target = Json::objectValue;

    target["info"]["version"] = ORTHANC_VERSION;
    target["info"]["title"] = "Orthanc";

    target["openapi"] = "3.0.0";

    target["servers"].append(Json::objectValue);
    target["servers"][0]["url"] = "https://demo.orthanc-server.com/";

    target["paths"] = visitor.GetPaths();
  }
}
