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

#include "../Core/OrthancException.h"
#include "../Core/Toolbox.h"
#include "../Core/Lua/LuaFunctionCall.h"

#include <boost/lexical_cast.hpp>

#if !defined(UNIT_TESTS_WITH_HTTP_CONNEXIONS)
#error "Please set UNIT_TESTS_WITH_HTTP_CONNEXIONS to 0 or 1"
#endif


TEST(Lua, Json)
{
  Orthanc::LuaContext lua;
  lua.Execute(Orthanc::EmbeddedResources::LUA_TOOLBOX);
  lua.Execute("a={}");
  lua.Execute("a['x'] = 10");
  lua.Execute("a['y'] = {}");
  lua.Execute("a['y'][1] = 20");
  lua.Execute("a['y'][2] = 20");
  lua.Execute("PrintRecursive(a)");

  lua.Execute("function f(a) print(a.bool) return a.bool,20,30,40,50,60 end");

  Json::Value v, vv, o;
  //v["a"] = "b";
  v.append("hello");
  v.append("world");
  v.append("42");
  vv.append("sub");
  vv.append("set");
  v.append(vv);
  o = Json::objectValue;
  o["x"] = 10;
  o["y"] = 20;
  o["z"] = 20.5f;
  v.append(o);

  {
    Orthanc::LuaFunctionCall f(lua, "PrintRecursive");
    f.PushJson(v);
    f.Execute();
  }

  {
    Orthanc::LuaFunctionCall f(lua, "f");
    f.PushJson(o);
    ASSERT_THROW(f.ExecutePredicate(), Orthanc::OrthancException);
  }

  o["bool"] = false;

  {
    Orthanc::LuaFunctionCall f(lua, "f");
    f.PushJson(o);
    ASSERT_FALSE(f.ExecutePredicate());
  }

  o["bool"] = true;

  {
    Orthanc::LuaFunctionCall f(lua, "f");
    f.PushJson(o);
    ASSERT_TRUE(f.ExecutePredicate());
  }
}


TEST(Lua, Existing)
{
  Orthanc::LuaContext lua;
  lua.Execute("a={}");
  lua.Execute("function f() end");

  ASSERT_TRUE(lua.IsExistingFunction("f"));
  ASSERT_FALSE(lua.IsExistingFunction("a"));
  ASSERT_FALSE(lua.IsExistingFunction("Dummy"));
}


TEST(Lua, Simple)
{
  Orthanc::LuaContext lua;
  lua.Execute(Orthanc::EmbeddedResources::LUA_TOOLBOX);

  {
    Orthanc::LuaFunctionCall f(lua, "PrintRecursive");
    f.PushString("hello");
    f.Execute();
  }

  {
    Orthanc::LuaFunctionCall f(lua, "PrintRecursive");
    f.PushBoolean(true);
    f.Execute();
  }

  {
    Orthanc::LuaFunctionCall f(lua, "PrintRecursive");
    f.PushInteger(42);
    f.Execute();
  }

  {
    Orthanc::LuaFunctionCall f(lua, "PrintRecursive");
    f.PushDouble(3.1415);
    f.Execute();
  }
}


