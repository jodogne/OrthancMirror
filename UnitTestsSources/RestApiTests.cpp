/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
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
#include "../Core/HttpServer/MultipartStreamReader.h"


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
                       const UriComponents& trailing) ORTHANC_OVERRIDE
    {
      return resource.Handle(*(RestApiGetCall*) NULL);
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
  class AcceptHandler : public HttpContentNegociation::IHandler
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
                        const std::string& subtype) ORTHANC_OVERRIDE
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
    HttpContentNegociation d;
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
    HttpContentNegociation d;
    d.Register("text/plain", h);
    d.Register("text/html", h);
    d.Register("text/x-dvi", h);
    ASSERT_TRUE(d.Apply(T1));
    ASSERT_EQ("text", h.GetType());
    ASSERT_EQ("html", h.GetSubType());
  }
  
  {
    HttpContentNegociation d;
    d.Register("text/plain", h);
    d.Register("text/x-dvi", h);
    d.Register("text/x-c", h);
    ASSERT_TRUE(d.Apply(T1));
    ASSERT_EQ("text", h.GetType());
    ASSERT_EQ("x-c", h.GetSubType());
  }
  
  {
    HttpContentNegociation d;
    d.Register("text/plain", h);
    d.Register("text/x-dvi", h);
    d.Register("text/x-c", h);
    d.Register("text/html", h);
    ASSERT_TRUE(d.Apply(T1));
    ASSERT_EQ("text", h.GetType());
    ASSERT_TRUE(h.GetSubType() == "x-c" || h.GetSubType() == "html");
  }
  
  {
    HttpContentNegociation d;
    d.Register("text/plain", h);
    d.Register("text/x-dvi", h);
    ASSERT_TRUE(d.Apply(T1));
    ASSERT_EQ("text", h.GetType());
    ASSERT_EQ("x-dvi", h.GetSubType());
  }
  
  {
    HttpContentNegociation d;
    d.Register("text/plain", h);
    ASSERT_TRUE(d.Apply(T1));
    ASSERT_EQ("text", h.GetType());
    ASSERT_EQ("plain", h.GetSubType());
  }
}


