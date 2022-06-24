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


#include "PrecompiledHeadersServer.h"
#include "DicomInstanceToStore.h"

#include "OrthancConfiguration.h"

#include "../../OrthancFramework/Sources/DicomParsing/FromDcmtkBridge.h"
#include "../../OrthancFramework/Sources/DicomParsing/Internals/DicomFrameIndex.h"
#include "../../OrthancFramework/Sources/DicomParsing/Internals/DicomImageDecoder.h"
#include "../../OrthancFramework/Sources/DicomParsing/ParsedDicomFile.h"
#include "../../OrthancFramework/Sources/Logging.h"
#include "../../OrthancFramework/Sources/OrthancException.h"

#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcdeftag.h>


namespace Orthanc
{
  class DicomInstanceToStore::FromBuffer : public DicomInstanceToStore
  {
  private:
    const void*                       buffer_;
    size_t                            size_;
    std::unique_ptr<ParsedDicomFile>  parsed_;

  public:
    FromBuffer(const void* buffer,
               size_t size) :
      buffer_(buffer),
      size_(size)
    {
    }

    virtual ParsedDicomFile& GetParsedDicomFile() const ORTHANC_OVERRIDE
    {
      if (parsed_.get() == NULL)
      {
        const_cast<FromBuffer&>(*this).parsed_.reset(new ParsedDicomFile(buffer_, size_));
      }

      return *parsed_;
    }

    virtual const void* GetBufferData() const ORTHANC_OVERRIDE
    {
      return buffer_;
    }

    virtual size_t GetBufferSize() const ORTHANC_OVERRIDE
    {
      return size_;
    }
  };

    
  class DicomInstanceToStore::FromParsedDicomFile : public DicomInstanceToStore
  {
  private:
    ParsedDicomFile&              parsed_;
    std::unique_ptr<std::string>  buffer_;

    void SerializeToBuffer()
    {
      if (buffer_.get() == NULL)
      {
        buffer_.reset(new std::string);
        parsed_.SaveToMemoryBuffer(*buffer_);
      }
    }

  public:
    explicit FromParsedDicomFile(ParsedDicomFile& parsed) :
      parsed_(parsed)
    {
    }

    virtual ParsedDicomFile& GetParsedDicomFile() const ORTHANC_OVERRIDE
    {
      return parsed_;
    }

    virtual const void* GetBufferData() const ORTHANC_OVERRIDE
    {
      const_cast<FromParsedDicomFile&>(*this).SerializeToBuffer();

      assert(buffer_.get() != NULL);
      return (buffer_->empty() ? NULL : buffer_->c_str());
    }

    virtual size_t GetBufferSize() const ORTHANC_OVERRIDE
    {
      const_cast<FromParsedDicomFile&>(*this).SerializeToBuffer();

      assert(buffer_.get() != NULL);
      return buffer_->size();
    }    
  };


  class DicomInstanceToStore::FromDcmDataset : public DicomInstanceToStore
  {
  private:
    DcmDataset&                       dataset_;
    std::unique_ptr<std::string>      buffer_;
    std::unique_ptr<ParsedDicomFile>  parsed_;

    void SerializeToBuffer()
    {
      if (buffer_.get() == NULL)
      {
        buffer_.reset(new std::string);
        
        if (!FromDcmtkBridge::SaveToMemoryBuffer(*buffer_, dataset_))
        {
          throw OrthancException(ErrorCode_InternalError, "Cannot write DICOM file to memory");
        }
      }
    }

  public:
    explicit FromDcmDataset(DcmDataset& dataset) :
      dataset_(dataset)
    {
    }
    
    virtual ParsedDicomFile& GetParsedDicomFile() const ORTHANC_OVERRIDE
    {
      if (parsed_.get() == NULL)
      {
        // This operation is costly, as it creates a clone of the
        // dataset. This explains why the default implementations
        // are overridden below to use "dataset_" as much as possible
        const_cast<FromDcmDataset&>(*this).parsed_.reset(new ParsedDicomFile(dataset_));
      }

      return *parsed_;
    }

    virtual const void* GetBufferData() const ORTHANC_OVERRIDE
    {
      const_cast<FromDcmDataset&>(*this).SerializeToBuffer();

      assert(buffer_.get() != NULL);
      return (buffer_->empty() ? NULL : buffer_->c_str());
    }

    virtual size_t GetBufferSize() const ORTHANC_OVERRIDE
    {
      const_cast<FromDcmDataset&>(*this).SerializeToBuffer();

      assert(buffer_.get() != NULL);
      return buffer_->size();
    }

    virtual bool HasPixelData() const ORTHANC_OVERRIDE
    {
      DcmTag key(DICOM_TAG_PIXEL_DATA.GetGroup(),
                 DICOM_TAG_PIXEL_DATA.GetElement());
      return dataset_.tagExists(key);
    }

