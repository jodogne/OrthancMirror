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


#pragma once

#include "../Core/RestApi/RestApiCall.h"

namespace Orthanc
{
  class DicomInstanceOrigin
  {
  private:
    RequestOrigin origin_;
    std::string   remoteIp_;
    std::string   dicomRemoteAet_;
    std::string   dicomCalledAet_;
    std::string   httpUsername_;

    DicomInstanceOrigin(RequestOrigin origin) :
      origin_(origin)
    {
    }

  public:
    DicomInstanceOrigin() :
      origin_(RequestOrigin_Unknown)
    {
    }

    DicomInstanceOrigin(const Json::Value& serialized);

    static DicomInstanceOrigin FromDicomProtocol(const char* remoteIp,
                                                 const char* remoteAet,
                                                 const char* calledAet);

    static DicomInstanceOrigin FromRest(const RestApiCall& call);

    static DicomInstanceOrigin FromHttp(const char* remoteIp,
                                        const char* username);

    static DicomInstanceOrigin FromLua()
    {
      return DicomInstanceOrigin(RequestOrigin_Lua);
    }

    static DicomInstanceOrigin FromPlugins()
    {
      return DicomInstanceOrigin(RequestOrigin_Plugins);
    }

    RequestOrigin GetRequestOrigin() const
    {
      return origin_;
    }

    const char* GetRemoteAetC() const; 

    bool LookupRemoteAet(std::string& result) const;

    bool LookupRemoteIp(std::string& result) const;

    bool LookupCalledAet(std::string& result) const;

    bool LookupHttpUsername(std::string& result) const;

    void Format(Json::Value& result) const;

    void Serialize(Json::Value& result) const;
  };
}
