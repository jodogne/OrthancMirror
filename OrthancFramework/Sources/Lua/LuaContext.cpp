/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2021-2023 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#include "../PrecompiledHeaders.h"
#include "LuaContext.h"

#include "../Logging.h"
#include "../OrthancException.h"
#include "../Toolbox.h"

#include <set>
#include <cassert>
#include <boost/lexical_cast.hpp>

extern "C" 
{
#include <lualib.h>
#include <lauxlib.h>
}

namespace Orthanc
{
  static bool OnlyContainsDigits(const std::string& s)
  {
    for (size_t i = 0; i < s.size(); i++)
    {
      if (!isdigit(s[i]))
      {
        return false;
      }
    }

    return true;
  }
  
  LuaContext& LuaContext::GetLuaContext(lua_State *state)
  {
    const void* value = GetGlobalVariable(state, "_LuaContext");
    assert(value != NULL);

    return *const_cast<LuaContext*>(reinterpret_cast<const LuaContext*>(value));
  }

  int LuaContext::PrintToLog(lua_State *state)
  {
    LuaContext& that = GetLuaContext(state);

    // http://medek.wordpress.com/2009/02/03/wrapping-lua-errors-and-print-function/
    int nArgs = lua_gettop(state);
    lua_getglobal(state, "tostring");

    // Make sure you start at 1 *NOT* 0 for arrays in Lua.
    std::string result;

    for (int i = 1; i <= nArgs; i++)
    {
      const char *s;
      lua_pushvalue(state, -1);
      lua_pushvalue(state, i);
      lua_call(state, 1, 1);
      s = lua_tostring(state, -1);

      if (result.size() > 0)
        result.append(", ");

      if (s == NULL)
        result.append("<No conversion to string>");
      else
        result.append(s);
 
      lua_pop(state, 1);
    }

    LOG(WARNING) << "Lua says: " << result;         
    that.log_.append(result);
    that.log_.append("\n");

    return 0;
  }


  int LuaContext::ParseJson(lua_State *state)
  {
    LuaContext& that = GetLuaContext(state);

    int nArgs = lua_gettop(state);
    if (nArgs != 1 ||
        !lua_isstring(state, 1))    // Password
    {
      lua_pushnil(state);
      return 1;
    }

    const char* str = lua_tostring(state, 1);

    Json::Value value;
    if (Toolbox::ReadJson(value, str, strlen(str)))
    {
      that.PushJson(value);
    }
    else
    {
      lua_pushnil(state);
    }

    return 1;
  }


