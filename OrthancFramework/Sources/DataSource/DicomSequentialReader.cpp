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
#include "DicomSequentialReader.h"

#include "../DicomParsing/ParsedDicomFile.h"
#include "../OrthancException.h"
#include "../StringMemoryBuffer.h"
#include "BaseDataIdentifier.h"
#include "DicomDataSource.h"
#include "StorageAreaDataSource.h"
#include "TranscoderDataSource.h"


namespace Orthanc
{
  DicomSequentialReader::Item::Item(ParsedDicomFile* parsed) :
    parsed_(parsed)
  {
    if (parsed == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
  }


  DicomSequentialReader::Item::Item(const boost::shared_ptr<IMemoryBuffer>& raw) :
    raw_(raw)
  {
    if (raw.get() == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
  }


  DicomSequentialReader::Item::Item(ParsedDicomFile* parsed /* takes ownership */,
                                    IMemoryBuffer* raw /* takes ownership */) :
    parsed_(parsed),
    raw_(raw)
  {
    if (parsed == NULL ||
        raw == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
  }


  ParsedDicomFile& DicomSequentialReader::Item::GetParsedDicomFile()
  {
    if (parsed_.get() != NULL)
    {
      return *parsed_;
    }
    else if (raw_.get() != NULL)
    {
      parsed_.reset(new ParsedDicomFile(raw_->GetData(), raw_->GetSize()));
      return *parsed_;
    }
    else
    {
      throw OrthancException(ErrorCode_InternalError);
    }
  }


  const IMemoryBuffer& DicomSequentialReader::Item::GetRawMemoryBuffer()
  {
    if (raw_.get() != NULL)
    {
      return *raw_;
    }
    else if (parsed_.get() != NULL)
    {
      std::string s;
      parsed_->SaveToMemoryBuffer(s);

      raw_.reset(StringMemoryBuffer::CreateFromSwap(s));
      return *raw_;
    }
    else
    {
      throw OrthancException(ErrorCode_InternalError);
    }
  }


  void DicomSequentialReader::Item::SetUserData(IDynamicObject* userData /* takes ownership */)
  {
    if (userData == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
    else
    {
      userData_.reset(userData);
    }
  }


  const IDynamicObject& DicomSequentialReader::Item::GetUserData() const
  {
    if (userData_.get() == NULL)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      return *userData_;
    }
  }


  class DicomSequentialReader::Disconnector : public DataSourceSequentialReader::IValueDisconnector
  {
  private:
    SourceType  sourceType_;

  public:
    Disconnector(SourceType sourceType) :
      sourceType_(sourceType)
    {
    }

    virtual IDynamicObject* Apply(DataSourceAnswer::Item* source) ORTHANC_OVERRIDE
    {
      std::unique_ptr<DataSourceAnswer::Item> protection(source);

      switch (sourceType_)
      {
        case SourceType_StorageArea:
        {
          std::unique_ptr<StorageAreaDataSource::Range> range(new StorageAreaDataSource::Range(protection.release()));
          return new Item(range->GetBuffer());
        }

        case SourceType_Dicom:
        {
          std::unique_ptr<DicomDataSource::Dicom> dicom(new DicomDataSource::Dicom(protection.release()));
          return new Item(dicom->Clone());
        }

        case SourceType_Transcoder:
        {
          std::unique_ptr<TranscoderDataSource::Transcoded> dicom(new TranscoderDataSource::Transcoded(protection.release()));
          TranscoderDataSource::Transcoded::LockAsParsed lock(*dicom);
          return new Item(lock.GetContent().Clone(true));
        }

        default:
          throw OrthancException(ErrorCode_InternalError);
      }
    }
  };


  DicomSequentialReader::DicomSequentialReader(SourceType sourceType,
                                               const boost::shared_ptr<IExecutorService>& executor,
                                               DicomTransferSyntax targetSyntax,
                                               TranscodingSopInstanceUidMode mode,
                                               bool hasLossyQuality,
                                               unsigned int lossyQuality,
                                               const boost::shared_ptr<DataSourceReader>& reader,
                                               unsigned int windowSize,
                                               uint64_t windowCapacity) :
    sourceType_(sourceType),
    targetSyntax_(targetSyntax),
    mode_(mode),
    hasLossyQuality_(hasLossyQuality),
    lossyQuality_(lossyQuality),
    reader_(executor, reader, new Disconnector(sourceType), windowSize, windowCapacity)
  {
  }


  void DicomSequentialReader::SubmitInternal(const FileInfo& attachment,
                                             IDynamicObject* userData)
  {
    std::unique_ptr<IDynamicObject> protection(userData);

    std::unique_ptr<IDataIdentifier> id;

    switch (sourceType_)
    {
      case SourceType_StorageArea:
        id.reset(StorageAreaDataSource::CreateAttachmentRequest(attachment, true /* uncompress */));
        break;

      case SourceType_Dicom:
        id.reset(DicomDataSource::CreateWholeRequest(attachment));
        break;

      case SourceType_Transcoder:
        id.reset(TranscoderDataSource::CreateRequest(attachment, targetSyntax_, mode_, hasLossyQuality_, lossyQuality_));
        break;

      default:
        throw OrthancException(ErrorCode_InternalError);
    }

    if (protection.get() != NULL)
    {
      dynamic_cast<BaseDataIdentifier&>(*id).SetUserData(protection.release());
    }

    reader_.Submit(id.release());
  }


  DicomSequentialReader* DicomSequentialReader::CreateForOriginal(const boost::shared_ptr<IExecutorService>& executor,
                                                                  ExpectedAnswer expectedAnswer,
                                                                  const boost::shared_ptr<DataSourceReader>& dicomReader,
                                                                  unsigned int windowSize,
                                                                  uint64_t windowCapacity)
  {
    switch (expectedAnswer)
    {
      case ExpectedAnswer_ParsedDicomFile:
        return new DicomSequentialReader(
          SourceType_Dicom, executor,
          DicomTransferSyntax_LittleEndianImplicit, TranscodingSopInstanceUidMode_AllowNew,  /* dummy values */
          false /* no lossy */, 0 /* unused lossy quality */,
          dicomReader, windowSize, windowCapacity);

      case ExpectedAnswer_RawMemoryBuffer:
        return new DicomSequentialReader(
          SourceType_StorageArea, executor,
          DicomTransferSyntax_LittleEndianImplicit, TranscodingSopInstanceUidMode_AllowNew,  /* dummy values */
          false /* no lossy */, 0 /* unused lossy quality */,
          dicomReader, windowSize, windowCapacity);

      default:
        throw OrthancException(ErrorCode_NotImplemented);
    }
  }


  DicomSequentialReader* DicomSequentialReader::CreateForTranscoded(const boost::shared_ptr<IExecutorService>& executor,
                                                                    ExpectedAnswer expectedAnswer,
                                                                    DicomTransferSyntax targetSyntax,
                                                                    TranscodingSopInstanceUidMode mode,
                                                                    bool hasLossyQuality,
                                                                    unsigned int lossyQuality,
                                                                    const boost::shared_ptr<DataSourceReader>& transcoderReader,
                                                                    unsigned int windowSize,
                                                                    uint64_t windowCapacity)
  {
    switch (expectedAnswer)
    {
      case ExpectedAnswer_RawMemoryBuffer:
        // TODO

      case ExpectedAnswer_ParsedDicomFile:
        return new DicomSequentialReader(
          SourceType_Transcoder, executor,
          targetSyntax, mode, hasLossyQuality, lossyQuality,
          transcoderReader, windowSize, windowCapacity);

      default:
        throw OrthancException(ErrorCode_NotImplemented);
    }
  }


  void DicomSequentialReader::Submit(const FileInfo& attachment)
  {
    SubmitInternal(attachment, NULL);
  }


  void DicomSequentialReader::Submit(const FileInfo& attachment,
                                     IDynamicObject* userData)
  {
    if (userData == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }

    SubmitInternal(attachment, userData);
  }


  void DicomSequentialReader::Start()
  {
    reader_.Start();
  }


  bool DicomSequentialReader::HasNext()
  {
    return reader_.HasNext();
  }


  DicomSequentialReader::Item* DicomSequentialReader::Next()
  {
    std::unique_ptr<DataSourceSequentialReader::Item> source(reader_.Next());

    std::unique_ptr<Item> target(dynamic_cast<Item*>(source->ReleaseValue()));

    if (source->HasUserData())
    {
      target->SetUserData(source->ReleaseUserData());
    }

    return target.release();
  }
}
