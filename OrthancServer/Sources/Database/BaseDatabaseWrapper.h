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

#include "IDatabaseWrapper.h"
#include "../../../OrthancFramework/Sources/OrthancException.h"

namespace Orthanc
{
  /**
   * This class provides a default "not implemented" implementation
   * for all recent methods (1.12.X)
   **/
  class BaseDatabaseWrapper : public IDatabaseWrapper
  {
  public:
    class BaseTransaction : public IDatabaseWrapper::ITransaction
    {
      virtual int64_t IncrementGlobalProperty(GlobalProperty property,
                                              int64_t increment,
                                              bool shared) ORTHANC_OVERRIDE
      {
        throw OrthancException(ErrorCode_NotImplemented);  // Not supported
      }

      virtual void UpdateAndGetStatistics(int64_t& patientsCount,
                                          int64_t& studiesCount,
                                          int64_t& seriesCount,
                                          int64_t& instancesCount,
                                          int64_t& compressedSize,
                                          int64_t& uncompressedSize) ORTHANC_OVERRIDE
      {
        throw OrthancException(ErrorCode_NotImplemented);  // Not supported
      }

    };

  };
}