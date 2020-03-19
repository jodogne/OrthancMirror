/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
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
#include "../Core/SerializationToolbox.h"

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


  DicomInstanceOrigin DicomInstanceOrigin::FromDicomProtocol(const char* remoteIp,
                                                             const char* remoteAet,
                                                             const char* calledAet)
  {
    DicomInstanceOrigin result(RequestOrigin_DicomProtocol);
    result.remoteIp_ = remoteIp;
    result.dicomRemoteAet_ = remoteAet;
    result.dicomCalledAet_ = calledAet;
    return result;
  }

  DicomInstanceOrigin DicomInstanceOrigin::FromRest(const RestApiCall& call)
  {
    DicomInstanceOrigin result(call.GetRequestOrigin());

    if (result.origin_ == RequestOrigin_RestApi)
    {
      result.remoteIp_ = call.GetRemoteIp();
      result.httpUsername_ = call.GetUsername();
    }

    return result;
  }

  DicomInstanceOrigin DicomInstanceOrigin::FromHttp(const char* remoteIp,
                                                    const char* username)
  {
    DicomInstanceOrigin result(RequestOrigin_RestApi);
    result.remoteIp_ = remoteIp;
    result.httpUsername_ = username;
    return result;
  }

  const char* DicomInstanceOrigin::GetRemoteAetC() const
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

  bool DicomInstanceOrigin::LookupRemoteAet(std::string& result) const
  {
    if (origin_ == RequestOrigin_DicomProtocol)
    {
      result = dicomRemoteAet_.c_str();
      return true;
    }
    else
    {
      return false;
    }
  }

  bool DicomInstanceOrigin::LookupRemoteIp(std::string& result) const
  {
    if (origin_ == RequestOrigin_DicomProtocol ||
        origin_ == RequestOrigin_RestApi)
    {
      result = remoteIp_;
      return true;
    }
    else
    {
      return false;
    }
  }

  bool DicomInstanceOrigin::LookupCalledAet(std::string& result) const
  {
    if (origin_ == RequestOrigin_DicomProtocol)
    {
      result = dicomCalledAet_;
      return true;
    }
    else
    {
      return false;
    }
  }

  bool DicomInstanceOrigin::LookupHttpUsername(std::string& result) const
  {
    if (origin_ == RequestOrigin_RestApi)
    {
      result = httpUsername_;
      return true;
    }
    else
    {
      return false;
    }
  }



  static const char* ORIGIN = "Origin";
  static const char* REMOTE_IP = "RemoteIP";
  static const char* DICOM_REMOTE_AET = "RemoteAET";
  static const char* DICOM_CALLED_AET = "CalledAET";
  static const char* HTTP_USERNAME = "Username";
  

  DicomInstanceOrigin::DicomInstanceOrigin(const Json::Value& serialized)
  {
    origin_ = StringToRequestOrigin(SerializationToolbox::ReadString(serialized, ORIGIN));
    remoteIp_ = SerializationToolbox::ReadString(serialized, REMOTE_IP);
    dicomRemoteAet_ = SerializationToolbox::ReadString(serialized, DICOM_REMOTE_AET);
    dicomCalledAet_ = SerializationToolbox::ReadString(serialized, DICOM_CALLED_AET);
    httpUsername_ = SerializationToolbox::ReadString(serialized, HTTP_USERNAME);
  }
  
  
  void DicomInstanceOrigin::Serialize(Json::Value& result) const
  {
    result = Json::objectValue;
    result[ORIGIN] = EnumerationToString(origin_);
    result[REMOTE_IP] = remoteIp_;
    result[DICOM_REMOTE_AET] = dicomRemoteAet_;
    result[DICOM_CALLED_AET] = dicomCalledAet_;
    result[HTTP_USERNAME] = httpUsername_;
  }
}
