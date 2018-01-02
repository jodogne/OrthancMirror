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


#include "PrecompiledHeadersUnitTests.h"
#include "gtest/gtest.h"

#include <ctype.h>
#include <boost/lexical_cast.hpp>
#include <algorithm>

#include "../Core/ChunkedBuffer.h"
#include "../Core/HttpClient.h"
#include "../Core/Logging.h"
#include "../Core/SystemToolbox.h"
#include "../Core/RestApi/RestApi.h"
#include "../Core/OrthancException.h"
#include "../Core/Compression/ZlibCompressor.h"
#include "../Core/RestApi/RestApiHierarchy.h"
#include "../Core/HttpServer/HttpContentNegociation.h"

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
  // The "http://www.orthanc-server.com/downloads/third-party/" does
  // not automatically redirect to HTTPS, so we cas use it even if the
  // OpenSSL/HTTPS support is disabled in curl
  const std::string BASE = "http://www.orthanc-server.com/downloads/third-party/";

  Json::Value v;
  c.SetUrl(BASE + "Product.json");

  c.Apply(v);
  ASSERT_TRUE(v.type() == Json::objectValue);
  ASSERT_TRUE(v.isMember("Description"));
#endif
}


#if UNIT_TESTS_WITH_HTTP_CONNEXIONS == 1 && ORTHANC_ENABLE_SSL == 1

/**
   The HTTPS CA certificates for BitBucket were extracted as follows:
   
   (1) We retrieve the certification chain of BitBucket:

   # echo | openssl s_client -showcerts -connect www.bitbucket.org:443

   (2) We see that the certification authority (CA) is
   "www.digicert.com", and the root certificate is "DigiCert High
   Assurance EV Root CA". As a consequence, we navigate to DigiCert to
   find the URL to this CA certificate:

   firefox https://www.digicert.com/digicert-root-certificates.htm

   (3) Once we get the URL to the CA certificate, we convert it to a C
   macro that can be used by libcurl:

   # cd UnitTestsSources
   # ../Resources/RetrieveCACertificates.py BITBUCKET_CERTIFICATES https://www.digicert.com/CACerts/DigiCertHighAssuranceEVRootCA.crt > BitbucketCACertificates.h
**/

#include "BitbucketCACertificates.h"

TEST(HttpClient, Ssl)
{
  SystemToolbox::WriteFile(BITBUCKET_CERTIFICATES, "UnitTestsResults/bitbucket.cert");

  /*{
    std::string s;
    SystemToolbox::ReadFile(s, "/usr/share/ca-certificates/mozilla/WoSign.crt");
    SystemToolbox::WriteFile(s, "UnitTestsResults/bitbucket.cert");
    }*/

  HttpClient c;
  c.SetHttpsVerifyPeers(true);
  c.SetHttpsCACertificates("UnitTestsResults/bitbucket.cert");
  c.SetUrl("https://bitbucket.org/sjodogne/orthanc/raw/Orthanc-0.9.3/Resources/Configuration.json");

  Json::Value v;
  c.Apply(v);
  ASSERT_TRUE(v.isMember("LuaScripts"));
}

TEST(HttpClient, SslNoVerification)
{
  HttpClient c;
  c.SetHttpsVerifyPeers(false);
  c.SetUrl("https://bitbucket.org/sjodogne/orthanc/raw/Orthanc-0.9.3/Resources/Configuration.json");

  Json::Value v;
  c.Apply(v);
  ASSERT_TRUE(v.isMember("LuaScripts"));
}

#endif


TEST(RestApi, ChunkedBuffer)
{
  ChunkedBuffer b;
  ASSERT_EQ(0u, b.GetNumBytes());

  b.AddChunk("hello", 5);
  ASSERT_EQ(5u, b.GetNumBytes());

  b.AddChunk("world", 5);
  ASSERT_EQ(10u, b.GetNumBytes());

  std::string s;
  b.Flatten(s);
  ASSERT_EQ("helloworld", s);
}

