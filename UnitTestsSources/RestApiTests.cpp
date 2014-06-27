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


#include "PrecompiledHeadersUnitTests.h"
#include "gtest/gtest.h"

#include <ctype.h>
#include <glog/logging.h>

#include "../Core/ChunkedBuffer.h"
#include "../Core/HttpClient.h"
#include "../Core/RestApi/RestApi.h"
#include "../Core/Uuid.h"
#include "../Core/OrthancException.h"
#include "../Core/Compression/ZlibCompressor.h"

using namespace Orthanc;

#if !defined(UNIT_TESTS_WITH_HTTP_CONNEXIONS)
#error "Please set UNIT_TESTS_WITH_HTTP_CONNEXIONS"
#endif

TEST(HttpClient, Basic)
{
  HttpClient c;
  ASSERT_FALSE(c.IsVerbose());
  c.SetVerbose(true);
  ASSERT_TRUE(c.IsVerbose());
  c.SetVerbose(false);
  ASSERT_FALSE(c.IsVerbose());

#if UNIT_TESTS_WITH_HTTP_CONNEXIONS == 1
  Json::Value v;
  c.SetUrl("http://orthanc.googlecode.com/hg/Resources/Configuration.json");
  c.Apply(v);
  ASSERT_TRUE(v.isMember("StorageDirectory"));
  //ASSERT_EQ(GetLastStatusText());

  v = Json::nullValue;

  HttpClient cc(c);
  cc.SetUrl("https://orthanc.googlecode.com/hg/Resources/Configuration.json");
  cc.Apply(v);
  ASSERT_TRUE(v.isMember("LuaScripts"));
#endif
}

TEST(RestApi, ChunkedBuffer)
{
  ChunkedBuffer b;
  ASSERT_EQ(0, b.GetNumBytes());

  b.AddChunk("hello", 5);
  ASSERT_EQ(5, b.GetNumBytes());

  b.AddChunk("world", 5);
  ASSERT_EQ(10, b.GetNumBytes());

  std::string s;
  b.Flatten(s);
  ASSERT_EQ("helloworld", s);
}

TEST(RestApi, ParseCookies)
{
  HttpHandler::Arguments headers;
  HttpHandler::Arguments cookies;

  headers["cookie"] = "a=b;c=d;;;e=f;;g=h;";
  HttpHandler::ParseCookies(cookies, headers);
  ASSERT_EQ(4u, cookies.size());
  ASSERT_EQ("b", cookies["a"]);
  ASSERT_EQ("d", cookies["c"]);
  ASSERT_EQ("f", cookies["e"]);
  ASSERT_EQ("h", cookies["g"]);

  headers["cookie"] = "  name =  value  ; name2=value2";
  HttpHandler::ParseCookies(cookies, headers);
  ASSERT_EQ(2u, cookies.size());
  ASSERT_EQ("value", cookies["name"]);
  ASSERT_EQ("value2", cookies["name2"]);

  headers["cookie"] = "  ;;;    ";
  HttpHandler::ParseCookies(cookies, headers);
  ASSERT_EQ(0u, cookies.size());

  headers["cookie"] = "  ;   n=v  ;;    ";
  HttpHandler::ParseCookies(cookies, headers);
  ASSERT_EQ(1u, cookies.size());
  ASSERT_EQ("v", cookies["n"]);
}

