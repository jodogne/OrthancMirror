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
#include "DicomDataSource.h"

#include "../DicomParsing/ParsedDicomFile.h"
#include "../Logging.h"
#include "../OrthancException.h"
#include "BaseDataIdentifier.h"
#include "DataSourceReader.h"
#include "StorageAreaDataSource.h"

#include <boost/lexical_cast.hpp>


namespace Orthanc
{
  class DicomDataSource::Identifier : public BaseDataIdentifier
  {
  public:
    virtual StorageAreaDataSource::Range* ReadRange(DataSourceReader& reader) const = 0;

    virtual const FileInfo& GetAttachment() const = 0;

    virtual bool IsUntilPixelData() const = 0;
  };


  class DicomDataSource::WholeIdentifier : public DicomDataSource::Identifier
  {
  private:
    FileInfo  attachment_;

  public:
    explicit WholeIdentifier(const FileInfo& attachment) :
      attachment_(attachment)
    {
    }

    virtual bool GetCacheKey(std::string& key) const ORTHANC_OVERRIDE
    {
      key = (attachment_.GetUuid() + "|" +
             boost::lexical_cast<std::string>(attachment_.GetContentType()));
      return true;
    }

    virtual bool EstimateValueSize(size_t& target) const ORTHANC_OVERRIDE
    {
      target = attachment_.GetUncompressedSize();
      return true;
    }

    virtual StorageAreaDataSource::Range* ReadRange(DataSourceReader& reader) const ORTHANC_OVERRIDE
    {
      return StorageAreaDataSource::Execute(
        reader, StorageAreaDataSource::CreateAttachmentRequest(attachment_, true /* uncompress */));
    }

    virtual const FileInfo& GetAttachment() const ORTHANC_OVERRIDE
    {
      return attachment_;
    }

    virtual bool IsUntilPixelData() const ORTHANC_OVERRIDE
    {
      return false;
    }
  };


  class DicomDataSource::BeginningIdentifier : public DicomDataSource::Identifier
  {
  private:
    FileInfo  attachment_;
    bool      hasPixelDataOffset_;
    size_t    pixelDataOffset_;

  public:
    BeginningIdentifier(const FileInfo& attachment,
                        size_t pixelDataOffset) :
      attachment_(attachment),
      hasPixelDataOffset_(true),
      pixelDataOffset_(pixelDataOffset)
    {
    }

    virtual bool GetCacheKey(std::string& key) const ORTHANC_OVERRIDE
    {
      key = (attachment_.GetUuid() + "|" +
             boost::lexical_cast<std::string>(attachment_.GetContentType()) + "|" +
             boost::lexical_cast<std::string>(pixelDataOffset_));
      return true;
    }

    virtual bool EstimateValueSize(size_t& target) const ORTHANC_OVERRIDE
    {
      target = pixelDataOffset_;
      return true;
    }

    virtual StorageAreaDataSource::Range* ReadRange(DataSourceReader& reader) const ORTHANC_OVERRIDE
    {
      return StorageAreaDataSource::Execute(reader, StorageAreaDataSource::CreateBeginningRequest(attachment_, pixelDataOffset_));
    }

    virtual const FileInfo& GetAttachment() const ORTHANC_OVERRIDE
    {
      return attachment_;
    }

    virtual bool IsUntilPixelData() const ORTHANC_OVERRIDE
    {
      return true;
    }
  };


  class DicomDataSource::Value : public IDynamicObject
  {
  private:
    Mutex                             mutex_;
    std::unique_ptr<ParsedDicomFile>  dicom_;
    size_t                            size_;

  public:
    Value(ParsedDicomFile* dicom,
          size_t size) :
      dicom_(dicom),
      size_(size)
    {
      if (dicom == NULL)
      {
        throw OrthancException(ErrorCode_NullPointer);
      }
    }

    size_t GetSize() const
    {
      return size_;
    }

    Mutex::ScopedLock* Lock()
    {
      return new Mutex::ScopedLock(mutex_);
    }

    ParsedDicomFile& GetContent() const
    {
      return *dicom_;
    }
  };


  DicomDataSource::DicomDataSource(const boost::shared_ptr<DataSourceReader>& storageAreaReader) :
    storageAreaReader_(storageAreaReader)
  {
    if (storageAreaReader == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
  }


  IDynamicObject* DicomDataSource::Load(const IDataIdentifier& obj,
                                        const boost::shared_ptr<SharedObjectCache>& readerCache)
  {
    const Identifier& id = dynamic_cast<const Identifier&>(obj);

    std::unique_ptr<StorageAreaDataSource::Range> range(id.ReadRange(*storageAreaReader_));

    const size_t size = range->GetSize();

    {
      // Sanity check
      size_t estimatedSize;
      if (!obj.EstimateValueSize(estimatedSize) ||
          estimatedSize != size)
      {
        THROW_WITH_FILE_AND_LINE_INFO(ErrorCode_InternalError);
      }
    }

    LOG(INFO) << "Parsing DICOM attachment"
              << (id.IsUntilPixelData() ? " (until pixel data): " : ": ")
              << id.GetAttachment().GetUuid();
    std::unique_ptr<Value> value(new Value(new ParsedDicomFile(range->GetData(), size), size));
    assert(GetValueSize(*value) == size);

    return value.release();
  }


  size_t DicomDataSource::GetValueSize(const IDynamicObject& obj) const
  {
    const Value& value = dynamic_cast<const Value&>(obj);
    return value.GetSize();
  }


  DicomDataSource::Dicom::Dicom(DataSourceAnswer::Item* item) :
    item_(item)
  {
    if (item == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
  }


  ParsedDicomFile* DicomDataSource::Dicom::Clone()
  {
    Lock lock(*this);
    return lock.GetContent().Clone(true);
  }


  DicomDataSource::Dicom::Lock::Lock(Dicom& that):
    that_(that)
  {
    Value& value = dynamic_cast<Value&>(*that_.item_->GetValue());
    lock_.reset(value.Lock());
  }


  ParsedDicomFile& DicomDataSource::Dicom::Lock::GetContent() const
  {
    const Value& value = dynamic_cast<const Value&>(*that_.item_->GetValue());
    return value.GetContent();
  }


  IDataIdentifier* DicomDataSource::CreateWholeRequest(const FileInfo& attachment)
  {
    return new WholeIdentifier(attachment);
  }


  IDataIdentifier* DicomDataSource::CreateUntilPixelDataRequest(const FileInfo& attachment,
                                                              uint64_t pixelDataOffset)
  {
    if (static_cast<uint64_t>(static_cast<size_t>(pixelDataOffset)) != pixelDataOffset)
    {
      throw OrthancException(ErrorCode_NotEnoughMemory);
    }
    else if (attachment.GetCompressionType() != CompressionType_None)
    {
      throw OrthancException(ErrorCode_BadParameterType);
    }
    else
    {
      return new BeginningIdentifier(attachment, static_cast<size_t>(pixelDataOffset));
    }
  }


  DicomDataSource::Dicom* DicomDataSource::Execute(DataSourceReader& reader,
                                                   IDataIdentifier* request)
  {
    if (request == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
    else
    {
      std::unique_ptr<DataSourceAnswer::Item> item(reader.ReadSingle(request));
      return new Dicom(item.release());
    }
  }
}
