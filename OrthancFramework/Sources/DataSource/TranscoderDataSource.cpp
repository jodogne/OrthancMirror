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
#include "../Logging.h"
#include "../OrthancException.h"
#include "BaseDataIdentifier.h"
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
    Mutex                             parsedMutex_;
    std::unique_ptr<ParsedDicomFile>  parsed_;
    std::unique_ptr<std::string>      raw_;
    size_t                            size_;

  public:
    explicit Value(IDicomTranscoder::DicomImage* transcoded)
    {
      std::unique_ptr<IDicomTranscoder::DicomImage> protection(transcoded);

      if (transcoded == NULL)
      {
        throw OrthancException(ErrorCode_NullPointer);
      }

      if (!transcoded->HasParsed() &&
          !transcoded->HasInternalBuffer())
      {
        throw OrthancException(ErrorCode_InternalError, "No transcoded image is available");
      }

      /**
       * Note that both "transcoded->HasParsed()" and
       * "transcoded->HasInternalBuffer()" could be true in debug
       * mode, because of "IDicomTranscoder::CheckTranscoding()".
       **/

      if (transcoded->HasParsed())
      {
        parsed_.reset(transcoded->ReleaseAsParsedDicomFile());
      }

      if (transcoded->HasInternalBuffer())
      {
        raw_.reset(transcoded->ReleaseInternalBuffer());
        size_ = raw_->size();  // This is more accurate than "f.calcElementLength()" below
      }
      else
      {
        assert(parsed_.get() != NULL);
        DcmFileFormat& f = parsed_->GetDcmtkObject();
        size_ = f.calcElementLength(f.getDataset()->getOriginalXfer(), EET_ExplicitLength);
      }
    }

    size_t GetSize() const
    {
      return size_;
    }

    bool HasParsed() const
    {
      return parsed_.get() != NULL;
    }

    bool HasRawBuffer() const
    {
      return raw_.get() != NULL;
    }

    Mutex::ScopedLock* LockParsed()
    {
      return new Mutex::ScopedLock(parsedMutex_);
    }

    ParsedDicomFile& GetParsed() const
    {
      if (parsed_.get() == NULL)
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }
      else
      {
        return *parsed_;
      }
    }

    const std::string& GetRawBuffer() const
    {
      if (raw_.get() == NULL)
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }
      else
      {
        return *raw_;
      }
    }
  };


  class TranscoderDataSource::Identifier : public BaseDataIdentifier
  {
  private:
    FileInfo                       attachment_;
    DicomTransferSyntax            targetSyntax_;
    TranscodingSopInstanceUidMode  mode_;
    bool                           hasLossyQuality_;
    unsigned int                   lossyQuality_;

  public:
    Identifier(const FileInfo& attachment,
               DicomTransferSyntax targetSyntax,
               TranscodingSopInstanceUidMode mode) :
      attachment_(attachment),
      targetSyntax_(targetSyntax),
      mode_(mode),
      hasLossyQuality_(false),
      lossyQuality_(0)
    {
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

      if (hasLossyQuality_)
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
        StorageAreaDataSource::Execute(
          storageAreaReader, StorageAreaDataSource::CreateAttachmentRequest(attachment_, true /* uncompress */)));

      IDicomTranscoder::DicomImage source;
      source.SetExternalBuffer(range->GetData(), range->GetSize());

      std::unique_ptr<IDicomTranscoder::DicomImage> target(new IDicomTranscoder::DicomImage);

      std::set<DicomTransferSyntax> allowedSyntaxes;
      allowedSyntaxes.insert(targetSyntax_);

      LOG(INFO) << "Transcoding DICOM attachment to " << GetTransferSyntaxUid(targetSyntax_) << ": " << attachment_.GetUuid();

      bool success;
      if (hasLossyQuality_)
      {
        success = transcoder.Transcode(*target, source, allowedSyntaxes, mode_, lossyQuality_);
      }
      else
      {
        success = transcoder.Transcode(*target, source, allowedSyntaxes, mode_);
      }

      if (success)
      {
        return new Value(target.release());
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


  TranscoderDataSource::Transcoded::Transcoded(DataSourceAnswer::Item* item) :
    item_(item)
  {
    if (!item)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
  }


  TranscoderDataSource::Transcoded::LockAsParsed::LockAsParsed(Transcoded& that)
  {
    Value& value = dynamic_cast<Value&>(*that.item_->GetValue());

    if (value.HasParsed())
    {
      lock_.reset(value.LockParsed());
      content_ = &value.GetParsed();
    }
    else
    {
      assert(value.HasRawBuffer());
      parsed_.reset(new ParsedDicomFile(value.GetRawBuffer()));

      content_ = parsed_.get();
    }

    assert(content_ != NULL);
  }


  TranscoderDataSource::Transcoded::LockAsBuffer::LockAsBuffer(Transcoded& that)
  {
    Value& value = dynamic_cast<Value&>(*that.item_->GetValue());

    if (value.HasParsed())
    {
      std::unique_ptr<Mutex::ScopedLock> lock(value.LockParsed());
      serialized_.reset(new std::string);
      value.GetParsed().SaveToMemoryBuffer(*serialized_);
      content_ = serialized_.get();
    }
    else
    {
      assert(value.HasRawBuffer());
      content_ = &value.GetRawBuffer();
    }

    assert(content_ != NULL);
  }


  IDataIdentifier* TranscoderDataSource::CreateRequest(const FileInfo& attachment,
                                                       DicomTransferSyntax targetSyntax,
                                                       TranscodingSopInstanceUidMode mode,
                                                       bool hasLossyQuality,
                                                       unsigned int lossyQuality)
  {
    std::unique_ptr<Identifier> id(new Identifier(attachment, targetSyntax, mode));

    if (hasLossyQuality &&
        !IsLosslessTransferSyntax(targetSyntax))
    {
      id->SetLossyQuality(lossyQuality);
    }

    return id.release();
  }


  TranscoderDataSource::Transcoded* TranscoderDataSource::Execute(DataSourceReader& reader,
                                                                  IDataIdentifier* request)
  {
    if (request == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
    else
    {
      std::unique_ptr<DataSourceAnswer::Item> item(reader.ReadSingle(request));
      return new Transcoded(item.release());
    }
  }
}
