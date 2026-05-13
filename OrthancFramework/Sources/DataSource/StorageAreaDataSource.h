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

#include "../Compatibility.h"
#include "../FileStorage/IStorageArea.h"
#include "IDataSource.h"

#include <boost/shared_ptr.hpp>


namespace Orthanc
{
  class DataSourceReader;

  class ORTHANC_PUBLIC StorageAreaDataSource : public IDataSource
  {
  private:
    class Value;
    class Identifier;

  private:
    std::unique_ptr<IPluginStorageArea>  area_;

  public:
    StorageAreaDataSource(IPluginStorageArea* area);

    IPluginStorageArea& GetArea() const;

    virtual size_t GetValueSize(const IDynamicObject& value) const ORTHANC_OVERRIDE;

    virtual IDynamicObject* Load(const IDataIdentifier& identifier) ORTHANC_OVERRIDE;

    class Range : public boost::noncopyable
    {
    private:
      boost::shared_ptr<IDynamicObject>  value_;

      const Value& GetValue() const;

    public:
      Range(const boost::shared_ptr<IDynamicObject>& value);

      const void* GetData() const;

      size_t GetSize() const;
    };

    static Range* ReadRange(DataSourceReader& reader,
                            const std::string& uuid,
                            FileContentType type,
                            uint64_t start /* inclusive */,
                            uint64_t end /* exclusive */,
                            const std::string& customData);
  };
}