TEST(WebServiceParameters, Serialization)
{
  {
    Json::Value v = Json::arrayValue;
    v.append("http://localhost:8042/");

    WebServiceParameters p(v);
    ASSERT_FALSE(p.IsAdvancedFormatNeeded());

    Json::Value v2;
    p.Serialize(v2, false, true);
    ASSERT_EQ(v, v2);

    WebServiceParameters p2(v2);
    ASSERT_EQ("http://localhost:8042/", p2.GetUrl());
    ASSERT_TRUE(p2.GetUsername().empty());
    ASSERT_TRUE(p2.GetPassword().empty());
    ASSERT_TRUE(p2.GetCertificateFile().empty());
    ASSERT_TRUE(p2.GetCertificateKeyFile().empty());
    ASSERT_TRUE(p2.GetCertificateKeyPassword().empty());
    ASSERT_FALSE(p2.IsPkcs11Enabled());
  }

  {
    Json::Value v = Json::arrayValue;
    v.append("http://localhost:8042/");
    v.append("user");
    v.append("pass");

    WebServiceParameters p(v);
    ASSERT_FALSE(p.IsAdvancedFormatNeeded());
    ASSERT_EQ("http://localhost:8042/", p.GetUrl());
    ASSERT_EQ("user", p.GetUsername());
    ASSERT_EQ("pass", p.GetPassword());
    ASSERT_TRUE(p.GetCertificateFile().empty());
    ASSERT_TRUE(p.GetCertificateKeyFile().empty());
    ASSERT_TRUE(p.GetCertificateKeyPassword().empty());
    ASSERT_FALSE(p.IsPkcs11Enabled());

    Json::Value v2;
    p.Serialize(v2, false, true);
    ASSERT_EQ(v, v2);

    p.Serialize(v2, false, false /* no password */);
    WebServiceParameters p2(v2);
    ASSERT_EQ(Json::arrayValue, v2.type());
    ASSERT_EQ(3u, v2.size());
    ASSERT_EQ("http://localhost:8042/", v2[0u].asString());
    ASSERT_EQ("user", v2[1u].asString());
    ASSERT_TRUE(v2[2u].asString().empty());
  }

  {
    Json::Value v = Json::arrayValue;
    v.append("http://localhost:8042/");

    WebServiceParameters p(v);
    ASSERT_FALSE(p.IsAdvancedFormatNeeded());
    p.SetPkcs11Enabled(true);
    ASSERT_TRUE(p.IsAdvancedFormatNeeded());

    Json::Value v2;
    p.Serialize(v2, false, true);
    WebServiceParameters p2(v2);

    ASSERT_EQ(Json::objectValue, v2.type());
    ASSERT_EQ(3u, v2.size());
    ASSERT_EQ("http://localhost:8042/", v2["Url"].asString());
    ASSERT_TRUE(v2["Pkcs11"].asBool());
    ASSERT_EQ(Json::objectValue, v2["HttpHeaders"].type());
    ASSERT_EQ(0u, v2["HttpHeaders"].size());
  }

  {
    Json::Value v = Json::arrayValue;
    v.append("http://localhost:8042/");

    WebServiceParameters p(v);
    ASSERT_FALSE(p.IsAdvancedFormatNeeded());
    p.SetClientCertificate("a", "b", "c");
    ASSERT_TRUE(p.IsAdvancedFormatNeeded());

    Json::Value v2;
    p.Serialize(v2, false, true);
    WebServiceParameters p2(v2);

    ASSERT_EQ(Json::objectValue, v2.type());
    ASSERT_EQ(6u, v2.size());
    ASSERT_EQ("http://localhost:8042/", v2["Url"].asString());
    ASSERT_EQ("a", v2["CertificateFile"].asString());
    ASSERT_EQ("b", v2["CertificateKeyFile"].asString());
    ASSERT_EQ("c", v2["CertificateKeyPassword"].asString());
    ASSERT_FALSE(v2["Pkcs11"].asBool());
    ASSERT_EQ(Json::objectValue, v2["HttpHeaders"].type());
    ASSERT_EQ(0u, v2["HttpHeaders"].size());
  }

  {
    Json::Value v = Json::arrayValue;
    v.append("http://localhost:8042/");

    WebServiceParameters p(v);
    ASSERT_FALSE(p.IsAdvancedFormatNeeded());
    p.AddHttpHeader("a", "b");
    p.AddHttpHeader("c", "d");
    ASSERT_TRUE(p.IsAdvancedFormatNeeded());

    Json::Value v2;
    p.Serialize(v2, false, true);
    WebServiceParameters p2(v2);

    ASSERT_EQ(Json::objectValue, v2.type());
    ASSERT_EQ(3u, v2.size());
    ASSERT_EQ("http://localhost:8042/", v2["Url"].asString());
    ASSERT_FALSE(v2["Pkcs11"].asBool());
    ASSERT_EQ(Json::objectValue, v2["HttpHeaders"].type());
    ASSERT_EQ(2u, v2["HttpHeaders"].size());
    ASSERT_EQ("b", v2["HttpHeaders"]["a"].asString());
    ASSERT_EQ("d", v2["HttpHeaders"]["c"].asString());

    std::set<std::string> a;
    p2.ListHttpHeaders(a);
    ASSERT_EQ(2u, a.size());
    ASSERT_TRUE(a.find("a") != a.end());
    ASSERT_TRUE(a.find("c") != a.end());

    std::string s;
    ASSERT_TRUE(p2.LookupHttpHeader(s, "a")); ASSERT_EQ("b", s);
    ASSERT_TRUE(p2.LookupHttpHeader(s, "c")); ASSERT_EQ("d", s);
    ASSERT_FALSE(p2.LookupHttpHeader(s, "nope"));
  }
}