    virtual void GetSummary(DicomMap& summary) const ORTHANC_OVERRIDE
    {
      OrthancConfiguration::DefaultExtractDicomSummary(summary, dataset_);
    }

    virtual void GetDicomAsJson(Json::Value& dicomAsJson, const std::set<DicomTag>& ignoreTagLength) const ORTHANC_OVERRIDE
    {
      OrthancConfiguration::DefaultDicomDatasetToJson(dicomAsJson, dataset_, ignoreTagLength);
    }

    virtual void DatasetToJson(Json::Value& target, 
                               DicomToJsonFormat format,
                               DicomToJsonFlags flags,
                               unsigned int maxStringLength) const ORTHANC_OVERRIDE
    {
      std::set<DicomTag> ignoreTagLength;
      FromDcmtkBridge::ExtractDicomAsJson(
        target, dataset_, format, flags, maxStringLength, ignoreTagLength);
    }

    virtual unsigned int GetFramesCount() const ORTHANC_OVERRIDE
    {
      return DicomFrameIndex::GetFramesCount(dataset_);
    }
    
    virtual ImageAccessor* DecodeFrame(unsigned int frame) const ORTHANC_OVERRIDE
    {
      return DicomImageDecoder::Decode(dataset_, frame);
    }
  };

  
  DicomInstanceToStore* DicomInstanceToStore::CreateFromBuffer(const void* buffer,
                                                               size_t size)
  {
    return new FromBuffer(buffer, size);
  }

  
  DicomInstanceToStore* DicomInstanceToStore::CreateFromBuffer(const std::string& buffer)
  {
    return new FromBuffer(buffer.empty() ? NULL : buffer.c_str(), buffer.size());
  }


  DicomInstanceToStore* DicomInstanceToStore::CreateFromParsedDicomFile(ParsedDicomFile& dicom)
  {
    return new FromParsedDicomFile(dicom);
  }

  
  DicomInstanceToStore* DicomInstanceToStore::CreateFromDcmDataset(DcmDataset& dataset)
  {
    return new FromDcmDataset(dataset);
  }

  
  bool DicomInstanceToStore::LookupTransferSyntax(DicomTransferSyntax& result) const
  {
    DicomMap header;
    if (DicomMap::ParseDicomMetaInformation(header, GetBufferData(), GetBufferSize()))
    {
      const DicomValue* value = header.TestAndGetValue(DICOM_TAG_TRANSFER_SYNTAX_UID);
      if (value != NULL &&
          !value->IsBinary() &&
          !value->IsNull())
      {
        return ::Orthanc::LookupTransferSyntax(result, Toolbox::StripSpaces(value->GetContent()));
      }
    }
    else
    {
      // This is a DICOM file without a proper meta-header. Fallback
      // to DCMTK, which will fully parse the dataset to retrieve
      // the transfer syntax. Added in Orthanc 1.8.2.
      return GetParsedDicomFile().LookupTransferSyntax(result);
    }
    
    return false;
  }


  bool DicomInstanceToStore::HasPixelData() const
  {
    return GetParsedDicomFile().HasTag(DICOM_TAG_PIXEL_DATA);
  }

  
  void DicomInstanceToStore::GetSummary(DicomMap& summary) const
  {
    OrthancConfiguration::DefaultExtractDicomSummary(summary, GetParsedDicomFile());
  }

  
  void DicomInstanceToStore::GetDicomAsJson(Json::Value& dicomAsJson, const std::set<DicomTag>& ignoreTagLength) const
  {
    OrthancConfiguration::DefaultDicomDatasetToJson(dicomAsJson, GetParsedDicomFile(), ignoreTagLength);
  }


  void DicomInstanceToStore::DatasetToJson(Json::Value& target, 
                                           DicomToJsonFormat format,
                                           DicomToJsonFlags flags,
                                           unsigned int maxStringLength) const
  {
    return GetParsedDicomFile().DatasetToJson(target, format, flags, maxStringLength);
  }


  unsigned int DicomInstanceToStore::GetFramesCount() const
  {
    return GetParsedDicomFile().GetFramesCount();
  }

  
  ImageAccessor* DicomInstanceToStore::DecodeFrame(unsigned int frame) const
  {
    return GetParsedDicomFile().DecodeFrame(frame);
  }

  void DicomInstanceToStore::CopyMetadata(const DicomInstanceToStore::MetadataMap& metadata)
  {
    for (MetadataMap::const_iterator it = metadata.begin(); 
         it != metadata.end(); ++it)
    {
      AddMetadata(it->first.first, it->first.second, it->second);
    }
  }

}
