/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012 Medical Physics Department, CHU of Liege,
 * Belgium
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

#include "../../Core/DicomFormat/DicomMap.h"

#include <vector>
#include <string>
#include <json/json.h>

namespace Orthanc
{
  class IStoreRequestHandler
  {
  public:
    virtual ~IStoreRequestHandler()
    {
    }

    virtual void Handle(const std::vector<uint8_t>& dicomFile,
                        const DicomMap& dicomSummary,
                        const Json::Value& dicomJson,
                        const std::string& distantAet) = 0;
  };
}
