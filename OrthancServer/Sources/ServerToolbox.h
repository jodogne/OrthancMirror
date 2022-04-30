/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

#include "Database/IDatabaseWrapper.h"
#include "ServerEnumerations.h"

#include <boost/noncopyable.hpp>
#include <list>

namespace Orthanc
{
  class ServerContext;
  class IStorageArea;

  namespace ServerToolbox
  {
    bool FindOneChildInstance(int64_t& result,
                              IDatabaseWrapper::ITransaction& transaction,
                              int64_t resource,
                              ResourceType type);

    void ReconstructMainDicomTags(IDatabaseWrapper::ITransaction& transaction,
                                  IStorageArea& storageArea,
                                  ResourceType level);

    void LoadIdentifiers(const DicomTag*& tags,
                         size_t& size,
                         ResourceType level);

    bool IsIdentifier(const DicomTag& tag,
                      ResourceType level);

    std::string NormalizeIdentifier(const std::string& value);

    void ReconstructResource(ServerContext& context,
                             const std::string& resource,
                             bool reconstructFiles);
  }
}