TEST(Lua, ReturnJson)
{
  Json::Value b = Json::objectValue;
  b["a"] = 42;
  b["b"] = 44.37;
  b["c"] = -43;

  Json::Value c = Json::arrayValue;
  c.append("test3");
  c.append("test1");
  c.append("test2");

  Json::Value a = Json::objectValue;
  a["Hello"] = "World";
  a["List"] = Json::arrayValue;
  a["List"].append(b);
  a["List"].append(c);

  Orthanc::LuaContext lua;

  // This is the identity function (it simply returns its input)
  lua.Execute("function identity(a) return a end");

  {
    Orthanc::LuaFunctionCall f(lua, "identity");
    f.PushJson("hello");
    Json::Value v;
    f.ExecuteToJson(v, false);
    ASSERT_EQ("hello", v.asString());
  }

  {
    Orthanc::LuaFunctionCall f(lua, "identity");
    f.PushJson(42.25);
    Json::Value v;
    f.ExecuteToJson(v, false);
    ASSERT_FLOAT_EQ(42.25f, v.asFloat());
  }

  {
    Orthanc::LuaFunctionCall f(lua, "identity");
    f.PushJson(-42);
    Json::Value v;
    f.ExecuteToJson(v, false);
    ASSERT_EQ(-42, v.asInt());
  }

  {
    Orthanc::LuaFunctionCall f(lua, "identity");
    Json::Value vv = Json::arrayValue;
    f.PushJson(vv);
    Json::Value v;
    f.ExecuteToJson(v, false);
    ASSERT_EQ(Json::arrayValue, v.type());
  }

  {
    Orthanc::LuaFunctionCall f(lua, "identity");
    Json::Value vv = Json::objectValue;
    f.PushJson(vv);
    Json::Value v;
    f.ExecuteToJson(v, false);
    // Lua does not make the distinction between empty lists and empty objects
    ASSERT_EQ(Json::arrayValue, v.type());
  }

  {
    Orthanc::LuaFunctionCall f(lua, "identity");
    f.PushJson(b);
    Json::Value v;
    f.ExecuteToJson(v, false);
    ASSERT_EQ(Json::objectValue, v.type());
    ASSERT_FLOAT_EQ(42.0f, v["a"].asFloat());
    ASSERT_FLOAT_EQ(44.37f, v["b"].asFloat());
    ASSERT_FLOAT_EQ(-43.0f, v["c"].asFloat());
  }

  {
    Orthanc::LuaFunctionCall f(lua, "identity");
    f.PushJson(c);
    Json::Value v;
    f.ExecuteToJson(v, false);
    ASSERT_EQ(Json::arrayValue, v.type());
    ASSERT_EQ("test3", v[0].asString());
    ASSERT_EQ("test1", v[1].asString());
    ASSERT_EQ("test2", v[2].asString());
  }

  {
    Orthanc::LuaFunctionCall f(lua, "identity");
    f.PushJson(a);
    Json::Value v;
    f.ExecuteToJson(v, false);
    ASSERT_EQ("World", v["Hello"].asString());
    ASSERT_EQ(Json::intValue, v["List"][0]["a"].type());
    ASSERT_EQ(Json::realValue, v["List"][0]["b"].type());
    ASSERT_EQ(Json::intValue, v["List"][0]["c"].type());
    ASSERT_EQ(42, v["List"][0]["a"].asInt());
    ASSERT_FLOAT_EQ(44.37f, v["List"][0]["b"].asFloat());
    ASSERT_EQ(44, v["List"][0]["b"].asInt());
    ASSERT_EQ(-43, v["List"][0]["c"].asInt());
    ASSERT_EQ("test3", v["List"][1][0].asString());
    ASSERT_EQ("test1", v["List"][1][1].asString());
    ASSERT_EQ("test2", v["List"][1][2].asString());
  }

  {
    Orthanc::LuaFunctionCall f(lua, "identity");
    f.PushJson(a);
    Json::Value v;
    f.ExecuteToJson(v, true);
    ASSERT_EQ("World", v["Hello"].asString());
    ASSERT_EQ(Json::stringValue, v["List"][0]["a"].type());
    ASSERT_EQ(Json::stringValue, v["List"][0]["b"].type());
    ASSERT_EQ(Json::stringValue, v["List"][0]["c"].type());
    ASSERT_FLOAT_EQ(42.0f, boost::lexical_cast<float>(v["List"][0]["a"].asString()));
    ASSERT_FLOAT_EQ(44.37f, boost::lexical_cast<float>(v["List"][0]["b"].asString()));
    ASSERT_FLOAT_EQ(-43.0f, boost::lexical_cast<float>(v["List"][0]["c"].asString()));
    ASSERT_EQ("test3", v["List"][1][0].asString());
    ASSERT_EQ("test1", v["List"][1][1].asString());
    ASSERT_EQ("test2", v["List"][1][2].asString());
  }

  {
    Orthanc::LuaFunctionCall f(lua, "DumpJson");
    f.PushJson(a);
    std::string s;
    f.ExecuteToString(s);

    Json::FastWriter writer;
    std::string t = writer.write(a);

    ASSERT_EQ(s, t);
  }
}



TEST(Lua, Http)
{
  Orthanc::LuaContext lua;

#if UNIT_TESTS_WITH_HTTP_CONNEXIONS == 1
  // The "http://www.orthanc-server.com/downloads/third-party/" does
  // not automatically redirect to HTTPS, so we cas use it even if the
  // OpenSSL/HTTPS support is disabled in curl
  const std::string BASE = "http://www.orthanc-server.com/downloads/third-party/";

  lua.Execute("JSON = loadstring(HttpGet('" + BASE + "JSON.lua')) ()");
  const std::string url(BASE + "Product.json");
#endif

  std::string s;
  lua.Execute(s, "print(HttpGet({}))");
  ASSERT_EQ("nil", Orthanc::Toolbox::StripSpaces(s));

#if UNIT_TESTS_WITH_HTTP_CONNEXIONS == 1  
  lua.Execute(s, "print(string.len(HttpGet(\"" + url + "\")))");
  ASSERT_LE(100, boost::lexical_cast<int>(Orthanc::Toolbox::StripSpaces(s)));

  // Parse a JSON file
  lua.Execute(s, "print(JSON:decode(HttpGet(\"" + url + "\")) ['Product'])");
  ASSERT_EQ("OrthancClient", Orthanc::Toolbox::StripSpaces(s));

#if 0
  // This part of the test can only be executed if one instance of
  // Orthanc is running on the localhost

  lua.Execute("modality = {}");
  lua.Execute("table.insert(modality, 'ORTHANC')");
  lua.Execute("table.insert(modality, 'localhost')");
  lua.Execute("table.insert(modality, 4242)");
  
  lua.Execute(s, "print(HttpPost(\"http://localhost:8042/tools/execute-script\", \"print('hello world')\"))");
  ASSERT_EQ("hello world", Orthanc::Toolbox::StripSpaces(s));

  lua.Execute(s, "print(JSON:decode(HttpPost(\"http://localhost:8042/tools/execute-script\", \"print('[10,42,1000]')\")) [2])");
  ASSERT_EQ("42", Orthanc::Toolbox::StripSpaces(s));

  // Add/remove a modality with Lua
  Json::Value v;
  lua.Execute(s, "print(HttpGet('http://localhost:8042/modalities/lua'))");
  ASSERT_EQ(0, Orthanc::Toolbox::StripSpaces(s).size());
  lua.Execute(s, "print(HttpPut('http://localhost:8042/modalities/lua', JSON:encode(modality)))");
  lua.Execute(v, "print(HttpGet('http://localhost:8042/modalities/lua'))");
  ASSERT_TRUE(v.type() == Json::arrayValue);
  lua.Execute(s, "print(HttpDelete('http://localhost:8042/modalities/lua'))");
  lua.Execute(s, "print(HttpGet('http://localhost:8042/modalities/lua'))");
  ASSERT_EQ(0, Orthanc::Toolbox::StripSpaces(s).size());
#endif

#endif
}
