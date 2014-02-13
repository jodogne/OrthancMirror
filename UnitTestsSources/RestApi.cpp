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
  }

  {
    RestApiPath uri("/coucou/{abc}/d");
    ASSERT_FALSE(uri.Match(args, trail, "/coucou/moi/d/e/f/g"));
    ASSERT_TRUE(uri.Match(args, trail, "/coucou/moi/d"));
    ASSERT_EQ(1u, args.size());
    ASSERT_EQ(0u, trail.size());
    ASSERT_EQ("moi", args["abc"]);
  }

  {
    RestApiPath uri("/*");
    ASSERT_TRUE(uri.Match(args, trail, "/a/b/c"));
    ASSERT_EQ(0u, args.size());
    ASSERT_EQ(3u, trail.size());
    ASSERT_EQ("a", trail[0]);
    ASSERT_EQ("b", trail[1]);
    ASSERT_EQ("c", trail[2]);
  }
}
