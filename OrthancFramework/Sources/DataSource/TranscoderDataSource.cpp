/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2026 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2026 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#include "../PrecompiledHeaders.h"
#include "TranscoderDataSource.h"

#include "../DicomParsing/ParsedDicomFile.h"
#include "../OrthancException.h"
#include "DataSourceReader.h"
#include "StorageAreaDataSource.h"

#include <boost/lexical_cast.hpp>
#include <dcmtk/dcmdata/dcfilefo.h>


namespace Orthanc
{
  static bool IsLosslessTransferSyntax(DicomTransferSyntax syntax)
  {
    return (syntax == DicomTransferSyntax_LittleEndianImplicit ||
            syntax == DicomTransferSyntax_LittleEndianExplicit ||
            syntax == DicomTransferSyntax_DeflatedLittleEndianExplicit ||
            syntax == DicomTransferSyntax_BigEndianExplicit ||
            syntax == DicomTransferSyntax_JPEGLSLossless ||
            syntax == DicomTransferSyntax_JPEG2000LosslessOnly ||
            syntax == DicomTransferSyntax_JPEG2000MulticomponentLosslessOnly ||
            syntax == DicomTransferSyntax_RLELossless ||
            syntax == DicomTransferSyntax_JPEGXLLossless ||
            syntax == DicomTransferSyntax_HighThroughputJPEG2000LosslessOnly);
  }


  class TranscoderDataSource::Value : public IDynamicObject
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

    ParsedDicomFile& GetContent() const
    {
      return *dicom_;
    }

    size_t GetSize() const
    {
      return size_;
    }
  };


  class TranscoderDataSource::Identifier : public IDataIdentifier
  {
  private:
    FileInfo                       attachment_;
    DicomTransferSyntax            targetSyntax_;
    bool                           checkMD5_;
    TranscodingSopInstanceUidMode  mode_;
    bool                           hasLossyQuality_;
    unsigned int                   lossyQuality_;

  public:
    Identifier(const FileInfo& attachment,
               DicomTransferSyntax targetSyntax,
               bool checkMD5) :
      attachment_(attachment),
      targetSyntax_(targetSyntax),
      checkMD5_(checkMD5),
      mode_(TranscodingSopInstanceUidMode_AllowNew),
      hasLossyQuality_(false),
      lossyQuality_(0)
    {
    }

    void SetMode(TranscodingSopInstanceUidMode mode)
    {
      mode_ = mode;
    }

    void SetLossyQuality(unsigned int quality)
    {
      hasLossyQuality_ = true;
      lossyQuality_ = quality;
    }

    virtual bool GetCacheKey(std::string& key) const ORTHANC_OVERRIDE
    {
      key = (attachment_.GetUuid() + "|" +
             GetTransferSyntaxUid(targetSyntax_) + "|" +
             boost::lexical_cast<std::string>(mode_));

      if (hasLossyQuality_ &&
          !IsLosslessTransferSyntax(targetSyntax_))
      {
        key += "|" + boost::lexical_cast<std::string>(lossyQuality_);
      }

      return true;
    }

    virtual bool EstimateValueSize(size_t& target) const ORTHANC_OVERRIDE
    {
      target = attachment_.GetUncompressedSize();
      return true;
    }

    Value* Load(IDicomTranscoder& transcoder,
                DataSourceReader& storageAreaReader) const
    {
      std::unique_ptr<StorageAreaDataSource::Range> range(
        StorageAreaDataSource::ReadAttachment(
          storageAreaReader, attachment_, true /* uncompress */, checkMD5_));

      IDicomTranscoder::DicomImage source;
      source.SetExternalBuffer(range->GetData(), range->GetSize());

      IDicomTranscoder::DicomImage target;

      std::set<DicomTransferSyntax> allowedSyntaxes;
      allowedSyntaxes.insert(targetSyntax_);

      bool success;
      if (hasLossyQuality_)
      {
        success = transcoder.Transcode(target, source, allowedSyntaxes, mode_, lossyQuality_);
      }
      else
      {
        success = transcoder.Transcode(target, source, allowedSyntaxes, mode_);
      }

      if (success)
      {
        const size_t size = target.GetBufferSize();
        return new Value(target.ReleaseAsParsedDicomFile(), size);
      }
      else
      {
        throw OrthancException(ErrorCode_NotImplemented);
      }
    }
  };


  TranscoderDataSource::TranscoderDataSource(const boost::shared_ptr<IDicomTranscoder>& transcoder,
                                             const boost::shared_ptr<DataSourceReader>& storageAreaReader) :
    transcoder_(transcoder),
    storageAreaReader_(storageAreaReader)
  {
    if (!transcoder_ ||
        !storageAreaReader_)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
  }


  IDynamicObject* TranscoderDataSource::Load(const IDataIdentifier& id)
  {
    return dynamic_cast<const Identifier&>(id).Load(*transcoder_, *storageAreaReader_);
  }


  size_t TranscoderDataSource::GetValueSize(const IDynamicObject& value) const
  {
    return dynamic_cast<const Value&>(value).GetSize();
  }


  TranscoderDataSource::Transcoded::Transcoded(const boost::shared_ptr<IDynamicObject>& value) :
    value_(value)
  {
    if (!value)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
  }


  const ParsedDicomFile& TranscoderDataSource::Transcoded::Lock::GetDicom() const
  {
    return dynamic_cast<const Value&>(*that_.value_).GetContent();
  }


  TranscoderDataSource::Transcoded* TranscoderDataSource::Transcode(DataSourceReader& reader,
                                                                    const FileInfo& attachment,
                                                                    DicomTransferSyntax targetSyntax,
                                                                    bool checkMD5)
  {
    std::unique_ptr<IDataIdentifier> id(new Identifier(attachment, targetSyntax, checkMD5));
    boost::shared_ptr<IDynamicObject> value = reader.ReadSingle(id.release());
    return new Transcoded(value);
  }


  TranscoderDataSource::Transcoded* TranscoderDataSource::Transcode(DataSourceReader& reader,
                                                                    const FileInfo& attachment,
                                                                    DicomTransferSyntax targetSyntax,
                                                                    TranscodingSopInstanceUidMode mode,
                                                                    unsigned int lossyQuality,
                                                                    bool checkMD5)
  {
    std::unique_ptr<Identifier> id(new Identifier(attachment, targetSyntax, checkMD5));
    id->SetMode(mode);
    id->SetLossyQuality(lossyQuality);

    boost::shared_ptr<IDynamicObject> value = reader.ReadSingle(id.release());
    return new Transcoded(value);
  }
}
