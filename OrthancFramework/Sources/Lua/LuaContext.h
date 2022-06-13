/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#pragma once

// To have ORTHANC_ENABLE_LUA defined if using the shared library
#include "../OrthancFramework.h"

#if !defined(ORTHANC_ENABLE_LUA)
#  error The macro ORTHANC_ENABLE_LUA must be defined
#endif

#if !defined(ORTHANC_ENABLE_CURL)
#  error Macro ORTHANC_ENABLE_CURL must be defined
#endif

#if ORTHANC_ENABLE_LUA == 0
#  error The Lua support is disabled, cannot include this file
#endif

#if ORTHANC_ENABLE_CURL == 1
#  include "../HttpClient.h"
#endif

extern "C" 
{
#include <lua.h>
}

#include <boost/noncopyable.hpp>
#include <json/value.h>

namespace Orthanc
{
  class ORTHANC_PUBLIC LuaContext : public boost::noncopyable
  {
  private:
    friend class LuaFunctionCall;

    lua_State *lua_;
    std::string log_;

#if ORTHANC_ENABLE_CURL == 1
    HttpClient httpClient_;
#endif

    static int PrintToLog(lua_State *state);
    static int ParseJson(lua_State *state);
    static int DumpJson(lua_State *state);

#if ORTHANC_ENABLE_CURL == 1
    static int SetHttpCredentials(lua_State *state);
    static int SetHttpTimeout(lua_State *state);
    static int CallHttpPostOrPut(lua_State *state,
                                 HttpMethod method);
    static int CallHttpGet(lua_State *state);
    static int CallHttpPost(lua_State *state);
    static int CallHttpPut(lua_State *state);
    static int CallHttpDelete(lua_State *state);
#endif

    bool AnswerHttpQuery(lua_State* state);

    void ExecuteInternal(std::string* output,
                         const std::string& command);

    static void GetJson(Json::Value& result,
                        lua_State* state,
                        int top,
                        bool keepStrings);

    void SetHttpHeaders(int top);
    
  public:
    LuaContext();

    ~LuaContext();

    void Execute(const std::string& command);

    void Execute(std::string& output,
                 const std::string& command);

    void Execute(Json::Value& output,
                 const std::string& command);

    bool IsExistingFunction(const char* name);

    void RegisterFunction(const char* name,
                          lua_CFunction func);

    void SetGlobalVariable(const char* name,
                           void* value);

    static LuaContext& GetLuaContext(lua_State *state);

    static const void* GetGlobalVariable(lua_State* state,
                                         const char* name);

    void PushJson(const Json::Value& value);

    static void GetDictionaryArgument(std::map<std::string, std::string>& target,
                                      lua_State* state,
                                      int top,
                                      bool keyToLowerCase);
  };
}
