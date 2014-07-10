/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2014 Medical Physics Department, CHU of Liege,
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


#include "../PrecompiledHeadersServer.h"
#include "OrthancRestApi.h"

#include "../OrthancInitialization.h"
#include "../FromDcmtkBridge.h"

#include <glog/logging.h>


namespace Orthanc
{
  // System information -------------------------------------------------------

  static void ServeRoot(RestApiGetCall& call)
  {
    call.GetOutput().Redirect("app/explorer.html");
  }
 
  static void GetSystemInformation(RestApiGetCall& call)
  {
    Json::Value result = Json::objectValue;

    result["Version"] = ORTHANC_VERSION;
    result["Name"] = Configuration::GetGlobalStringParameter("Name", "");

    call.GetOutput().AnswerJson(result);
  }

  static void GetStatistics(RestApiGetCall& call)
  {
    Json::Value result = Json::objectValue;
    OrthancRestApi::GetIndex(call).ComputeStatistics(result);
    call.GetOutput().AnswerJson(result);
  }

  static void GenerateUid(RestApiGetCall& call)
  {
    std::string level = call.GetArgument("level", "");
    if (level == "patient")
    {
      call.GetOutput().AnswerBuffer(FromDcmtkBridge::GenerateUniqueIdentifier(ResourceType_Patient), "text/plain");
    }
    else if (level == "study")
    {
      call.GetOutput().AnswerBuffer(FromDcmtkBridge::GenerateUniqueIdentifier(ResourceType_Study), "text/plain");
    }
    else if (level == "series")
    {
      call.GetOutput().AnswerBuffer(FromDcmtkBridge::GenerateUniqueIdentifier(ResourceType_Series), "text/plain");
    }
    else if (level == "instance")
    {
      call.GetOutput().AnswerBuffer(FromDcmtkBridge::GenerateUniqueIdentifier(ResourceType_Instance), "text/plain");
    }
  }

  static void ExecuteScript(RestApiPostCall& call)
  {
    std::string result;
    ServerContext& context = OrthancRestApi::GetContext(call);

    {
      ServerContext::LuaContextLocker locker(context);
      locker.GetLua().Execute(result, call.GetPostBody());
    }

    call.GetOutput().AnswerBuffer(result, "text/plain");
  }

  static void GetNowIsoString(RestApiGetCall& call)
  {
    call.GetOutput().AnswerBuffer(Toolbox::GetNowIsoString(), "text/plain");
  }

  void OrthancRestApi::RegisterSystem()
  {
    Register("/", ServeRoot);
    Register("/system", GetSystemInformation);
    Register("/statistics", GetStatistics);
    Register("/tools/generate-uid", GenerateUid);
    Register("/tools/execute-script", ExecuteScript);
    Register("/tools/now", GetNowIsoString);
  }
}
