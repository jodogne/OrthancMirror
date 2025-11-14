/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2025 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2025 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#include "../PrecompiledHeaders.h"
#include "DimseErrorPayload.h"

#include "../SerializationToolbox.h"

namespace Orthanc
{
  static const char* const DIMSE_ERROR_STATUS = "DimseErrorStatus";  // uint16_t


  Json::Value MakeDimseErrorStatusPayload(uint16_t dimseErrorStatus)
  {
    Json::Value payload;
    payload[DIMSE_ERROR_STATUS] = dimseErrorStatus;
    return payload;
  }


  uint16_t GetDimseErrorStatusFromPayload(const Json::Value& payload)
  {
    unsigned int status = SerializationToolbox::ReadUnsignedInteger(payload, DIMSE_ERROR_STATUS);

    if (status <= 65535)
    {
      return static_cast<uint16_t>(status);
    }
    else
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }
  }
}
