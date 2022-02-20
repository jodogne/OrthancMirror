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


#include "../PrecompiledHeaders.h"
#include "RestApi.h"

#include "../HttpServer/StringHttpOutput.h"
#include "../Logging.h"
#include "../OrthancException.h"

#include <boost/algorithm/string/replace.hpp>
#include <boost/math/special_functions/round.hpp>
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



    class DocumentationVisitor : public RestApiHierarchy::IVisitor
    {
    private:
      RestApi&    restApi_;
      size_t      successPathsCount_;
      size_t      totalPathsCount_;

    protected:
      virtual bool HandleCall(RestApiCall& call,
                              const std::set<std::string>& uriArgumentsNames) = 0;
  
    public:
      explicit DocumentationVisitor(RestApi& restApi) :
        restApi_(restApi),
        successPathsCount_(0),
        totalPathsCount_(0)
      {
      }
  
      virtual bool Visit(const RestApiHierarchy::Resource& resource,
                         const UriComponents& uri,
                         bool hasTrailing,
                         const HttpToolbox::Arguments& components,
                         const UriComponents& trailing)
      {
        std::string path = Toolbox::FlattenUri(uri);
        if (hasTrailing)
        {
          path += "/{...}";
        }

        std::set<std::string> uriArgumentsNames;
        HttpToolbox::Arguments uriArguments;
        
        for (HttpToolbox::Arguments::const_iterator
               it = components.begin(); it != components.end(); ++it)
        {
          assert(it->second.empty());
          uriArgumentsNames.insert(it->first.c_str());
          uriArguments[it->first] = "";
        }

        if (hasTrailing)
        {
          uriArgumentsNames.insert("...");
          uriArguments["..."] = "";
        }

        if (resource.HasHandler(HttpMethod_Get))
        {
          totalPathsCount_ ++;
          
          StringHttpOutput o1;
          HttpOutput o2(o1, false);
          RestApiOutput o3(o2, HttpMethod_Get);
          RestApiGetCall call(o3, restApi_, RequestOrigin_Documentation, "" /* remote IP */,
                              "" /* username */, HttpToolbox::Arguments() /* HTTP headers */,
                              uriArguments, UriComponents() /* trailing */,
                              uri, HttpToolbox::Arguments() /* GET arguments */);

          bool ok = false;
      
          try
          {
            ok = (resource.Handle(call) &&
                  HandleCall(call, uriArgumentsNames));
          }
          catch (OrthancException& e)
          {
            LOG(ERROR) << "Exception while documenting GET " << path << ": " << e.What();
          }
          catch (boost::bad_lexical_cast&)
          {
            LOG(ERROR) << "Bad lexical cast while documenting GET " << path;
          }

          if (ok)
          {
            successPathsCount_ ++;
          }
          else
          {
            LOG(WARNING) << "Ignoring URI without API documentation: GET " << path;
          }
        }
    
        if (resource.HasHandler(HttpMethod_Post))
        {
          totalPathsCount_ ++;
          
          StringHttpOutput o1;
          HttpOutput o2(o1, false);
          RestApiOutput o3(o2, HttpMethod_Post);
          RestApiPostCall call(o3, restApi_, RequestOrigin_Documentation, "" /* remote IP */,
                               "" /* username */, HttpToolbox::Arguments() /* HTTP headers */,
                               uriArguments, UriComponents() /* trailing */,
                               uri, NULL /* body */, 0 /* body size */);

          bool ok = false;
      
          try
          {
            ok = (resource.Handle(call) &&
                  HandleCall(call, uriArgumentsNames));
          }
          catch (OrthancException& e)
          {
            LOG(ERROR) << "Exception while documenting POST " << path << ": " << e.What();
          }
          catch (boost::bad_lexical_cast&)
          {
            LOG(ERROR) << "Bad lexical cast while documenting POST " << path;
          }

          if (ok)
          {
            successPathsCount_ ++;
          }
          else
          {
            LOG(WARNING) << "Ignoring URI without API documentation: POST " << path;
          }
        }
    
        if (resource.HasHandler(HttpMethod_Delete))
        {
          totalPathsCount_ ++;
          
          StringHttpOutput o1;
          HttpOutput o2(o1, false);
          RestApiOutput o3(o2, HttpMethod_Delete);
          RestApiDeleteCall call(o3, restApi_, RequestOrigin_Documentation, "" /* remote IP */,
                                 "" /* username */, HttpToolbox::Arguments() /* HTTP headers */,
                                 uriArguments, UriComponents() /* trailing */, uri);

          bool ok = false;
      
          try
          {
            ok = (resource.Handle(call) &&
                  HandleCall(call, uriArgumentsNames));
          }
          catch (OrthancException& e)
          {
            LOG(ERROR) << "Exception while documenting DELETE " << path << ": " << e.What();
          }
          catch (boost::bad_lexical_cast&)
          {
            LOG(ERROR) << "Bad lexical cast while documenting DELETE " << path;
          }

          if (ok)
          {
            successPathsCount_ ++;
          }
          else
          {
            LOG(WARNING) << "Ignoring URI without API documentation: DELETE " << path;
          }
        }

        if (resource.HasHandler(HttpMethod_Put))
        {
          totalPathsCount_ ++;
          
          StringHttpOutput o1;
          HttpOutput o2(o1, false);
          RestApiOutput o3(o2, HttpMethod_Put);
          RestApiPutCall call(o3, restApi_, RequestOrigin_Documentation, "" /* remote IP */,
                              "" /* username */, HttpToolbox::Arguments() /* HTTP headers */,
                              uriArguments, UriComponents() /* trailing */, uri,
                              NULL /* body */, 0 /* body size */);

          bool ok = false;
      
          try
          {
            ok = (resource.Handle(call) &&
                  HandleCall(call, uriArgumentsNames));
          }
          catch (OrthancException& e)
          {
            LOG(ERROR) << "Exception while documenting PUT " << path << ": " << e.What();
          }
          catch (boost::bad_lexical_cast&)
          {
            LOG(ERROR) << "Bad lexical cast while documenting PUT " << path;
          }

          if (ok)
          {
            successPathsCount_ ++;
          }
          else
          {
            LOG(WARNING) << "Ignoring URI without API documentation: PUT " << path;
          }
        }
    
        return true;
      }

      size_t GetSuccessPathsCount() const
      {
        return successPathsCount_;
      }

      size_t GetTotalPathsCount() const
      {
        return totalPathsCount_;
      }

      void LogStatistics() const
      {
        assert(GetSuccessPathsCount() <= GetTotalPathsCount());
        size_t total = GetTotalPathsCount();
        if (total == 0)
        {
          total = 1;  // Avoid division by zero
        }
        float coverage = (100.0f * static_cast<float>(GetSuccessPathsCount()) /
                          static_cast<float>(total));
    
        LOG(WARNING) << "The documentation of the REST API contains " << GetSuccessPathsCount()
                     << " paths over a total of " << GetTotalPathsCount() << " paths "
                     << "(coverage: " << static_cast<unsigned int>(boost::math::iround(coverage)) << "%)";
      }
    };


    class OpenApiVisitor : public DocumentationVisitor
    {
    private:
      Json::Value paths_;

    protected:
      virtual bool HandleCall(RestApiCall& call,
                              const std::set<std::string>& uriArgumentsNames) ORTHANC_OVERRIDE
      {
        const std::string path = Toolbox::FlattenUri(call.GetFullUri());

        Json::Value v;
        if (call.GetDocumentation().FormatOpenApi(v, uriArgumentsNames, path))
        {
          std::string method;
          
          switch (call.GetMethod())
          {
            case HttpMethod_Get:
              method = "get";
              break;
            
            case HttpMethod_Post:
              method = "post";
              break;
            
            case HttpMethod_Delete:
              method = "delete";
              break;
            
            case HttpMethod_Put:
              method = "put";
              break;
            
            default:
              throw OrthancException(ErrorCode_ParameterOutOfRange);
          }
          
          if ((paths_.isMember(path) &&
               paths_[path].type() != Json::objectValue) ||
              paths_[path].isMember(method))
          {
            throw OrthancException(ErrorCode_InternalError);
          }

          paths_[path][method] = v;
          
          return true;
        }
        else
        {
          return false;
        }
      }      
  
    public:
      explicit OpenApiVisitor(RestApi& restApi) :
        DocumentationVisitor(restApi),
        paths_(Json::objectValue)
      {
      }
  
      const Json::Value& GetPaths() const
      {
        return paths_;
      }
    };


    class ReStructuredTextCheatSheet : public DocumentationVisitor
    {
    private:
      class Path
      {
      private:
        bool        hasGet_;
        bool        hasPost_;
        bool        hasDelete_;
        bool        hasPut_;
        std::string getTag_;
        std::string postTag_;
        std::string deleteTag_;
        std::string putTag_;
        std::string summary_;
        bool        getDeprecated_;
        bool        postDeprecated_;
        bool        deleteDeprecated_;
        bool        putDeprecated_;
        HttpMethod  summaryOrigin_;

      public:
        Path() :
          hasGet_(false),
          hasPost_(false),
          hasDelete_(false),
          hasPut_(false),
          getDeprecated_(false),
          postDeprecated_(false),
          deleteDeprecated_(false),
          putDeprecated_(false),
          summaryOrigin_(HttpMethod_Get)  // Dummy initialization
        {
        }

        void AddMethod(HttpMethod method,
                       const std::string& tag,
                       bool deprecated)
        {
          switch (method)
          {
            case HttpMethod_Get:
              if (hasGet_)
              {
                throw OrthancException(ErrorCode_InternalError);
              }
              
              hasGet_ = true;
              getTag_ = tag;
              getDeprecated_ = deprecated;
              break;
              
            case HttpMethod_Post:
              if (hasPost_)
              {
                throw OrthancException(ErrorCode_InternalError);
              }
              
              hasPost_ = true;
              postTag_ = tag;
              postDeprecated_ = deprecated;
              break;
              
            case HttpMethod_Delete:
              if (hasDelete_)
              {
                throw OrthancException(ErrorCode_InternalError);
              }
              
              hasDelete_ = true;
              deleteTag_ = tag;
              deleteDeprecated_ = deprecated;
              break;
              
            case HttpMethod_Put:
              if (hasPut_)
              {
                throw OrthancException(ErrorCode_InternalError);
              }
              
              hasPut_ = true;
              putTag_ = tag;
              putDeprecated_ = deprecated;
              break;

            default:
              throw OrthancException(ErrorCode_ParameterOutOfRange);
          }
        }

        void SetSummary(const std::string& summary,
                        HttpMethod newOrigin)
        {
          if (!summary.empty())
          {
            bool replace;

            if (summary_.empty())
            {
              // We don't have a summary so far
              replace = true;
            }
            else
            {
              // We already have a summary. Replace it if the new
              // summary is associated with a HTTP method of higher
              // weight (GET > POST > DELETE > PUT)
              switch (summaryOrigin_)
              {
                case HttpMethod_Get:
                  replace = false;
                  break;

                case HttpMethod_Post:
                  replace = (newOrigin == HttpMethod_Get);
                  break;

                case HttpMethod_Delete:
                  replace = (newOrigin == HttpMethod_Get ||
                             newOrigin == HttpMethod_Post);
                  break;

                case HttpMethod_Put:
                  replace = (newOrigin == HttpMethod_Get ||
                             newOrigin == HttpMethod_Post ||
                             newOrigin == HttpMethod_Delete);
                  break;

                default:
                  throw OrthancException(ErrorCode_ParameterOutOfRange);
              }
            }

            if (replace)
            {
              summary_ = summary;
              summaryOrigin_ = newOrigin;
            }
          }
        }

        const std::string& GetSummary() const
        {
          return summary_;
        }

        static std::string FormatTag(const std::string& tag)
        {
          if (tag.empty())
          {
            return tag;
          }
          else
          {
            std::string s;
            s.reserve(tag.size());
            s.push_back(tag[0]);

            for (size_t i = 1; i < tag.size(); i++)
            {
              if (tag[i] == ' ')
              {
                s.push_back('-');
              }
              else if (isupper(tag[i]) &&
                       tag[i - 1] == ' ')
              {
                s.push_back(tolower(tag[i]));
              }
              else
              {
                s.push_back(tag[i]);
              }
            }

            return s;
          }
        }

        std::string Format(const std::string& openApiUrl,
                           HttpMethod method,
                           const std::string& uri) const
        {
          std::string p = uri;
          boost::replace_all(p, "/", "~1");

          std::string verb;
          std::string url;
          
          switch (method)
          {
            case HttpMethod_Get:
              if (hasGet_)
              {
                verb = (getDeprecated_ ? "(get)" : "GET");
                url = openApiUrl + "#tag/" + FormatTag(getTag_) + "/paths/" + p + "/get";
              }
              break;
              
            case HttpMethod_Post:
              if (hasPost_)
              {
                verb = (postDeprecated_ ? "(post)" : "POST");
                url = openApiUrl + "#tag/" + FormatTag(postTag_) + "/paths/" + p + "/post";
              }
              break;
              
            case HttpMethod_Delete:
              if (hasDelete_)
              {
                verb = (deleteDeprecated_ ? "(delete)" : "DELETE");
                url = openApiUrl + "#tag/" + FormatTag(deleteTag_) + "/paths/" + p + "/delete";
              }
              break;
              
            case HttpMethod_Put:
              if (hasPut_)
              {
                verb = (putDeprecated_ ? "(put)" : "PUT");
                url = openApiUrl + "#tag/" + FormatTag(putTag_) + "/paths/" + p + "/put";
              }
              break;              

            default:
              throw OrthancException(ErrorCode_InternalError);
          }

          if (verb.empty())
          {
            return "";
          }
          else if (openApiUrl.empty())
          {
            return verb;
          }
          else
          {
            return "`" + verb + " <" + url + ">`__";
          }
        }

        bool HasDeprecated() const
        {
          return ((hasGet_ && getDeprecated_) ||
                  (hasPost_ && postDeprecated_) ||
                  (hasDelete_ && deleteDeprecated_) ||
                  (hasPut_ && putDeprecated_));
        }
      };

      typedef std::map<std::string, Path>  Paths;

      Paths paths_;

    protected:
      virtual bool HandleCall(RestApiCall& call,
                              const std::set<std::string>& uriArgumentsNames) ORTHANC_OVERRIDE
      {
        Path& path = paths_[ Toolbox::FlattenUri(call.GetFullUri()) ];

        path.AddMethod(call.GetMethod(), call.GetDocumentation().GetTag(), call.GetDocumentation().IsDeprecated());

        if (call.GetDocumentation().HasSummary())
        {
          path.SetSummary(call.GetDocumentation().GetSummary(), call.GetMethod());
        }
        
        return true;
      }      
  
    public:
      explicit ReStructuredTextCheatSheet(RestApi& restApi) :
        DocumentationVisitor(restApi)
      {
      }

      void Format(std::string& target,
                  const std::string& openApiUrl) const
      {
        target += "Path,GET,POST,DELETE,PUT,Summary\n";
        for (Paths::const_iterator it = paths_.begin(); it != paths_.end(); ++it)
        {
          target += "``" + it->first + "``,";
          target += it->second.Format(openApiUrl, HttpMethod_Get, it->first) + ",";
          target += it->second.Format(openApiUrl, HttpMethod_Post, it->first) + ",";
          target += it->second.Format(openApiUrl, HttpMethod_Delete, it->first) + ",";
          target += it->second.Format(openApiUrl, HttpMethod_Put, it->first) + ",";
          
          if (it->second.HasDeprecated())
          {
            target += "*(deprecated)* ";
          }
          
          target += it->second.GetSummary() + "\n";
        }        
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
      .SetSummary("List operations")
      .SetDescription("List the available operations under URI `" + call.FlattenUri() + "`")
      .AddAnswerType(MimeType_Json, "List of the available operations");

    RestApi& context = call.GetContext();

    Json::Value directory;
    if (context.root_.GetDirectory(directory, call.GetFullUri()))
    {
      if (call.IsDocumentation())
      {
        call.GetDocumentation().SetSample(directory);

        std::set<std::string> c;
        call.GetUriComponentsNames(c);
        for (std::set<std::string>::const_iterator it = c.begin(); it != c.end(); ++it)
        {
          call.GetDocumentation().SetUriArgument(*it, RestApiCallDocumentation::Type_String, "");
        }    
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
    std::set<std::string> uriArgumentsNames;
    root_.ExploreAllResources(visitor, root, uriArgumentsNames);

    target = Json::objectValue;

    target["info"] = Json::objectValue;
    target["openapi"] = "3.0.0";
    target["servers"] = Json::arrayValue;
    target["paths"] = visitor.GetPaths();

    visitor.LogStatistics();
  }


  void RestApi::GenerateReStructuredTextCheatSheet(std::string& target,
                                                   const std::string& openApiUrl)
  {
    ReStructuredTextCheatSheet visitor(*this);
    
    UriComponents root;
    std::set<std::string> uriArgumentsNames;
    root_.ExploreAllResources(visitor, root, uriArgumentsNames);

    visitor.Format(target, openApiUrl);
    
    visitor.LogStatistics();
  }
}