TEST(RestApi, ParseCookies)
{
  IHttpHandler::Arguments headers;
  IHttpHandler::Arguments cookies;

  headers["cookie"] = "a=b;c=d;;;e=f;;g=h;";
  HttpToolbox::ParseCookies(cookies, headers);
  ASSERT_EQ(4u, cookies.size());
  ASSERT_EQ("b", cookies["a"]);
  ASSERT_EQ("d", cookies["c"]);
  ASSERT_EQ("f", cookies["e"]);
  ASSERT_EQ("h", cookies["g"]);

  headers["cookie"] = "  name =  value  ; name2=value2";
  HttpToolbox::ParseCookies(cookies, headers);
  ASSERT_EQ(2u, cookies.size());
  ASSERT_EQ("value", cookies["name"]);
  ASSERT_EQ("value2", cookies["name2"]);

  headers["cookie"] = "  ;;;    ";
  HttpToolbox::ParseCookies(cookies, headers);
  ASSERT_EQ(0u, cookies.size());

  headers["cookie"] = "  ;   n=v  ;;    ";
  HttpToolbox::ParseCookies(cookies, headers);
  ASSERT_EQ(1u, cookies.size());
  ASSERT_EQ("v", cookies["n"]);
}

TEST(RestApi, RestApiPath)
{
  IHttpHandler::Arguments args;
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






static int testValue;

template <int value>
static void SetValue(RestApiGetCall& get)
{
  testValue = value;
}


static bool GetDirectory(Json::Value& target,
                         RestApiHierarchy& hierarchy, 
                         const std::string& uri)
{
  UriComponents p;
  Toolbox::SplitUriComponents(p, uri);
  return hierarchy.GetDirectory(target, p);
}



namespace
{
  class MyVisitor : public RestApiHierarchy::IVisitor
  {
  public:
    virtual bool Visit(const RestApiHierarchy::Resource& resource,
                       const UriComponents& uri,
                       const IHttpHandler::Arguments& components,
                       const UriComponents& trailing)
    {
      return resource.Handle(*reinterpret_cast<RestApiGetCall*>(NULL));
    }
  };
}


static bool HandleGet(RestApiHierarchy& hierarchy, 
                      const std::string& uri)
{
  UriComponents p;
  Toolbox::SplitUriComponents(p, uri);
  MyVisitor visitor;
  return hierarchy.LookupResource(p, visitor);
}


TEST(RestApi, RestApiHierarchy)
{
  RestApiHierarchy root;
  root.Register("/hello/world/test", SetValue<1>);
  root.Register("/hello/world/test2", SetValue<2>);
  root.Register("/hello/{world}/test3/test4", SetValue<3>);
  root.Register("/hello2/*", SetValue<4>);

  Json::Value m;
  root.CreateSiteMap(m);
  std::cout << m;

  Json::Value d;
  ASSERT_FALSE(GetDirectory(d, root, "/hello"));

  ASSERT_TRUE(GetDirectory(d, root, "/hello/a")); 
  ASSERT_EQ(1u, d.size());
  ASSERT_EQ("test3", d[0].asString());

  ASSERT_TRUE(GetDirectory(d, root, "/hello/world"));
  ASSERT_EQ(2u, d.size());

  ASSERT_TRUE(GetDirectory(d, root, "/hello/a/test3"));
  ASSERT_EQ(1u, d.size());
  ASSERT_EQ("test4", d[0].asString());

  ASSERT_TRUE(GetDirectory(d, root, "/hello/world/test"));
  ASSERT_TRUE(GetDirectory(d, root, "/hello/world/test2"));
  ASSERT_FALSE(GetDirectory(d, root, "/hello2"));

  testValue = 0;
  ASSERT_TRUE(HandleGet(root, "/hello/world/test"));
  ASSERT_EQ(testValue, 1);
  ASSERT_TRUE(HandleGet(root, "/hello/world/test2"));
  ASSERT_EQ(testValue, 2);
  ASSERT_TRUE(HandleGet(root, "/hello/b/test3/test4"));
  ASSERT_EQ(testValue, 3);
  ASSERT_FALSE(HandleGet(root, "/hello/b/test3/test"));
  ASSERT_EQ(testValue, 3);
  ASSERT_TRUE(HandleGet(root, "/hello2/a/b"));
  ASSERT_EQ(testValue, 4);
}





namespace
{
  class AcceptHandler : public Orthanc::HttpContentNegociation::IHandler
  {
  private:
    std::string type_;
    std::string subtype_;

  public:
    AcceptHandler()
    {
      Reset();
    }

    void Reset()
    {
      Handle("nope", "nope");
    }

    const std::string& GetType() const
    {
      return type_;
    }

    const std::string& GetSubType() const
    {
      return subtype_;
    }

    virtual void Handle(const std::string& type,
                        const std::string& subtype)
    {
      type_ = type;
      subtype_ = subtype;
    }
  };
}


TEST(RestApi, HttpContentNegociation)
{
  // Reference: http://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html#sec14.1

  AcceptHandler h;

  {
    Orthanc::HttpContentNegociation d;
    d.Register("audio/mp3", h);
    d.Register("audio/basic", h);

    ASSERT_TRUE(d.Apply("audio/*; q=0.2, audio/basic"));
    ASSERT_EQ("audio", h.GetType());
    ASSERT_EQ("basic", h.GetSubType());

    ASSERT_TRUE(d.Apply("audio/*; q=0.2, audio/nope"));
    ASSERT_EQ("audio", h.GetType());
    ASSERT_EQ("mp3", h.GetSubType());
    
    ASSERT_FALSE(d.Apply("application/*; q=0.2, application/pdf"));
    
    ASSERT_TRUE(d.Apply("*/*; application/*; q=0.2, application/pdf"));
    ASSERT_EQ("audio", h.GetType());
  }

  // "This would be interpreted as "text/html and text/x-c are the
  // preferred media types, but if they do not exist, then send the
  // text/x-dvi entity, and if that does not exist, send the
  // text/plain entity.""
  const std::string T1 = "text/plain; q=0.5, text/html, text/x-dvi; q=0.8, text/x-c";
  
  {
    Orthanc::HttpContentNegociation d;
    d.Register("text/plain", h);
    d.Register("text/html", h);
    d.Register("text/x-dvi", h);
    ASSERT_TRUE(d.Apply(T1));
    ASSERT_EQ("text", h.GetType());
    ASSERT_EQ("html", h.GetSubType());
  }
  
  {
    Orthanc::HttpContentNegociation d;
    d.Register("text/plain", h);
    d.Register("text/x-dvi", h);
    d.Register("text/x-c", h);
    ASSERT_TRUE(d.Apply(T1));
    ASSERT_EQ("text", h.GetType());
    ASSERT_EQ("x-c", h.GetSubType());
  }
  
  {
    Orthanc::HttpContentNegociation d;
    d.Register("text/plain", h);
    d.Register("text/x-dvi", h);
    d.Register("text/x-c", h);
    d.Register("text/html", h);
    ASSERT_TRUE(d.Apply(T1));
    ASSERT_EQ("text", h.GetType());
    ASSERT_TRUE(h.GetSubType() == "x-c" || h.GetSubType() == "html");
  }
  
  {
    Orthanc::HttpContentNegociation d;
    d.Register("text/plain", h);
    d.Register("text/x-dvi", h);
    ASSERT_TRUE(d.Apply(T1));
    ASSERT_EQ("text", h.GetType());
    ASSERT_EQ("x-dvi", h.GetSubType());
  }
  
  {
    Orthanc::HttpContentNegociation d;
    d.Register("text/plain", h);
    ASSERT_TRUE(d.Apply(T1));
    ASSERT_EQ("text", h.GetType());
    ASSERT_EQ("plain", h.GetSubType());
  }
}
