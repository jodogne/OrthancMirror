/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2024 Osimis S.A., Belgium
 * Copyright (C) 2021-2024 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#include "BaseDatabaseWrapper.h"

#include "../../../OrthancFramework/Sources/OrthancException.h"

namespace Orthanc
{
  int64_t BaseDatabaseWrapper::BaseTransaction::IncrementGlobalProperty(GlobalProperty property,
                                                                        int64_t increment,
                                                                        bool shared)
  {
    throw OrthancException(ErrorCode_NotImplemented);  // Not supported
  }


  void BaseDatabaseWrapper::BaseTransaction::UpdateAndGetStatistics(int64_t& patientsCount,
                                                                    int64_t& studiesCount,
                                                                    int64_t& seriesCount,
                                                                    int64_t& instancesCount,
                                                                    int64_t& compressedSize,
                                                                    int64_t& uncompressedSize)
  {
    throw OrthancException(ErrorCode_NotImplemented);  // Not supported
  }
}