TEST(RestApi, RestApiPath)
{
  RestApiPath::Components args;
  UriComponents trail;

  {
    RestApiPath uri("/coucou/{abc}/d/*");
    ASSERT_TRUE(uri.Match(args, trail, "/coucou/moi/d/e/f/g"));
    ASSERT_EQ(1u, args.size());
    ASSERT_EQ(3u, trail.size());
    ASSERT_EQ("moi", args["abc"]);
    ASSERT_EQ("e", trail[0]);
    ASSERT_EQ("f", trail[1]);
    ASSERT_EQ("g", trail[2]);

    ASSERT_FALSE(uri.Match(args, trail, "/coucou/moi/f"));
    ASSERT_TRUE(uri.Match(args, trail, "/coucou/moi/d/"));
    ASSERT_FALSE(uri.Match(args, trail, "/a/moi/d"));
    ASSERT_FALSE(uri.Match(args, trail, "/coucou/moi"));

    ASSERT_EQ(3u, uri.GetLevelCount());
    ASSERT_TRUE(uri.IsUniversalTrailing());

    ASSERT_EQ("coucou", uri.GetLevelName(0));
    ASSERT_THROW(uri.GetWildcardName(0), OrthancException);

    ASSERT_EQ("abc", uri.GetWildcardName(1));
    ASSERT_THROW(uri.GetLevelName(1), OrthancException);

    ASSERT_EQ("d", uri.GetLevelName(2));
    ASSERT_THROW(uri.GetWildcardName(2), OrthancException);
  }

  {
    RestApiPath uri("/coucou/{abc}/d");
    ASSERT_FALSE(uri.Match(args, trail, "/coucou/moi/d/e/f/g"));
    ASSERT_TRUE(uri.Match(args, trail, "/coucou/moi/d"));
    ASSERT_EQ(1u, args.size());
    ASSERT_EQ(0u, trail.size());
    ASSERT_EQ("moi", args["abc"]);

    ASSERT_EQ(3u, uri.GetLevelCount());
    ASSERT_FALSE(uri.IsUniversalTrailing());

    ASSERT_EQ("coucou", uri.GetLevelName(0));
    ASSERT_THROW(uri.GetWildcardName(0), OrthancException);

    ASSERT_EQ("abc", uri.GetWildcardName(1));
    ASSERT_THROW(uri.GetLevelName(1), OrthancException);

    ASSERT_EQ("d", uri.GetLevelName(2));
    ASSERT_THROW(uri.GetWildcardName(2), OrthancException);
  }

  {
    RestApiPath uri("/*");
    ASSERT_TRUE(uri.Match(args, trail, "/a/b/c"));
    ASSERT_EQ(0u, args.size());
    ASSERT_EQ(3u, trail.size());
    ASSERT_EQ("a", trail[0]);
    ASSERT_EQ("b", trail[1]);
    ASSERT_EQ("c", trail[2]);

    ASSERT_EQ(0u, uri.GetLevelCount());
    ASSERT_TRUE(uri.IsUniversalTrailing());
  }
}






namespace Orthanc
{
  class RestApiHierarchy
  {
  private:
    struct Handlers
    {
      typedef std::list<RestApi::GetHandler>  GetHandlers;
      typedef std::list<RestApi::PostHandler>  PostHandlers;
      typedef std::list<RestApi::PutHandler>  PutHandlers;
      typedef std::list<RestApi::DeleteHandler>  DeleteHandlers;

      GetHandlers  getHandlers_;
      PostHandlers  postHandlers_;
      PutHandlers  putHandlers_;
      DeleteHandlers  deleteHandlers_;

      bool HasGet() const
      {
        return getHandlers_.size() > 0;
      }

      void Register(RestApi::GetHandler handler)
      {
        getHandlers_.push_back(handler);
      }

      void Register(RestApi::PutHandler handler)
      {
        putHandlers_.push_back(handler);
      }

      void Register(RestApi::PostHandler handler)
      {
        postHandlers_.push_back(handler);
      }

      void Register(RestApi::DeleteHandler handler)
      {
        deleteHandlers_.push_back(handler);
      }

      bool IsEmpty() const
      {
        return (getHandlers_.empty() &&
                postHandlers_.empty() &&
                putHandlers_.empty() &&
                deleteHandlers_.empty());
      }
    };


    typedef std::map<std::string, RestApiHierarchy*>  Children;
    typedef bool (*ResourceCallback) (Handlers&,
                                      const UriComponents& uri,
                                      const RestApiPath::Components& components,
                                      const UriComponents& trailing,
                                      void* call);

