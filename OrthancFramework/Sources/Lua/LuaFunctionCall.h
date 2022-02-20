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

#if !defined(ORTHANC_ENABLE_DCMTK)
#  error The macro ORTHANC_ENABLE_DCMTK must be defined
#endif

#include "LuaContext.h"

#include "../DicomFormat/DicomArray.h"
#include "../DicomFormat/DicomMap.h"

namespace Orthanc
{
  class ORTHANC_PUBLIC LuaFunctionCall : public boost::noncopyable
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

    void Execute();

    bool ExecutePredicate();

    void ExecuteToJson(Json::Value& result,
                       bool keepStrings);

    void ExecuteToString(std::string& result);

    void ExecuteToInt(int& result);

#if ORTHANC_ENABLE_DCMTK == 1
    void ExecuteToDicom(DicomMap& target);
#endif
  };
}
