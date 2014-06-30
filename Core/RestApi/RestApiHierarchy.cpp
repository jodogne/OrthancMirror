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


#include "RestApiHierarchy.h"

#include <cassert>

namespace Orthanc
{
  RestApiHierarchy::Handlers::Handlers() : 
    getHandler_(NULL), 
    postHandler_(NULL),
    putHandler_(NULL), 
    deleteHandler_(NULL)
  {
  }


  bool RestApiHierarchy::Handlers::HasHandler(HttpMethod method) const
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


  bool RestApiHierarchy::Handlers::IsEmpty() const
  {
    return (getHandler_ == NULL &&
            postHandler_ == NULL &&
            putHandler_ == NULL &&
            deleteHandler_ == NULL);
  }


  RestApiGetCall::Handler RestApiHierarchy::Handlers::GetGetHandler() const
  {
    assert(getHandler_ != NULL);
    return getHandler_;
  }

  RestApiPutCall::Handler RestApiHierarchy::Handlers::GetPutHandler() const
  {
    assert(putHandler_ != NULL);
    return putHandler_;
  }

  RestApiPostCall::Handler RestApiHierarchy::Handlers::GetPostHandler() const
  {
    assert(postHandler_ != NULL);
    return postHandler_;
  }

  RestApiDeleteCall::Handler RestApiHierarchy::Handlers::GetDeleteHandler() const
  {
    assert(deleteHandler_ != NULL);
    return deleteHandler_;
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


  void RestApiHierarchy::DeleteChildren(Children& children)
  {
    for (Children::iterator it = children.begin();
         it != children.end(); it++)
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


  bool RestApiHierarchy::LookupHandler(RestApiPath::Components& components,
                                       const UriComponents& uri,
                                       ResourceCallback callback,
                                       size_t level,
                                       void* call)
  {
    assert(uri.size() >= level);
    UriComponents trailing;

    // Look for an exact match on the resource of interest
    if (uri.size() == level)
    {
      if (!handlers_.IsEmpty() &&
          callback(handlers_, uri, components, trailing, call))
      {
        return true;
      }
    }


    // Try and go down in the hierarchy, using an exact match for the child
    Children::const_iterator child = children_.find(uri[level]);
    if (child != children_.end())
    {
      if (child->second->LookupHandler(components, uri, callback, level + 1, call))
      {
        return true;
      }
    }


    // Try and go down in the hierarchy, using wildcard rules for children
    for (child = wildcardChildren_.begin();
         child != wildcardChildren_.end(); child++)
    {
      RestApiPath::Components subComponents = components;
      subComponents[child->first] = uri[level];

      if (child->second->LookupHandler(components, uri, callback, level + 1, call))
      {
        return true;
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

      if (callback(universalHandlers_, uri, components, trailing, call))
      {
        return true;
      }
    }

    return false;
  }


  bool RestApiHierarchy::GetDirectory(Json::Value& result,
                                      const UriComponents& uri,
                                      size_t level)
  {
    if (uri.size() == level)
    {
      if (!handlers_.HasHandler(HttpMethod_Get) && 
          universalHandlers_.IsEmpty() &&
          wildcardChildren_.size() == 0)
      {
        result = Json::arrayValue;

        for (Children::const_iterator it = children_.begin();
             it != children_.end(); it++)
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
         child != wildcardChildren_.end(); child++)
    {
      if (child->second->GetDirectory(result, uri, level + 1))
      {
        return true;
      }
    }

    return false;
  }
                       

  bool RestApiHierarchy::GetCallback(Handlers& handlers,
                                     const UriComponents& uri,
                                     const RestApiPath::Components& components,
                                     const UriComponents& trailing,
                                     void* call)
  {
    if (handlers.HasHandler(HttpMethod_Get))
    {
      handlers.GetGetHandler() (*reinterpret_cast<RestApiGetCall*>(call));
      return true;
    }
    else
    {
      return false;
    }
  }


  bool RestApiHierarchy::PostCallback(Handlers& handlers,
                                      const UriComponents& uri,
                                      const RestApiPath::Components& components,
                                      const UriComponents& trailing,
                                      void* call)
  {
    if (handlers.HasHandler(HttpMethod_Post))
    {
      handlers.GetPostHandler() (*reinterpret_cast<RestApiPostCall*>(call));
      return true;
    }
    else
    {
      return false;
    }
  }


  bool RestApiHierarchy::PutCallback(Handlers& handlers,
                                     const UriComponents& uri,
                                     const RestApiPath::Components& components,
                                     const UriComponents& trailing,
                                     void* call)
  {
    if (handlers.HasHandler(HttpMethod_Put))
    {
      handlers.GetPutHandler() (*reinterpret_cast<RestApiPutCall*>(call));
      return true;
    }
    else
    {
      return false;
    }
  }


  bool RestApiHierarchy::DeleteCallback(Handlers& handlers,
                                        const UriComponents& uri,
                                        const RestApiPath::Components& components,
                                        const UriComponents& trailing,
                                        void* call)
  {
    if (handlers.HasHandler(HttpMethod_Delete))
    {
      handlers.GetDeleteHandler() (*reinterpret_cast<RestApiDeleteCall*>(call));
      return true;
    }
    else
    {
      return false;
    }
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
    if (children_.size() == 0)
    {
      std::string s = " ";
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

      target = s;
    }
    else
    {
      target = Json::objectValue;
      
      for (Children::const_iterator it = children_.begin();
           it != children_.end(); it++)
      {
        it->second->CreateSiteMap(target[it->first]);
      }
    }
      
    /*for (Children::const_iterator it = wildcardChildren_.begin();
      it != wildcardChildren_.end(); it++)
      {
      it->second->CreateSiteMap(target["* (" + it->first + ")"]);
      }*/
  }

  bool RestApiHierarchy::Handle(RestApiGetCall& call,
                                const UriComponents& uri)
  {
    RestApiPath::Components components;
    return LookupHandler(components, uri, GetCallback, 0, &call);
  }    

  bool RestApiHierarchy::Handle(RestApiPutCall& call,
                                const UriComponents& uri)
  {
    RestApiPath::Components components;
    return LookupHandler(components, uri, PutCallback, 0, &call);
  }    

  bool RestApiHierarchy::Handle(RestApiPostCall& call,
                                const UriComponents& uri)
  {
    RestApiPath::Components components;
    return LookupHandler(components, uri, PostCallback, 0, &call);
  }    

  bool RestApiHierarchy::Handle(RestApiDeleteCall& call,
                                const UriComponents& uri)
  {
    RestApiPath::Components components;
    return LookupHandler(components, uri, DeleteCallback, 0, &call);
  }    

}