    Handlers  handlers_;
    Children  children_;
    Children  wildcardChildren_;
    Handlers  universalHandlers_;


    static RestApiHierarchy& AddChild(Children& children,
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


    static void DeleteChildren(Children& children)
    {
      for (Children::iterator it = children.begin();
           it != children.end(); it++)
      {
        delete it->second;
      }
    }


    template <typename Handler>
    void RegisterInternal(const RestApiPath& path,
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


    bool LookupHandler(RestApiPath::Components& components,
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


    bool GetDirectory(Json::Value& result,
                      const UriComponents& uri,
                      size_t level)
    {
      if (uri.size() == level)
      {
        if (!handlers_.HasGet() && 
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
                       

    static bool GetCallback(Handlers& handlers,
                            const UriComponents& uri,
                            const RestApiPath::Components& components,
                            const UriComponents& trailing,
                            void* call)
    {
      for (Handlers::GetHandlers::iterator
             it = handlers.getHandlers_.begin(); 
           it != handlers.getHandlers_.end(); it++)
      {
        // TODO RETURN BOOL

        (*it) (*reinterpret_cast<RestApi::GetCall*>(call));
        return true;
      }

      return false;
    }


    static bool PostCallback(Handlers& handlers,
                             const UriComponents& uri,
                             const RestApiPath::Components& components,
                             const UriComponents& trailing,
                             void* call)
    {
      for (Handlers::PostHandlers::iterator
             it = handlers.postHandlers_.begin(); 
           it != handlers.postHandlers_.end(); it++)
      {
        // TODO RETURN BOOL

        (*it) (*reinterpret_cast<RestApi::PostCall*>(call));
        return true;
      }

      return false;
    }


    static bool PutCallback(Handlers& handlers,
                            const UriComponents& uri,
                            const RestApiPath::Components& components,
                            const UriComponents& trailing,
                            void* call)
    {
      for (Handlers::PutHandlers::iterator
             it = handlers.putHandlers_.begin(); 
           it != handlers.putHandlers_.end(); it++)
      {
        // TODO RETURN BOOL

        (*it) (*reinterpret_cast<RestApi::PutCall*>(call));
        return true;
      }

      return false;
    }


    static bool DeleteCallback(Handlers& handlers,
                               const UriComponents& uri,
                               const RestApiPath::Components& components,
                               const UriComponents& trailing,
                               void* call)
    {
      for (Handlers::DeleteHandlers::iterator
             it = handlers.deleteHandlers_.begin(); 
           it != handlers.deleteHandlers_.end(); it++)
      {
        // TODO RETURN BOOL

        (*it) (*reinterpret_cast<RestApi::DeleteCall*>(call));
        return true;
      }

      return false;
    }


  public:
    ~RestApiHierarchy()
    {
      DeleteChildren(children_);
      DeleteChildren(wildcardChildren_);
    }

    void Register(const RestApiPath& path,
                  RestApi::GetHandler handler)
    {
      RegisterInternal(path, handler, 0);
    }

    void Register(const RestApiPath& path,
                  RestApi::PutHandler handler)
    {
      RegisterInternal(path, handler, 0);
    }

    void Register(const RestApiPath& path,
                  RestApi::PostHandler handler)
    {
      RegisterInternal(path, handler, 0);
    }

    void Register(const RestApiPath& path,
                  RestApi::DeleteHandler handler)
    {
      RegisterInternal(path, handler, 0);
    }

    void CreateSiteMap(Json::Value& target) const
    {
      if (children_.size() == 0)
      {
        std::string s = " ";
        if (handlers_.getHandlers_.size() != 0)
        {
          s += "GET ";
        }

        if (handlers_.postHandlers_.size() != 0)
        {
          s += "POST ";
        }

        if (handlers_.putHandlers_.size() != 0)
        {
          s += "PUT ";
        }

        if (handlers_.deleteHandlers_.size() != 0)
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

    bool GetDirectory(Json::Value& result,
                      const UriComponents& uri)
    {
      return GetDirectory(result, uri, 0);
    }

    bool GetDirectory(Json::Value& result,
                      const std::string& uri)
    {
      UriComponents c;
      Toolbox::SplitUriComponents(c, uri);
      return GetDirectory(result, c, 0);
    }

    bool Handle(RestApi::GetCall& call,
                const UriComponents& uri)
    {
      RestApiPath::Components components;
      return LookupHandler(components, uri, GetCallback, 0, &call);
    }    

    bool Handle(RestApi::PutCall& call,
                const UriComponents& uri)
    {
      RestApiPath::Components components;
      return LookupHandler(components, uri, PutCallback, 0, &call);
    }    

    bool Handle(RestApi::PostCall& call,
                const UriComponents& uri)
    {
      RestApiPath::Components components;
      return LookupHandler(components, uri, PostCallback, 0, &call);
    }    

    bool Handle(RestApi::DeleteCall& call,
                const UriComponents& uri)
    {
      RestApiPath::Components components;
      return LookupHandler(components, uri, DeleteCallback, 0, &call);
    }    

    bool Handle(RestApi::GetCall& call,
                const std::string& uri)
    {
      UriComponents c;
      Toolbox::SplitUriComponents(c, uri);
      return Handle(call, c);
    }    
  };

}




static int testValue;

template <int value>
static void SetValue(RestApi::GetCall& get)
{
  testValue = value;
}


TEST(RestApi, RestApiHierarchy)
{
  RestApiHierarchy root;
  root.Register(RestApiPath("/hello/world/test"), SetValue<1>);
  root.Register(RestApiPath("/hello/world/test2"), SetValue<2>);
  root.Register(RestApiPath("/hello/{world}/test3/test4"), SetValue<3>);
  root.Register(RestApiPath("/hello2/*"), SetValue<4>);

  Json::Value m;
  root.CreateSiteMap(m);
  std::cout << m;

  Json::Value d;
  ASSERT_FALSE(root.GetDirectory(d, "/hello"));

  ASSERT_TRUE(root.GetDirectory(d, "/hello/a")); 
  ASSERT_EQ(1u, d.size());
  ASSERT_EQ("test3", d[0].asString());

  ASSERT_TRUE(root.GetDirectory(d, "/hello/world"));
  ASSERT_EQ(2u, d.size());

  ASSERT_TRUE(root.GetDirectory(d, "/hello/a/test3"));
  ASSERT_EQ(1u, d.size());
  ASSERT_EQ("test4", d[0].asString());

  ASSERT_FALSE(root.GetDirectory(d, "/hello/world/test"));
  ASSERT_FALSE(root.GetDirectory(d, "/hello/world/test2"));
  ASSERT_FALSE(root.GetDirectory(d, "/hello2"));

  testValue = 0;
  ASSERT_TRUE(root.Handle(*reinterpret_cast<RestApi::GetCall*>(NULL), "/hello/world/test"));
  ASSERT_EQ(testValue, 1);
  ASSERT_TRUE(root.Handle(*reinterpret_cast<RestApi::GetCall*>(NULL), "/hello/world/test2"));
  ASSERT_EQ(testValue, 2);
  ASSERT_TRUE(root.Handle(*reinterpret_cast<RestApi::GetCall*>(NULL), "/hello/b/test3/test4"));
  ASSERT_EQ(testValue, 3);
  ASSERT_FALSE(root.Handle(*reinterpret_cast<RestApi::GetCall*>(NULL), "/hello/b/test3/test"));
  ASSERT_EQ(testValue, 3);
  ASSERT_TRUE(root.Handle(*reinterpret_cast<RestApi::GetCall*>(NULL), "/hello2/a/b"));
  ASSERT_EQ(testValue, 4);
}