TEST(WebServiceParameters, UserProperties)
{
  Json::Value v = Json::nullValue;

  {
    WebServiceParameters p;
    p.SetUrl("http://localhost:8042/");
    ASSERT_FALSE(p.IsAdvancedFormatNeeded());

    ASSERT_THROW(p.AddUserProperty("Url", "nope"), OrthancException);
    p.AddUserProperty("Hello", "world");
    p.AddUserProperty("a", "b");
    ASSERT_TRUE(p.IsAdvancedFormatNeeded());

    p.Serialize(v, false, true);

    p.ClearUserProperties();
    ASSERT_FALSE(p.IsAdvancedFormatNeeded());
  }

  {
    WebServiceParameters p(v);
    ASSERT_TRUE(p.IsAdvancedFormatNeeded());
    ASSERT_TRUE(p.GetHttpHeaders().empty());

    std::set<std::string> tmp;
    p.ListUserProperties(tmp);
    ASSERT_EQ(2u, tmp.size());
    ASSERT_TRUE(tmp.find("a")     != tmp.end());
    ASSERT_TRUE(tmp.find("Hello") != tmp.end());
    ASSERT_TRUE(tmp.find("hello") == tmp.end());

    std::string s;
    ASSERT_TRUE(p.LookupUserProperty(s, "a"));      ASSERT_TRUE(s == "b");
    ASSERT_TRUE(p.LookupUserProperty(s, "Hello"));  ASSERT_TRUE(s == "world");
    ASSERT_FALSE(p.LookupUserProperty(s, "hello"));
  }
}


TEST(StringMatcher, Basic)
{
  StringMatcher matcher("---");

  ASSERT_THROW(matcher.GetMatchBegin(), OrthancException);

  {
    const std::string s = "";
    ASSERT_FALSE(matcher.Apply(s));
  }

  {
    const std::string s = "abc----def";
    ASSERT_TRUE(matcher.Apply(s));
    ASSERT_EQ(3, std::distance(s.begin(), matcher.GetMatchBegin()));
    ASSERT_EQ("---", std::string(matcher.GetMatchBegin(), matcher.GetMatchEnd()));
  }

  {
    const std::string s = "abc---";
    ASSERT_TRUE(matcher.Apply(s));
    ASSERT_EQ(3, std::distance(s.begin(), matcher.GetMatchBegin()));
    ASSERT_EQ(s.end(), matcher.GetMatchEnd());
    ASSERT_EQ("---", std::string(matcher.GetMatchBegin(), matcher.GetMatchEnd()));
    ASSERT_EQ("", std::string(matcher.GetMatchEnd(), s.end()));
  }

  {
    const std::string s = "abc--def";
    ASSERT_FALSE(matcher.Apply(s));
    ASSERT_THROW(matcher.GetMatchBegin(), OrthancException);
    ASSERT_THROW(matcher.GetMatchEnd(), OrthancException);
  }

  {
    std::string s(10u, '\0');  // String with null values
    ASSERT_EQ(10u, s.size());
    ASSERT_EQ(10u, s.size());
    ASSERT_FALSE(matcher.Apply(s));

    s[9] = '-';
    ASSERT_FALSE(matcher.Apply(s));

    s[8] = '-';
    ASSERT_FALSE(matcher.Apply(s));

    s[7] = '-';
    ASSERT_TRUE(matcher.Apply(s));
    ASSERT_EQ(s.c_str() + 7, matcher.GetPointerBegin());
    ASSERT_EQ(s.c_str() + 10, matcher.GetPointerEnd());
    ASSERT_EQ(s.end() - 3, matcher.GetMatchBegin());
    ASSERT_EQ(s.end(), matcher.GetMatchEnd());
  }
}



class MultipartTester : public MultipartStreamReader::IHandler
{
private:
  struct Part
  {
    MultipartStreamReader::HttpHeaders   headers_;
    std::string  data_;

    Part(const MultipartStreamReader::HttpHeaders& headers,
         const void* part,
         size_t size) :
      headers_(headers),
      data_(reinterpret_cast<const char*>(part), size)
    {
    }
  };

  std::vector<Part> parts_;

public:
  virtual void HandlePart(const MultipartStreamReader::HttpHeaders& headers,
                          const void* part,
                          size_t size)
  {
    parts_.push_back(Part(headers, part, size));
  }

  unsigned int GetCount() const
  {
    return parts_.size();
  }

  MultipartStreamReader::HttpHeaders& GetHeaders(size_t i)
  {
    return parts_[i].headers_;
  }

