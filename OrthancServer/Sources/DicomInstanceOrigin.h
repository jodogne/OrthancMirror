/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2021-2023 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
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

#include "../../OrthancFramework/Sources/RestApi/RestApiCall.h"

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

    explicit DicomInstanceOrigin(RequestOrigin origin) :
      origin_(origin)
    {
    }

  public:
    DicomInstanceOrigin() :
      origin_(RequestOrigin_Unknown)
    {
    }

    explicit DicomInstanceOrigin(const Json::Value& serialized);

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

    static DicomInstanceOrigin FromWebDav()
    {
      return DicomInstanceOrigin(RequestOrigin_WebDav);
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
