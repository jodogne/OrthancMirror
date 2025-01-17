/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2025 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2025 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

#include "../../OrthancFramework/Sources/DicomFormat/DicomMap.h"
#include "../../OrthancFramework/Sources/FileStorage/FileInfo.h"

namespace Orthanc
{
  class ServerIndex;

  class SimpleInstanceOrdering
  {
  private:
    struct Instance;

    std::vector<Instance*>   instances_;
    bool                     hasDuplicateIndexInSeries_;

  public:
    SimpleInstanceOrdering(ServerIndex& index,
                           const std::string& seriesId);
    ~SimpleInstanceOrdering();

    size_t GetInstancesCount() const
    {
      return instances_.size();
    }

    const std::string& GetInstanceId(size_t index) const;

    uint32_t GetInstanceIndexInSeries(size_t index) const;

    const FileInfo& GetInstanceFileInfo(size_t index) const;

  private:
    static bool IndexInSeriesComparator(const SimpleInstanceOrdering::Instance* a,
                                        const SimpleInstanceOrdering::Instance* b);
  };
}
