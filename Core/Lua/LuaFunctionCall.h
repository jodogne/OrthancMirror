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


#pragma once

#include "LuaContext.h"

#include "../DicomFormat/DicomArray.h"
#include "../DicomFormat/DicomMap.h"

#include <json/json.h>

namespace Orthanc
{
  class LuaFunctionCall : public boost::noncopyable
  {
  private:
    LuaContext& context_;
    bool isExecuted_;

    void CheckAlreadyExecuted();

  protected:
    void ExecuteInternal(int numOutputs);

    lua_State* GetState()
    {
      return context_.lua_;
    }

  public:
    LuaFunctionCall(LuaContext& context,
                    const char* functionName);

    void PushString(const std::string& value);

    void PushBoolean(bool value);

    void PushInteger(int value);

    void PushDouble(double value);

    void PushJson(const Json::Value& value);

    void PushStringMap(const std::map<std::string, std::string>& value);

    void PushDicom(const DicomMap& dicom);

    void PushDicom(const DicomArray& dicom);

    void Execute()
    {
      ExecuteInternal(0);
    }

    bool ExecutePredicate();

    void ExecuteToJson(Json::Value& result,
                       bool keepStrings);

    void ExecuteToString(std::string& result);
  };
}
