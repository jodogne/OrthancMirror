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
#include "RestApiHierarchy.h"

#include "../OrthancException.h"

#include <cassert>
#include <stdio.h>

namespace Orthanc
{
  RestApiHierarchy::Resource::Resource() : 
    getHandler_(NULL), 
    postHandler_(NULL),
    putHandler_(NULL), 
    deleteHandler_(NULL)
  {
  }


  bool RestApiHierarchy::Resource::HasHandler(HttpMethod method) const
  {
    switch (method)
    {
      case HttpMethod_Get:
        return getHandler_ != NULL;

      case HttpMethod_Post:
        return postHandler_ != NULL;

      case HttpMethod_Put:
        return putHandler_ != NULL;

      case HttpMethod_Delete:
        return deleteHandler_ != NULL;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }


  bool RestApiHierarchy::Resource::IsEmpty() const
  {
    return (getHandler_ == NULL &&
            postHandler_ == NULL &&
            putHandler_ == NULL &&
            deleteHandler_ == NULL);
  }


  RestApiHierarchy& RestApiHierarchy::AddChild(Children& children,
                                               const std::string& name)
  {
    Children::iterator it = children.find(name);

    if (it == children.end())
    {
      // Create new child
      RestApiHierarchy *child = new RestApiHierarchy;
      children[name] = child;
      return *child;
    }
    else
    {
      return *it->second;
    }
  }



  bool RestApiHierarchy::Resource::Handle(RestApiGetCall& call) const
  {
    if (getHandler_ != NULL)
    {
      getHandler_(call);
      return true;
    }
    else
    {
      return false;
    }
  }


  bool RestApiHierarchy::Resource::Handle(RestApiPutCall& call) const
  {
    if (putHandler_ != NULL)
    {
      putHandler_(call);
      return true;
    }
    else
    {
      return false;
    }
  }


  bool RestApiHierarchy::Resource::Handle(RestApiPostCall& call) const
  {
    if (postHandler_ != NULL)
    {
      postHandler_(call);
      return true;
    }
    else
    {
      return false;
    }
  }


  bool RestApiHierarchy::Resource::Handle(RestApiDeleteCall& call) const
  {
    if (deleteHandler_ != NULL)
    {
      deleteHandler_(call);
      return true;
    }
    else
    {
      return false;
    }
  }



  void RestApiHierarchy::DeleteChildren(Children& children)
  {
    for (Children::iterator it = children.begin();
         it != children.end(); ++it)
    {
      delete it->second;
    }
  }


  template <typename Handler>
  void RestApiHierarchy::RegisterInternal(const RestApiPath& path,
                                          Handler handler,
                                          size_t level)
  {
    if (path.GetLevelCount() == level)
    {
      if (path.IsUniversalTrailing())
      {
        universalHandlers_.Register(handler);
      }
      else
      {
        handlers_.Register(handler);
      }
    }
    else
    {
      RestApiHierarchy* child;
      if (path.IsWildcardLevel(level))
      {
        child = &AddChild(wildcardChildren_, path.GetWildcardName(level));
      }
      else
      {
        child = &AddChild(children_, path.GetLevelName(level));
      }

      child->RegisterInternal(path, handler, level + 1);
    }
  }


  bool RestApiHierarchy::LookupResource(IHttpHandler::Arguments& components,
                                       const UriComponents& uri,
                                       IVisitor& visitor,
                                       size_t level)
  {
    if (uri.size() != 0 &&
        level > uri.size())
    {
      return false;
    }

    UriComponents trailing;

    // Look for an exact match on the resource of interest
      if (uri.size() == 0 ||
          level == uri.size())
    {
      if (!handlers_.IsEmpty() &&
          visitor.Visit(handlers_, uri, components, trailing))
      {
        return true;
      }
    }


    if (level < uri.size())  // A recursive call is possible
    {
      // Try and go down in the hierarchy, using an exact match for the child
      Children::const_iterator child = children_.find(uri[level]);
      if (child != children_.end())
      {
        if (child->second->LookupResource(components, uri, visitor, level + 1))
        {
          return true;
        }
      }

      // Try and go down in the hierarchy, using wildcard rules for children
      for (child = wildcardChildren_.begin();
           child != wildcardChildren_.end(); ++child)
      {
        IHttpHandler::Arguments subComponents = components;
        subComponents[child->first] = uri[level];

        if (child->second->LookupResource(subComponents, uri, visitor, level + 1))
        {
          return true;
        }        
      }
    }


    // As a last resort, call the universal handlers, if any
    if (!universalHandlers_.IsEmpty())
    {
      trailing.resize(uri.size() - level);
      size_t pos = 0;
      for (size_t i = level; i < uri.size(); i++, pos++)
      {
        trailing[pos] = uri[i];
      }

      assert(pos == trailing.size());

      if (visitor.Visit(universalHandlers_, uri, components, trailing))
      {
        return true;
      }
    }

    return false;
  }


  bool RestApiHierarchy::CanGenerateDirectory() const
  {
    return (universalHandlers_.IsEmpty() &&
            wildcardChildren_.empty());
  }


  bool RestApiHierarchy::GetDirectory(Json::Value& result,
                                      const UriComponents& uri,
                                      size_t level)
  {
    if (uri.size() == level)
    {
      if (CanGenerateDirectory())
      {
        result = Json::arrayValue;

        for (Children::const_iterator it = children_.begin();
             it != children_.end(); ++it)
        {
          result.append(it->first);
        }

        return true;
      }
      else
      {
        return false;
      }
    }

    Children::const_iterator child = children_.find(uri[level]);
    if (child != children_.end())
    {
      if (child->second->GetDirectory(result, uri, level + 1))
      {
        return true;
      }
    }

    for (child = wildcardChildren_.begin(); 
         child != wildcardChildren_.end(); ++child)
    {
      if (child->second->GetDirectory(result, uri, level + 1))
      {
        return true;
      }
    }

    return false;
  }
                       

  RestApiHierarchy::~RestApiHierarchy()
  {
    DeleteChildren(children_);
    DeleteChildren(wildcardChildren_);
  }

  void RestApiHierarchy::Register(const std::string& uri,
                                  RestApiGetCall::Handler handler)
  {
    RestApiPath path(uri);
    RegisterInternal(path, handler, 0);
  }

  void RestApiHierarchy::Register(const std::string& uri,
                                  RestApiPutCall::Handler handler)
  {
    RestApiPath path(uri);
    RegisterInternal(path, handler, 0);
  }

  void RestApiHierarchy::Register(const std::string& uri,
                                  RestApiPostCall::Handler handler)
  {
    RestApiPath path(uri);
    RegisterInternal(path, handler, 0);
  }

  void RestApiHierarchy::Register(const std::string& uri,
                                  RestApiDeleteCall::Handler handler)
  {
    RestApiPath path(uri);
    RegisterInternal(path, handler, 0);
  }

  void RestApiHierarchy::CreateSiteMap(Json::Value& target) const
  {
    target = Json::objectValue;

    /*std::string s = " ";
      if (handlers_.HasHandler(HttpMethod_Get))
      {
      s += "GET ";
      }

      if (handlers_.HasHandler(HttpMethod_Post))
      {
      s += "POST ";
      }

      if (handlers_.HasHandler(HttpMethod_Put))
      {
      s += "PUT ";
      }

      if (handlers_.HasHandler(HttpMethod_Delete))
      {
      s += "DELETE ";
      }

      target = s;*/
      
    for (Children::const_iterator it = children_.begin();
         it != children_.end(); ++it)
    {
      it->second->CreateSiteMap(target[it->first]);
    }
      
    for (Children::const_iterator it = wildcardChildren_.begin();
         it != wildcardChildren_.end(); ++it)
    {
      it->second->CreateSiteMap(target["<" + it->first + ">"]);
    }
  }


  bool RestApiHierarchy::LookupResource(const UriComponents& uri,
                                        IVisitor& visitor)
  {
    IHttpHandler::Arguments components;
    return LookupResource(components, uri, visitor, 0);
  }    



  namespace
  {
    // Anonymous namespace to avoid clashes between compilation modules

    class AcceptedMethodsVisitor : public RestApiHierarchy::IVisitor
    {
    private:
      std::set<HttpMethod>& methods_;

    public:
      AcceptedMethodsVisitor(std::set<HttpMethod>& methods) : methods_(methods)
      {
      }

      virtual bool Visit(const RestApiHierarchy::Resource& resource,
                         const UriComponents& uri,
                         const IHttpHandler::Arguments& components,
                         const UriComponents& trailing)
      {
        if (trailing.size() == 0)  // Ignore universal handlers
        {
          if (resource.HasHandler(HttpMethod_Get))
          {
            methods_.insert(HttpMethod_Get);
          }

          if (resource.HasHandler(HttpMethod_Post))
          {
            methods_.insert(HttpMethod_Post);
          }

          if (resource.HasHandler(HttpMethod_Put))
          {
            methods_.insert(HttpMethod_Put);
          }

          if (resource.HasHandler(HttpMethod_Delete))
          {
            methods_.insert(HttpMethod_Delete);
          }
        }

        return false;  // Continue to check all the possible ways to access this URI
      }
    };
  }

  void RestApiHierarchy::GetAcceptedMethods(std::set<HttpMethod>& methods,
                                            const UriComponents& uri)
  {
    IHttpHandler::Arguments components;
    AcceptedMethodsVisitor visitor(methods);
    if (LookupResource(components, uri, visitor, 0))
    {
      Json::Value d;
      if (GetDirectory(d, uri))
      {
        methods.insert(HttpMethod_Get);
      }
    }
  }
}
