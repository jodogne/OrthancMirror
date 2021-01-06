/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
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


#if ORTHANC_UNIT_TESTS_LINK_FRAMEWORK == 1
// Must be the first to be sure to use the Orthanc framework shared library
#  include <OrthancFramework.h>
#endif

#include <gtest/gtest.h>

#include "../Sources/OrthancException.h"
#include "../Sources/Toolbox.h"
#include "../Sources/Lua/LuaFunctionCall.h"

#include <boost/lexical_cast.hpp>


TEST(Lua, Existing)
{
  Orthanc::LuaContext lua;
  lua.Execute("a={}");
  lua.Execute("function f() end");

  ASSERT_TRUE(lua.IsExistingFunction("f"));
  ASSERT_FALSE(lua.IsExistingFunction("a"));
  ASSERT_FALSE(lua.IsExistingFunction("Dummy"));
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

    std::string t;
    Orthanc::Toolbox::WriteFastJson(t, a);
    ASSERT_EQ(s, t);
  }
}