  const std::string& GetData(size_t i) const
  {
    return parts_[i].data_;
  }
};


TEST(MultipartStreamReader, ParseHeaders)
{
  std::string ct, b, st, header;

  {
    MultipartStreamReader::HttpHeaders h;
    h["hello"] = "world";
    h["Content-Type"] = "world";  // Should be in lower-case
    h["CONTENT-type"] = "world";  // Should be in lower-case
    ASSERT_FALSE(MultipartStreamReader::GetMainContentType(header, h));
  }

  {
    MultipartStreamReader::HttpHeaders h;
    h["content-type"] = "world";
    ASSERT_TRUE(MultipartStreamReader::GetMainContentType(header, h)); 
    ASSERT_EQ(header, "world");
    ASSERT_FALSE(MultipartStreamReader::ParseMultipartContentType(ct, st, b, header));
  }

  {
    MultipartStreamReader::HttpHeaders h;
    h["content-type"] = "multipart/related; dummy=value; boundary=1234; hello=world";
    ASSERT_TRUE(MultipartStreamReader::GetMainContentType(header, h)); 
    ASSERT_EQ(header, h["content-type"]);
    ASSERT_TRUE(MultipartStreamReader::ParseMultipartContentType(ct, st, b, header));
    ASSERT_EQ(ct, "multipart/related");
    ASSERT_EQ(b, "1234");
    ASSERT_TRUE(st.empty());
  }

  {
    ASSERT_FALSE(MultipartStreamReader::ParseMultipartContentType
                 (ct, st, b, "multipart/related; boundary="));  // Empty boundary
  }

  {
    ASSERT_TRUE(MultipartStreamReader::ParseMultipartContentType
                (ct, st, b, "Multipart/Related; TYPE=Application/Dicom; Boundary=heLLO"));
    ASSERT_EQ(ct, "multipart/related");
    ASSERT_EQ(b, "heLLO");
    ASSERT_EQ(st, "application/dicom");
  }

  {
    ASSERT_TRUE(MultipartStreamReader::ParseMultipartContentType
                (ct, st, b, "Multipart/Related; type=\"application/DICOM\"; Boundary=a"));
    ASSERT_EQ(ct, "multipart/related");
    ASSERT_EQ(b, "a");
    ASSERT_EQ(st, "application/dicom");
  }
}


TEST(MultipartStreamReader, BytePerByte)
{
  std::string stream = "GARBAGE";

  std::string boundary = "123456789123456789";

  {
    for (size_t i = 0; i < 10; i++)
    {
      std::string f = "hello " + boost::lexical_cast<std::string>(i);
    
      stream += "\r\n--" + boundary + "\r\n";
      if (i % 2 == 0)
        stream += "Content-Length: " + boost::lexical_cast<std::string>(f.size()) + "\r\n";
      stream += "Content-Type: toto " + boost::lexical_cast<std::string>(i) + "\r\n\r\n";
      stream += f;
    }
  
    stream += "\r\n--" + boundary + "--";
    stream += "GARBAGE";
  }

  for (unsigned int k = 0; k < 2; k++)
  {
    MultipartTester decoded;

    MultipartStreamReader reader(boundary);
    reader.SetBlockSize(1);
    reader.SetHandler(decoded);

    if (k == 0)
    {
      for (size_t i = 0; i < stream.size(); i++)
      {
        reader.AddChunk(&stream[i], 1);
      }
    }
    else
    {
      reader.AddChunk(stream);
    }

    reader.CloseStream();

    ASSERT_EQ(10u, decoded.GetCount());

    for (size_t i = 0; i < 10; i++)
    {
      ASSERT_EQ("hello " + boost::lexical_cast<std::string>(i), decoded.GetData(i));
      ASSERT_EQ("toto " + boost::lexical_cast<std::string>(i), decoded.GetHeaders(i)["content-type"]);

      if (i % 2 == 0)
      {
        ASSERT_EQ(2u, decoded.GetHeaders(i).size());
        ASSERT_TRUE(decoded.GetHeaders(i).find("content-length") != decoded.GetHeaders(i).end());
      }
    }
  }
}
