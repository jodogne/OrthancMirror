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


#include "PrecompiledHeadersServer.h"
#include "DicomInstanceOrigin.h"

#include "../Core/OrthancException.h"


namespace Orthanc
{
  void DicomInstanceOrigin::Format(Json::Value& result) const
  {
    result = Json::objectValue;
    result["RequestOrigin"] = EnumerationToString(origin_);

    switch (origin_)
    {
      case RequestOrigin_Unknown:
      {
        // None of the methods "SetDicomProtocolOrigin()", "SetHttpOrigin()",
        // "SetLuaOrigin()" or "SetPluginsOrigin()" was called!
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }

      case RequestOrigin_DicomProtocol:
      {
        result["RemoteIp"] = remoteIp_;
        result["RemoteAet"] = dicomRemoteAet_;
        result["CalledAet"] = dicomCalledAet_;
        break;
      }

      case RequestOrigin_RestApi:
      {
        result["RemoteIp"] = remoteIp_;
        result["Username"] = httpUsername_;
        break;
      }

      case RequestOrigin_Lua:
      case RequestOrigin_Plugins:
      {
        // No additional information available for these kinds of requests
        break;
      }

      default:
        throw OrthancException(ErrorCode_InternalError);
    }
  }


  void DicomInstanceOrigin::SetDicomProtocolOrigin(const char* remoteIp,
                                                   const char* remoteAet,
                                                   const char* calledAet)
  {
    origin_ = RequestOrigin_DicomProtocol;
    remoteIp_ = remoteIp;
    dicomRemoteAet_ = remoteAet;
    dicomCalledAet_ = calledAet;
  }

  void DicomInstanceOrigin::SetRestOrigin(const RestApiCall& call)
  {
    origin_ = call.GetRequestOrigin();

    if (origin_ == RequestOrigin_RestApi)
    {
      remoteIp_ = call.GetRemoteIp();
      httpUsername_ = call.GetUsername();
    }
  }

  void DicomInstanceOrigin::SetHttpOrigin(const char* remoteIp,
                                          const char* username)
  {
    origin_ = RequestOrigin_RestApi;
    remoteIp_ = remoteIp;
    httpUsername_ = username;
  }

  void DicomInstanceOrigin::SetLuaOrigin()
  {
    origin_ = RequestOrigin_Lua;
  }

  void DicomInstanceOrigin::SetPluginsOrigin()
  {
    origin_ = RequestOrigin_Plugins;
  }

  const char* DicomInstanceOrigin::GetRemoteAet() const
  {
    if (origin_ == RequestOrigin_DicomProtocol)
    {
      return dicomRemoteAet_.c_str();
    }
    else
    {
      return "";
    }
  }
}
