/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2026 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2026 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

#include <string>
#include <json/value.h>


namespace Orthanc
{
  class DicomInstanceDestination
  {
  private:
    std::string   remoteIp_;
    std::string   remoteAet_;

  public:
    DicomInstanceDestination(const std::string& remoteIp, const std::string& remoteAet) :
      remoteIp_(remoteIp),
      remoteAet_(remoteAet)
    {
    }

    DicomInstanceDestination()
    {
    }

    const std::string& GetRemoteIp() const
    {
      return remoteIp_;
    }

    const std::string& GetRemoteAet() const
    {
      return remoteAet_;
    }

    void Format(Json::Value& result) const;
  };
}
