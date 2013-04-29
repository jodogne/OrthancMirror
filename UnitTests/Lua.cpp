#include "gtest/gtest.h"

#include "../Core/Lua/LuaFunctionCall.h"


TEST(Lua, Simple)
{
  try
  {
    Orthanc::LuaContext lua;
    lua.Execute(Orthanc::EmbeddedResources::LUA_TOOLBOX);
    lua.Execute("a={}");
    lua.Execute("a['x'] = 10");
    lua.Execute("a['y'] = {}");
    lua.Execute("a['y'][1] = 20");
    lua.Execute("a['y'][2] = 20");
    lua.Execute("rPrint(a)");

    lua.Execute("function f(a) print(a.bool) return a.bool,20,30,40,50,60 end");

    Json::Value v, vv, o;
    //v["a"] = "b";
    v.append("hello");
    v.append("world");
    v.append("42");
    vv.append("coucou");
    vv.append("toi");
    v.append(vv);
    o = Json::objectValue;
    o["x"] = 10;
    o["y"] = 20;
    o["z"] = 20.5f;
    v.append(o);

    {
      Orthanc::LuaFunctionCall f(lua, "rPrint");
      f.PushJSON(v);
      f.Execute();
    }

    {
      Orthanc::LuaFunctionCall f(lua, "f");
      f.PushJSON(o);
      ASSERT_THROW(f.ExecutePredicate(), Orthanc::LuaException);
    }

    o["bool"] = false;

    {
      Orthanc::LuaFunctionCall f(lua, "f");
      f.PushJSON(o);
      ASSERT_FALSE(f.ExecutePredicate());
    }

    o["bool"] = true;

    {
      Orthanc::LuaFunctionCall f(lua, "f");
      f.PushJSON(o);
      ASSERT_TRUE(f.ExecutePredicate());
    }

  }
  catch (Orthanc::LuaException e)
  {
    std::cerr << "EXCEPTION: [" << e.What() << "]" << std::endl;
  }
}