  int LuaContext::DumpJson(lua_State *state)
  {
    LuaContext& that = GetLuaContext(state);

    int nArgs = lua_gettop(state);
    if ((nArgs != 1 && nArgs != 2) ||
        (nArgs == 2 && !lua_isboolean(state, 2)))
    {
      lua_pushnil(state);
      return 1;
    }

    bool keepStrings = false;
    if (nArgs == 2)
    {
      keepStrings = lua_toboolean(state, 2) ? true : false;
    }

    Json::Value json;
    that.GetJson(json, state, 1, keepStrings);

    std::string s;
    Toolbox::WriteFastJson(s, json);
    lua_pushlstring(state, s.c_str(), s.size());

    return 1;
  }


#if ORTHANC_ENABLE_CURL == 1
  int LuaContext::SetHttpCredentials(lua_State *state)
  {
    LuaContext& that = GetLuaContext(state);

    // Check the types of the arguments
    int nArgs = lua_gettop(state);
    if (nArgs != 2 ||
        !lua_isstring(state, 1) ||  // Username
        !lua_isstring(state, 2))    // Password
    {
      LOG(ERROR) << "Lua: Bad parameters to SetHttpCredentials()";
    }
    else
    {
      // Configure the HTTP client
      const char* username = lua_tostring(state, 1);
      const char* password = lua_tostring(state, 2);
      that.httpClient_.SetCredentials(username, password);
    }

    return 0;
  }
#endif


#if ORTHANC_ENABLE_CURL == 1
  int LuaContext::SetHttpTimeout(lua_State *state)
  {
    LuaContext& that = GetLuaContext(state);

    // Check the types of the arguments
    int nArgs = lua_gettop(state);
    if (nArgs != 1 ||
        !lua_isnumber(state, 1))    // Timeout
    {
      LOG(ERROR) << "Lua: Bad parameters to SetHttpTimeout()";
    }
    else
    {
      // Configure the HTTP client
      // Convert to "int" if truncation does not loose precision
      long timeout = static_cast<long>(lua_tonumber(state, 1));

      that.httpClient_.SetTimeout(timeout);
    }

    return 0;
  }
#endif


#if ORTHANC_ENABLE_CURL == 1
  bool LuaContext::AnswerHttpQuery(lua_State* state)
  {
    std::string str;

    try
    {
      httpClient_.Apply(str);
    }
    catch (OrthancException&)
    {
      return false;
    }

    // Return the result of the HTTP request
    lua_pushlstring(state, str.c_str(), str.size());

    return true;
  }
#endif
  

#if ORTHANC_ENABLE_CURL == 1
  void LuaContext::SetHttpHeaders(int top)
  {
    std::map<std::string, std::string> headers;
    GetDictionaryArgument(headers, lua_, top, false /* keep key case as provided by Lua script */);
      
    httpClient_.ClearHeaders(); // always reset headers in case they have been set in a previous request

    for (std::map<std::string, std::string>::const_iterator
           it = headers.begin(); it != headers.end(); ++it)
    {
      httpClient_.AddHeader(it->first, it->second);
    }
  }
#endif


#if ORTHANC_ENABLE_CURL == 1
  int LuaContext::CallHttpGet(lua_State *state)
  {
    LuaContext& that = GetLuaContext(state);

    // Check the types of the arguments
    int nArgs = lua_gettop(state);
    if (nArgs < 1 || nArgs > 2 ||         // check args count
       !lua_isstring(state, 1))           // URL is a string
    {
      LOG(ERROR) << "Lua: Bad parameters to HttpGet()";
      lua_pushnil(state);
      return 1;
    }

    // Configure the HTTP client class
    const char* url = lua_tostring(state, 1);
    that.httpClient_.SetMethod(HttpMethod_Get);
    that.httpClient_.SetUrl(url);
    that.httpClient_.ClearBody();
    that.SetHttpHeaders(2);

    // Do the HTTP GET request
    if (!that.AnswerHttpQuery(state))
    {
      LOG(ERROR) << "Lua: Error in HttpGet() for URL " << url;
      lua_pushnil(state);
    }

    return 1;
  }
#endif


#if ORTHANC_ENABLE_CURL == 1
  int LuaContext::CallHttpPostOrPut(lua_State *state,
                                    HttpMethod method)
  {
    LuaContext& that = GetLuaContext(state);

    // Check the types of the arguments
    int nArgs = lua_gettop(state);
    if ((nArgs < 1 || nArgs > 3) ||                 // check arg count
        !lua_isstring(state, 1) ||                  // URL is a string
        (nArgs >= 2 && (!lua_isstring(state, 2) && !lua_isnil(state, 2))))    // Body data is null or is a string
    {
      LOG(ERROR) << "Lua: Bad parameters to HttpPost() or HttpPut()";
      lua_pushnil(state);
      return 1;
    }

    // Configure the HTTP client class
    const char* url = lua_tostring(state, 1);
    that.httpClient_.SetMethod(method);
    that.httpClient_.SetUrl(url);
    that.SetHttpHeaders(3);

    if (nArgs >= 2 && !lua_isnil(state, 2))
    {
      size_t bodySize = 0;
      const char* bodyData = lua_tolstring(state, 2, &bodySize);

      if (bodySize == 0)
      {
        that.httpClient_.ClearBody();
      }
      else
      {
        that.httpClient_.AssignBody(bodyData, bodySize);
      }
    }
    else
    {
      that.httpClient_.ClearBody();
    }

    // Do the HTTP POST/PUT request
    if (!that.AnswerHttpQuery(state))
    {
      LOG(ERROR) << "Lua: Error in HttpPost() or HttpPut() for URL " << url;
      lua_pushnil(state);
    }

    return 1;
  }
#endif
  

#if ORTHANC_ENABLE_CURL == 1
  int LuaContext::CallHttpPost(lua_State *state)
  {
    return CallHttpPostOrPut(state, HttpMethod_Post);
  }
#endif


#if ORTHANC_ENABLE_CURL == 1
  int LuaContext::CallHttpPut(lua_State *state)
  {
    return CallHttpPostOrPut(state, HttpMethod_Put);
  }
#endif


#if ORTHANC_ENABLE_CURL == 1
  int LuaContext::CallHttpDelete(lua_State *state)
  {
    LuaContext& that = GetLuaContext(state);

    // Check the types of the arguments
    int nArgs = lua_gettop(state);
    if (nArgs < 1 || nArgs > 2 || !lua_isstring(state, 1))  // URL
    {
      LOG(ERROR) << "Lua: Bad parameters to HttpDelete()";
      lua_pushnil(state);
      return 1;
    }

    // Configure the HTTP client class
    const char* url = lua_tostring(state, 1);
    that.httpClient_.SetMethod(HttpMethod_Delete);
    that.httpClient_.SetUrl(url);
    that.httpClient_.ClearBody();
    that.SetHttpHeaders(2);

    // Do the HTTP DELETE request
    std::string s;
    if (!that.httpClient_.Apply(s))
    {
      LOG(ERROR) << "Lua: Error in HttpDelete() for URL " << url;
      lua_pushnil(state);
    }
    else
    {
      lua_pushstring(state, "SUCCESS");
    }

    return 1;
  }
#endif


