/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2013 Medical Physics Department, CHU of Liege,
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


#include "LuaContext.h"

#include <glog/logging.h>

extern "C" 
{
#include <lualib.h>
#include <lauxlib.h>
}

namespace Orthanc
{
  int LuaContext::PrintToLog(lua_State *L)
  {
    // http://medek.wordpress.com/2009/02/03/wrapping-lua-errors-and-print-function/
    int nArgs = lua_gettop(L);
    lua_getglobal(L, "tostring");

    // Make sure you start at 1 *NOT* 0 for arrays in Lua.
    std::string result;

    for (int i = 1; i <= nArgs; i++)
    {
      const char *s;
      lua_pushvalue(L, -1);
      lua_pushvalue(L, i);
      lua_call(L, 1, 1);
      s = lua_tostring(L, -1);

      if (result.size() > 0)
        result.append(", ");

      if (s == NULL)
        result.append("<No conversion to string>");
      else
        result.append(s);
 
      lua_pop(L, 1);
    }

    LOG(WARNING) << "Lua: " << result;         

    return 0;
  }


  LuaContext::LuaContext()
  {
    lua_ = luaL_newstate();
    if (!lua_)
    {
      throw LuaException("Unable to create the Lua context");
    }

    luaL_openlibs(lua_);
    lua_register(lua_, "print", PrintToLog);
  }


  LuaContext::~LuaContext()
  {
    lua_close(lua_);
  }


  void LuaContext::Execute(const std::string& command)
  {
    boost::mutex::scoped_lock lock(mutex_);

    lua_settop(lua_, 0);

    int error = (luaL_loadbuffer(lua_, command.c_str(), command.size(), "line") ||
                 lua_pcall(lua_, 0, 0, 0));

    if (error) 
    {
      assert(lua_gettop(lua_) >= 1);

      std::string description(lua_tostring(lua_, -1));
      lua_pop(lua_, 1); /* pop error message from the stack */
      throw LuaException(description);
    }
  }


  void LuaContext::Execute(EmbeddedResources::FileResourceId resource)
  {
    std::string command;
    EmbeddedResources::GetFileResource(command, resource);
    Execute(command);
  }
}
