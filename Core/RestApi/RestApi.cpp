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


#include "RestApi.h"

#include <stdlib.h>   // To define "_exit()" under Windows
#include <glog/logging.h>

namespace Orthanc
{
  bool RestApi::Call::ParseJsonRequestInternal(Json::Value& result,
                                               const char* request)
  {
    result.clear();
    Json::Reader reader;
    return reader.parse(request, result);
  }


  bool RestApi::GetCall::ParseJsonRequest(Json::Value& result) const
  {
    result.clear();

    for (HttpHandler::Arguments::const_iterator 
           it = getArguments_->begin(); it != getArguments_->end(); it++)
    {
      result[it->first] = result[it->second];
    }

    return true;
  }


  bool RestApi::IsGetAccepted(const UriComponents& uri)
  {
    for (GetHandlers::const_iterator it = getHandlers_.begin();
         it != getHandlers_.end(); it++)
    {
      if (it->first->Match(uri))
      {
        return true;
      }
    }

    return false;
  }

  bool RestApi::IsPutAccepted(const UriComponents& uri)
  {
    for (PutHandlers::const_iterator it = putHandlers_.begin();
         it != putHandlers_.end(); it++)
    {
      if (it->first->Match(uri))
      {
        return true;
      }
    }

    return false;
  }

  bool RestApi::IsPostAccepted(const UriComponents& uri)
  {
    for (PostHandlers::const_iterator it = postHandlers_.begin();
         it != postHandlers_.end(); it++)
    {
      if (it->first->Match(uri))
      {
        return true;
      }
    }

    return false;
  }

  bool RestApi::IsDeleteAccepted(const UriComponents& uri)
  {
    for (DeleteHandlers::const_iterator it = deleteHandlers_.begin();
         it != deleteHandlers_.end(); it++)
    {
      if (it->first->Match(uri))
      {
        return true;
      }
    }

    return false;
  }

  static void AddMethod(std::string& target,
                        const std::string& method)
  {
    if (target.size() > 0)
      target += "," + method;
    else
      target = method;
  }

  std::string  RestApi::GetAcceptedMethods(const UriComponents& uri)
  {
    std::string s;

    if (IsGetAccepted(uri))
      AddMethod(s, "GET");

    if (IsPutAccepted(uri))
      AddMethod(s, "PUT");

    if (IsPostAccepted(uri))
      AddMethod(s, "POST");

    if (IsDeleteAccepted(uri))
      AddMethod(s, "DELETE");

    return s;
  }

  RestApi::~RestApi()
  {
    for (GetHandlers::iterator it = getHandlers_.begin(); 
         it != getHandlers_.end(); it++)
    {
      delete it->first;
    } 

    for (PutHandlers::iterator it = putHandlers_.begin(); 
         it != putHandlers_.end(); it++)
    {
      delete it->first;
    } 

    for (PostHandlers::iterator it = postHandlers_.begin(); 
         it != postHandlers_.end(); it++)
    {
      delete it->first;
    } 

    for (DeleteHandlers::iterator it = deleteHandlers_.begin(); 
         it != deleteHandlers_.end(); it++)
    {
      delete it->first;
    } 
  }

  bool RestApi::IsServedUri(const UriComponents& uri)
  {
    return (IsGetAccepted(uri) ||
            IsPutAccepted(uri) ||
            IsPostAccepted(uri) ||
            IsDeleteAccepted(uri));
  }

  void RestApi::Handle(HttpOutput& output,
                       const std::string& method,
                       const UriComponents& uri,
                       const Arguments& headers,
                       const Arguments& getArguments,
                       const std::string& postData)
  {
    bool ok = false;
    RestApiOutput restOutput(output);
    RestApiPath::Components components;
    UriComponents trailing;

    if (method == "GET")
    {
      for (GetHandlers::const_iterator it = getHandlers_.begin();
           it != getHandlers_.end(); it++)
      {
        if (it->first->Match(components, trailing, uri))
        {
          LOG(INFO) << "REST GET call on: " << Toolbox::FlattenUri(uri);
          ok = true;
          GetCall call;
          call.output_ = &restOutput;
          call.context_ = this;
          call.httpHeaders_ = &headers;
          call.uriComponents_ = &components;
          call.trailing_ = &trailing;
          call.fullUri_ = &uri;
          
          call.getArguments_ = &getArguments;
          it->second(call);
        }
      }
    }
    else if (method == "PUT")
    {
      for (PutHandlers::const_iterator it = putHandlers_.begin();
           it != putHandlers_.end(); it++)
      {
        if (it->first->Match(components, trailing, uri))
        {
          LOG(INFO) << "REST PUT call on: " << Toolbox::FlattenUri(uri);
          ok = true;
          PutCall call;
          call.output_ = &restOutput;
          call.context_ = this;
          call.httpHeaders_ = &headers;
          call.uriComponents_ = &components;
          call.trailing_ = &trailing;
          call.fullUri_ = &uri;
           
          call.data_ = &postData;
          it->second(call);
        }
      }
    }
    else if (method == "POST")
    {
      for (PostHandlers::const_iterator it = postHandlers_.begin();
           it != postHandlers_.end(); it++)
      {
        if (it->first->Match(components, trailing, uri))
        {
          LOG(INFO) << "REST POST call on: " << Toolbox::FlattenUri(uri);
          ok = true;
          PostCall call;
          call.output_ = &restOutput;
          call.context_ = this;
          call.httpHeaders_ = &headers;
          call.uriComponents_ = &components;
          call.trailing_ = &trailing;
          call.fullUri_ = &uri;
           
          call.data_ = &postData;
          it->second(call);
        }
      }
    }
    else if (method == "DELETE")
    {
      for (DeleteHandlers::const_iterator it = deleteHandlers_.begin();
           it != deleteHandlers_.end(); it++)
      {
        if (it->first->Match(components, trailing, uri))
        {
          LOG(INFO) << "REST DELETE call on: " << Toolbox::FlattenUri(uri);
          ok = true;
          DeleteCall call;
          call.output_ = &restOutput;
          call.context_ = this;
          call.httpHeaders_ = &headers;
          call.uriComponents_ = &components;
          call.trailing_ = &trailing;
          call.fullUri_ = &uri;
          it->second(call);
        }
      }
    }

    if (!ok)
    {
      LOG(INFO) << "REST method " << method << " not allowed on: " << Toolbox::FlattenUri(uri);
      output.SendMethodNotAllowedError(GetAcceptedMethods(uri));
    }
  }

  void RestApi::Register(const std::string& path,
                         GetHandler handler)
  {
    getHandlers_.push_back(std::make_pair(new RestApiPath(path), handler));
  }

  void RestApi::Register(const std::string& path,
                         PutHandler handler)
  {
    putHandlers_.push_back(std::make_pair(new RestApiPath(path), handler));
  }

  void RestApi::Register(const std::string& path,
                         PostHandler handler)
  {
    postHandlers_.push_back(std::make_pair(new RestApiPath(path), handler));
  }

  void RestApi::Register(const std::string& path,
                         DeleteHandler handler)
  {
    deleteHandlers_.push_back(std::make_pair(new RestApiPath(path), handler));
  }
}