  void LuaContext::PushJson(const Json::Value& value)
  {
    if (value.isString())
    {
      const std::string s = value.asString();
      lua_pushlstring(lua_, s.c_str(), s.size());
    }
    else if (value.isInt())
    {
      lua_pushinteger(lua_, value.asInt());
    }
    else if (value.isUInt())
    {
      lua_pushinteger(lua_, value.asUInt());
    }
    else if (value.isDouble())
    {
      lua_pushnumber(lua_, value.asDouble());
    }
    else if (value.isBool())
    {
      lua_pushboolean(lua_, value.asBool());
    }
    else if (value.isNull())
    {
      lua_pushnil(lua_);
    }
    else if (value.isArray())
    {
      lua_newtable(lua_);

      // http://lua-users.org/wiki/SimpleLuaApiExample
      for (Json::Value::ArrayIndex i = 0; i < value.size(); i++)
      {
        // Push the table index (note the "+1" because of Lua conventions)
        lua_pushnumber(lua_, i + 1);

        // Push the value of the cell
        PushJson(value[i]);

        // Stores the pair in the table
        lua_rawset(lua_, -3);
      }
    }
    else if (value.isObject())
    {
      lua_newtable(lua_);

      Json::Value::Members members = value.getMemberNames();

      for (Json::Value::Members::const_iterator 
             it = members.begin(); it != members.end(); ++it)
      {
        // Push the index of the cell
        lua_pushlstring(lua_, it->c_str(), it->size());

        // Push the value of the cell
        PushJson(value[*it]);

        // Stores the pair in the table
        lua_rawset(lua_, -3);
      }
    }
    else
    {
      throw OrthancException(ErrorCode_JsonToLuaTable);
    }
  }


  void LuaContext::GetJson(Json::Value& result,
                           lua_State* state,
                           int top,
                           bool keepStrings)
  {
    if (lua_istable(state, top))
    {
      Json::Value tmp = Json::objectValue;
      bool isArray = true;
      size_t size = 0;

      // Code adapted from: http://stackoverflow.com/a/6142700/881731
      
      // Push another reference to the table on top of the stack (so we know
      // where it is, and this function can work for negative, positive and
      // pseudo indices
      lua_pushvalue(state, top);
      // stack now contains: -1 => table
      lua_pushnil(state);
      // stack now contains: -1 => nil; -2 => table
      while (lua_next(state, -2))
      {
        // stack now contains: -1 => value; -2 => key; -3 => table
        // copy the key so that lua_tostring does not modify the original
        lua_pushvalue(state, -2);
        // stack now contains: -1 => key; -2 => value; -3 => key; -4 => table
        std::string key(lua_tostring(state, -1));
        Json::Value v;
        GetJson(v, state, -2, keepStrings);

        tmp[key] = v;

        size += 1;
        try
        {
          if (!OnlyContainsDigits(key) ||
              boost::lexical_cast<size_t>(key) != size)
          {
            isArray = false;
          }
        }
        catch (boost::bad_lexical_cast&)
        {
          isArray = false;
        }
        
        // pop value + copy of key, leaving original key
        lua_pop(state, 2);
        // stack now contains: -1 => key; -2 => table
      }
      // stack now contains: -1 => table (when lua_next returns 0 it pops the key
      // but does not push anything.)
      // Pop table
      lua_pop(state, 1);

      // Stack is now the same as it was on entry to this function

      if (isArray)
      {
        result = Json::arrayValue;
        for (size_t i = 0; i < size; i++)
        {
          result.append(tmp[boost::lexical_cast<std::string>(i + 1)]);
        }
      }
      else
      {
        result = tmp;
      }
    }
    else if (lua_isnil(state, top))
    {
      result = Json::nullValue;
    }
    else if (!keepStrings &&
             lua_isboolean(state, top))
    {
      result = lua_toboolean(state, top) ? true : false;
    }
    else if (!keepStrings &&
             lua_isnumber(state, top))
    {
      // Convert to "int" if truncation does not loose precision
      double value = static_cast<double>(lua_tonumber(state, top));
      int truncated = static_cast<int>(value);

      if (std::abs(value - static_cast<double>(truncated)) <= 
          std::numeric_limits<double>::epsilon())
      {
        result = truncated;
      }
      else
      {
        result = value;
      }
    }
    else if (lua_isstring(state, top))
    {
      // Caution: The "lua_isstring()" case must be the last, since
      // Lua can convert most types to strings by default.
      result = std::string(lua_tostring(state, top));
    }
    else if (lua_isboolean(state, top))
    {
      result = lua_toboolean(state, top) ? true : false;
    }
    else
    {
      LOG(WARNING) << "Unsupported Lua type when returning Json";
      result = Json::nullValue;
    }
  }


