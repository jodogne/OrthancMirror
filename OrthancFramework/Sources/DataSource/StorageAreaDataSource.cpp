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


#include "../PrecompiledHeaders.h"
#include "StorageAreaDataSource.h"

#include "../OrthancException.h"
#include "DataSourceReader.h"

#include <boost/lexical_cast.hpp>
#include <cassert>


namespace Orthanc
{
  class StorageAreaDataSource::Value : public IDynamicObject
  {
  private:
    std::unique_ptr<IMemoryBuffer>  buffer_;

  public:
    Value(IMemoryBuffer* buffer) :
      buffer_(buffer)
    {
      if (buffer == NULL)
      {
        throw OrthancException(ErrorCode_NullPointer);
      }
    }

    const IMemoryBuffer& GetBuffer() const
    {
      assert(buffer_.get() != NULL);
      return *buffer_;
    }
  };


  class StorageAreaDataSource::Identifier : public IDataIdentifier
  {
  private:
    std::string      uuid_;
    FileContentType  type_;
    uint64_t         start_;
    uint64_t         end_;
    std::string      customData_;

  public:
    Identifier(const std::string& uuid,
               FileContentType type,
               uint64_t start,
               uint64_t end,
               const std::string& customData) :
      uuid_(uuid),
      type_(type),
      start_(start),
      end_(end),
      customData_(customData)
    {
    }

    Value* Read(IPluginStorageArea& area) const
    {
      return new Value(area.ReadRange(uuid_, type_, start_, end_, customData_));
    }

    bool GetCacheKey(std::string& key) const
    {
      // custom data is not part of the cache key, as it is for internal use by the plugin
      // (it is opaque to the Orthanc core)
      key = (uuid_ + "|" +
             boost::lexical_cast<std::string>(type_) + "|" +
             boost::lexical_cast<std::string>(start_) + "|" +
             boost::lexical_cast<std::string>(end_));
      return true;
    }
  };


  StorageAreaDataSource::StorageAreaDataSource(IPluginStorageArea* area) :
    area_(area)
  {
    if (area == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
  }


  IPluginStorageArea& StorageAreaDataSource::GetArea() const
  {
    assert(area_.get() != NULL);
    return *area_;
  }

  size_t StorageAreaDataSource::GetSize(const IDynamicObject& object) const
  {
    const Value& value = dynamic_cast<const Value&>(object);
    return value.GetBuffer().GetSize();
  }

  IDynamicObject* StorageAreaDataSource::Load(const IDataIdentifier& identifier)
  {
    const Identifier& id = dynamic_cast<const Identifier&>(identifier);
    return id.Read(*area_);
  }


  const StorageAreaDataSource::Value& StorageAreaDataSource::Range::GetValue() const
  {
    assert(value_.get() != NULL);
    return dynamic_cast<const StorageAreaDataSource::Value&>(*value_);
  }


  StorageAreaDataSource::Range::Range(const boost::shared_ptr<IDynamicObject>& value) :
    value_(value)
  {
    if (value_.get() == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
  }


  const void* StorageAreaDataSource::Range::GetData() const
  {
    return GetValue().GetBuffer().GetData();
  }


  size_t StorageAreaDataSource::Range::GetSize() const
  {
    return GetValue().GetBuffer().GetSize();
  }


  StorageAreaDataSource::Range* StorageAreaDataSource::ReadRange(DataSourceReader& reader,
                                                                 const std::string& uuid,
                                                                 FileContentType type,
                                                                 uint64_t start,
                                                                 uint64_t end,
                                                                 const std::string& customData)
  {
    std::unique_ptr<IDataIdentifier> id(new StorageAreaDataSource::Identifier(uuid, type, start, end, customData));
    boost::shared_ptr<IDynamicObject> value = reader.ReadSingle(id.release());
    return new Range(value);
  }
}
