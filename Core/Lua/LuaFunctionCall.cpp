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


#include "../PrecompiledHeaders.h"
#include "LuaFunctionCall.h"

#include "../OrthancException.h"
#include "../Logging.h"

#include <cassert>
#include <stdio.h>
#include <boost/lexical_cast.hpp>

namespace Orthanc
{
  void LuaFunctionCall::CheckAlreadyExecuted()
  {
    if (isExecuted_)
    {
      throw OrthancException(ErrorCode_LuaAlreadyExecuted);
    }
  }

  LuaFunctionCall::LuaFunctionCall(LuaContext& context,
                                   const char* functionName) : 
    context_(context),
    isExecuted_(false)
  {
    // Clear the stack to fulfill the invariant
    lua_settop(context_.lua_, 0);
    lua_getglobal(context_.lua_, functionName);
  }

  void LuaFunctionCall::PushString(const std::string& value)
  {
    CheckAlreadyExecuted();
    lua_pushlstring(context_.lua_, value.c_str(), value.size());
  }

  void LuaFunctionCall::PushBoolean(bool value)
  {
    CheckAlreadyExecuted();
    lua_pushboolean(context_.lua_, value);
  }

  void LuaFunctionCall::PushInteger(int value)
  {
    CheckAlreadyExecuted();
    lua_pushinteger(context_.lua_, value);
  }

  void LuaFunctionCall::PushDouble(double value)
  {
    CheckAlreadyExecuted();
    lua_pushnumber(context_.lua_, value);
  }

  void LuaFunctionCall::PushJson(const Json::Value& value)
  {
    CheckAlreadyExecuted();
    context_.PushJson(value);
  }

  void LuaFunctionCall::ExecuteInternal(int numOutputs)
  {
    CheckAlreadyExecuted();

    assert(lua_gettop(context_.lua_) >= 1);
    int nargs = lua_gettop(context_.lua_) - 1;
    int error = lua_pcall(context_.lua_, nargs, numOutputs, 0);

    if (error) 
    {
      assert(lua_gettop(context_.lua_) >= 1);
          
      std::string description(lua_tostring(context_.lua_, -1));
      lua_pop(context_.lua_, 1); /* pop error message from the stack */
      LOG(ERROR) << description;

      throw OrthancException(ErrorCode_CannotExecuteLua);
    }

    if (lua_gettop(context_.lua_) < numOutputs)
    {
      throw OrthancException(ErrorCode_LuaBadOutput);
    }

    isExecuted_ = true;
  }

  bool LuaFunctionCall::ExecutePredicate()
  {
    ExecuteInternal(1);
    
    if (!lua_isboolean(context_.lua_, 1))
    {
      throw OrthancException(ErrorCode_NotLuaPredicate);
    }

    return lua_toboolean(context_.lua_, 1) != 0;
  }


  void LuaFunctionCall::ExecuteToJson(Json::Value& result,
                                      bool keepStrings)
  {
    ExecuteInternal(1);
    context_.GetJson(result, lua_gettop(context_.lua_), keepStrings);
  }


  void LuaFunctionCall::ExecuteToString(std::string& result)
  {
    ExecuteInternal(1);
    
    int top = lua_gettop(context_.lua_);
    if (lua_isstring(context_.lua_, top))
    {
      result = lua_tostring(context_.lua_, top);
    }
    else
    {
      throw OrthancException(ErrorCode_LuaReturnsNoString);
    }
  }


  void LuaFunctionCall::PushStringMap(const std::map<std::string, std::string>& value)
  {
    Json::Value json = Json::objectValue;

    for (std::map<std::string, std::string>::const_iterator
           it = value.begin(); it != value.end(); ++it)
    {
      json[it->first] = it->second;
    }

    PushJson(json);
  }


  void LuaFunctionCall::PushDicom(const DicomMap& dicom)
  {
    DicomArray a(dicom);
    PushDicom(a);
  }


  void LuaFunctionCall::PushDicom(const DicomArray& dicom)
  {
    Json::Value value = Json::objectValue;

    for (size_t i = 0; i < dicom.GetSize(); i++)
    {
      const DicomValue& v = dicom.GetElement(i).GetValue();
      std::string s = (v.IsNull() || v.IsBinary()) ? "" : v.GetContent();
      value[dicom.GetElement(i).GetTag().Format()] = s;
    }

    PushJson(value);
  }
}