  LuaContext::LuaContext()
  {
    lua_ = luaL_newstate();
    if (!lua_)
    {
      throw OrthancException(ErrorCode_CannotCreateLua);
    }

    luaL_openlibs(lua_);
    lua_register(lua_, "print", PrintToLog);
    lua_register(lua_, "ParseJson", ParseJson);
    lua_register(lua_, "DumpJson", DumpJson);
    
#if ORTHANC_ENABLE_CURL == 1
    lua_register(lua_, "HttpGet", CallHttpGet);
    lua_register(lua_, "HttpPost", CallHttpPost);
    lua_register(lua_, "HttpPut", CallHttpPut);
    lua_register(lua_, "HttpDelete", CallHttpDelete);
    lua_register(lua_, "SetHttpCredentials", SetHttpCredentials);
    lua_register(lua_, "SetHttpTimeout", SetHttpTimeout);
#endif

    SetGlobalVariable("_LuaContext", this);
  }


  LuaContext::~LuaContext()
  {
    lua_close(lua_);
  }


  void LuaContext::Execute(const std::string &command)
  {
    ExecuteInternal(NULL, command);
  }

  void LuaContext::Execute(std::string &output, const std::string &command)
  {
    ExecuteInternal(&output, command);
  }


  void LuaContext::ExecuteInternal(std::string* output,
                                   const std::string& command)
  {
    log_.clear();
    int error = (luaL_loadbuffer(lua_, command.c_str(), command.size(), "line") ||
                 lua_pcall(lua_, 0, 0, 0));

    if (error) 
    {
      assert(lua_gettop(lua_) >= 1);

      std::string description(lua_tostring(lua_, -1));
      lua_pop(lua_, 1); /* pop error message from the stack */
      throw OrthancException(ErrorCode_CannotExecuteLua, description);
    }

    if (output != NULL)
    {
      *output = log_;
    }
  }


  bool LuaContext::IsExistingFunction(const char* name)
  {
    lua_settop(lua_, 0);
    lua_getglobal(lua_, name);
    return lua_type(lua_, -1) == LUA_TFUNCTION;
  }


  void LuaContext::Execute(Json::Value& output,
                           const std::string& command)
  {
    std::string s;
    ExecuteInternal(&s, command);

    if (!Toolbox::ReadJson(output, s))
    {
      throw OrthancException(ErrorCode_BadJson);
    }
  }


  void LuaContext::RegisterFunction(const char* name,
                                    lua_CFunction func)
  {
    lua_register(lua_, name, func);
  }


  void LuaContext::SetGlobalVariable(const char* name,
                                     void* value)
  {
    lua_pushlightuserdata(lua_, value);
    lua_setglobal(lua_, name);
  }

  
  const void* LuaContext::GetGlobalVariable(lua_State* state,
                                            const char* name)
  {
    lua_getglobal(state, name);
    assert(lua_type(state, -1) == LUA_TLIGHTUSERDATA);
    const void* value = lua_topointer(state, -1);
    lua_pop(state, 1);
    return value;
  }


  void LuaContext::GetDictionaryArgument(std::map<std::string, std::string>& target,
                                         lua_State* state,
                                         int top,
                                         bool keyToLowerCase)
  {
    target.clear();

    if (lua_gettop(state) >= top)
    {
      Json::Value headers;
      GetJson(headers, state, top, true);

      Json::Value::Members members = headers.getMemberNames();

      for (size_t i = 0; i < members.size(); i++)
      {
        std::string key = members[i];

        if (keyToLowerCase)
        {
          Toolbox::ToLowerCase(key);
        }
        
        target[key] = headers[members[i]].asString();
      }
    }
  }
}
