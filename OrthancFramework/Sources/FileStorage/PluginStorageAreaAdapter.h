/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2026 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2026 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#pragma once

#include "IStorageArea.h"


namespace Orthanc
{
  class ORTHANC_PUBLIC PluginStorageAreaAdapter : public IPluginStorageArea
  {
  private:
    std::unique_ptr<IStorageArea> storage_;

  public:
    explicit PluginStorageAreaAdapter(IStorageArea* storage /* takes ownership */);

    virtual void Create(std::string& customData,
                        const std::string& uuid,
                        const void* content,
                        size_t size,
                        FileContentType type,
                        CompressionType compression,
                        const DicomInstanceToStore* dicomInstance) ORTHANC_OVERRIDE;

    virtual IMemoryBuffer* ReadRange(const std::string& uuid,
                                     FileContentType type,
                                     uint64_t start /* inclusive */,
                                     uint64_t end /* exclusive */,
                                     const std::string& customData) ORTHANC_OVERRIDE
    {
      return storage_->ReadRange(uuid, type, start, end);
    }

    virtual void Remove(const std::string& uuid,
                        FileContentType type,
                        const std::string& customData) ORTHANC_OVERRIDE
    {
      storage_->Remove(uuid, type);
    }

    virtual bool HasEfficientReadRange() const ORTHANC_OVERRIDE
    {
      return storage_->HasEfficientReadRange();
    }
  };
}
