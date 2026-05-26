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

#if !defined(ORTHANC_ENABLE_ZLIB)
#  error The macro ORTHANC_ENABLE_ZLIB must be defined
#endif

#include "../Compatibility.h"
#include "../FileStorage/FileInfo.h"
#include "../FileStorage/IStorageArea.h"
#include "../FileStorage/StorageRange.h"
#include "../IMemoryBuffer.h"
#include "DataSourceAnswer.h"
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
    class IPostProcessing;
    class AttachmentPostProcessing;

  private:
    IPluginStorageArea&  area_;
    bool                 checkMD5_;

  public:
    explicit StorageAreaDataSource(IPluginStorageArea& area,
                                   bool checkMD5) :
      area_(area),
      checkMD5_(checkMD5)
    {
    }

    virtual size_t GetValueSize(const IDynamicObject& value) const ORTHANC_OVERRIDE;

    virtual IDynamicObject* Load(const IDataIdentifier& identifier) ORTHANC_OVERRIDE;

    class Range : public boost::noncopyable
    {
    private:
      std::unique_ptr<DataSourceAnswer::Item>  item_;   // Holding item puts backpressure on the data source (can be NULL)
      boost::shared_ptr<IMemoryBuffer>         buffer_;  // To be used if "item_ == NULL"

    public:
      explicit Range(DataSourceAnswer::Item* item /* takes ownership */);

      explicit Range(IMemoryBuffer* buffer /* takes ownership */);

      const boost::shared_ptr<IMemoryBuffer>& GetBuffer() const;

      const void* GetData() const
      {
        return GetBuffer()->GetData();
      }

      const size_t GetSize() const
      {
        return GetBuffer()->GetSize();
      }

      void Copy(std::string& target) const
      {
        GetBuffer()->CopyToString(target);
      }
    };

    static IDataIdentifier* CreateRangeRequest(const std::string& uuid,
                                               FileContentType type,
                                               uint64_t start /* inclusive */,
                                               uint64_t end /* exclusive */,
                                               const std::string& pluginCustomData);

    static IDataIdentifier* CreateAttachmentRequest(const FileInfo& attachment,
                                                    bool uncompress);

    static IDataIdentifier* CreateBeginningRequest(const FileInfo& attachment,
                                                   uint64_t untilPosition /* exclusive */);

    static Range* Execute(DataSourceReader& reader,
                          IDataIdentifier* request /* takes ownership */);

    static Range* ReadRange(DataSourceReader& reader,
                            const FileInfo& attachment,
                            const StorageRange& range,
                            bool uncompress);
  };
}
