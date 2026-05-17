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
  class DicomDataSource::BaseIdentifier : public IDataIdentifier
  {
  public:
    virtual StorageAreaDataSource::Range* ReadRange(DataSourceReader& reader) const = 0;
  };


  class DicomDataSource::WholeIdentifier : public DicomDataSource::BaseIdentifier
  {
  private:
    FileInfo  attachment_;
    bool      checkMD5_;

  public:
    explicit WholeIdentifier(const FileInfo& attachment,
                             bool checkMD5) :
      attachment_(attachment),
      checkMD5_(checkMD5)
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
      return StorageAreaDataSource::ReadAttachment(reader, attachment_, true /* uncompress */, checkMD5_);
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
  };


  class DicomDataSource::Value : public IDynamicObject
  {
  private:
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

    ParsedDicomFile& GetDicom() const
    {
      return *dicom_;
    }

    size_t GetSize() const
    {
      return size_;
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

    // Sanity check
    size_t size;
    if (!obj.EstimateValueSize(size) ||
        size != range->GetSize())
    {
      throw OrthancException(ErrorCode_InternalError);
    }

    return new Value(new ParsedDicomFile(range->GetData(), range->GetSize()), range->GetSize());
  }

  size_t DicomDataSource::GetValueSize(const IDynamicObject& obj) const
  {
    const Value& value = dynamic_cast<const Value&>(obj);
    return value.GetSize();
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
    return dynamic_cast<const Value&>(*that_.value_).GetDicom();
  }


  DicomDataSource::Dicom* DicomDataSource::ReadWhole(DataSourceReader& reader,
                                                     const FileInfo& attachment,
                                                     bool checkMD5)
  {
    std::unique_ptr<IDataIdentifier> id(new WholeIdentifier(attachment, checkMD5));
    boost::shared_ptr<IDynamicObject> value = reader.ReadSingle(id.release());
    return new Dicom(value);
  }


  DicomDataSource::Dicom* DicomDataSource::ReadUntilPixelData(DataSourceReader& reader,
                                                              const FileInfo& attachment,
                                                              size_t pixelDataOffset)
  {
    std::unique_ptr<IDataIdentifier> id(new BeginningIdentifier(attachment, pixelDataOffset));
    boost::shared_ptr<IDynamicObject> value = reader.ReadSingle(id.release());
    return new Dicom(value);
  }
}
