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
#include "../OrthancException.h"
#include "DataSourceReader.h"
#include "StorageAreaDataSource.h"

#include <boost/lexical_cast.hpp>


namespace Orthanc
{
  static size_t ComputeValueSize(size_t size,
                                 bool isKeepRawBuffer)
  {
    if (isKeepRawBuffer)
    {
      uint64_t s = 2 * static_cast<uint64_t>(size);
      if (s != static_cast<uint64_t>(static_cast<size_t>(s)))
      {
        return std::numeric_limits<size_t>::max();
      }
      else
      {
        return static_cast<size_t>(s);
      }
    }
    else
    {
      return size;
    }
  }


  class DicomDataSource::BaseIdentifier : public IDataIdentifier
  {
  public:
    virtual StorageAreaDataSource::Range* ReadRange(DataSourceReader& reader) const = 0;

    virtual bool IsKeepRawBuffer() const = 0;
  };


  class DicomDataSource::WholeIdentifier : public DicomDataSource::BaseIdentifier
  {
  private:
    FileInfo  attachment_;
    bool      keepRawBuffer_;
    bool      checkMD5_;

  public:
    explicit WholeIdentifier(const FileInfo& attachment,
                             bool keepRawBuffer,
                             bool checkMD5) :
      attachment_(attachment),
      keepRawBuffer_(keepRawBuffer),
      checkMD5_(checkMD5)
    {
    }

    virtual bool GetCacheKey(std::string& key) const ORTHANC_OVERRIDE
    {
      key = (attachment_.GetUuid() + "|" +
             boost::lexical_cast<std::string>(attachment_.GetContentType()) + "|" +
             boost::lexical_cast<std::string>(keepRawBuffer_));
      return true;
    }

    virtual bool EstimateValueSize(size_t& target) const ORTHANC_OVERRIDE
    {
      target = ComputeValueSize(attachment_.GetUncompressedSize(), keepRawBuffer_);
      return true;
    }

    virtual StorageAreaDataSource::Range* ReadRange(DataSourceReader& reader) const ORTHANC_OVERRIDE
    {
      return StorageAreaDataSource::ReadAttachment(reader, attachment_, true /* uncompress */, checkMD5_);
    }

    virtual bool IsKeepRawBuffer() const ORTHANC_OVERRIDE
    {
      return keepRawBuffer_;
    }
  };


  class DicomDataSource::BeginningIdentifier : public DicomDataSource::BaseIdentifier
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
      return StorageAreaDataSource::ReadBeginning(reader, attachment_, pixelDataOffset_);
    }

    virtual bool IsKeepRawBuffer() const ORTHANC_OVERRIDE
    {
      return false;
    }
  };


  class DicomDataSource::Value : public IDynamicObject
  {
  private:
    std::unique_ptr<ParsedDicomFile>   dicom_;
    size_t                             size_;
    bool                               hasRawBuffer_;
    std::string                        rawBuffer_;

  public:
    Value(ParsedDicomFile* dicom,
          size_t size) :
      dicom_(dicom),
      size_(size),
      hasRawBuffer_(false)
    {
      if (dicom == NULL)
      {
        throw OrthancException(ErrorCode_NullPointer);
      }
    }

    ParsedDicomFile& GetContent() const
    {
      return *dicom_;
    }

    size_t GetRawBufferSize() const
    {
      return size_;
    }

    void SetRawBuffer(const void* data,
                      size_t size)
    {
      if (hasRawBuffer_)
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }
      else if (size != size_)
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
      else
      {
        hasRawBuffer_ = true;
        rawBuffer_.assign(reinterpret_cast<const char*>(data), size);
      }
    }

    bool HasRawBuffer() const
    {
      return hasRawBuffer_;
    }

    const void* GetRawBufferData() const
    {
      if (hasRawBuffer_)
      {
        assert(rawBuffer_.size() == size_);
        return rawBuffer_.empty() ? NULL : rawBuffer_.c_str();
      }
      else
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }
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


  IDynamicObject* DicomDataSource::Load(const IDataIdentifier& obj)
  {
    const BaseIdentifier& id = dynamic_cast<const BaseIdentifier&>(obj);

    std::unique_ptr<StorageAreaDataSource::Range> range(id.ReadRange(*storageAreaReader_));

    const size_t size = range->GetSize();

    {
      // Sanity check
      bool ok = false;
      size_t estimatedSize;
      if (obj.EstimateValueSize(estimatedSize))
      {
        ok = (estimatedSize == ComputeValueSize(size, id.IsKeepRawBuffer()));
      }

      if (!ok)
      {
        throw OrthancException(ErrorCode_InternalError);
      }
    }

    std::unique_ptr<Value> value(new Value(new ParsedDicomFile(range->GetData(), size), size));

    if (id.IsKeepRawBuffer())
    {
      value->SetRawBuffer(range->GetData(), size);
      assert(GetValueSize(*value) == 2 * size);
    }
    else
    {
      assert(GetValueSize(*value) == size);
    }

    return value.release();
  }


  size_t DicomDataSource::GetValueSize(const IDynamicObject& obj) const
  {
    const Value& value = dynamic_cast<const Value&>(obj);
    return ComputeValueSize(value.GetRawBufferSize(), value.HasRawBuffer());
  }


  DicomDataSource::Dicom::Dicom(const boost::shared_ptr<IDynamicObject>& value) :
    value_(value)
  {
    if (value.get() == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
  }


  ParsedDicomFile& DicomDataSource::Dicom::Lock::GetContent() const
  {
    return dynamic_cast<const Value&>(*that_.value_).GetContent();
  }


  bool DicomDataSource::Dicom::Lock::HasRawBuffer() const
  {
    return dynamic_cast<const Value&>(*that_.value_).HasRawBuffer();
  }


  const void* DicomDataSource::Dicom::Lock::GetRawBufferData() const
  {
    return dynamic_cast<const Value&>(*that_.value_).GetRawBufferData();
  }


  size_t DicomDataSource::Dicom::Lock::GetRawBufferSize() const
  {
    return dynamic_cast<const Value&>(*that_.value_).GetRawBufferSize();
  }


  DicomDataSource::Dicom* DicomDataSource::ReadWhole(DataSourceReader& reader,
                                                     const FileInfo& attachment,
                                                     bool keepRawBuffer,
                                                     bool checkMD5)
  {
    std::unique_ptr<IDataIdentifier> id(new WholeIdentifier(attachment, keepRawBuffer, checkMD5));
    boost::shared_ptr<IDynamicObject> value = reader.ReadSingle(id.release());
    return new Dicom(value);
  }


  DicomDataSource::Dicom* DicomDataSource::ReadUntilPixelData(DataSourceReader& reader,
                                                              const FileInfo& attachment,
                                                              uint64_t pixelDataOffset)
  {
    if (static_cast<uint64_t>(static_cast<size_t>(pixelDataOffset)) != pixelDataOffset)
    {
      throw OrthancException(ErrorCode_NotEnoughMemory);
    }

    std::unique_ptr<IDataIdentifier> id(new BeginningIdentifier(attachment, static_cast<size_t>(pixelDataOffset)));
    boost::shared_ptr<IDynamicObject> value = reader.ReadSingle(id.release());
    return new Dicom(value);
  }
}
