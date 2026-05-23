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
#include "BaseDataIdentifier.h"
#include "DicomDataSource.h"
#include "TranscoderDataSource.h"


namespace Orthanc
{
  class DicomSequentialReader::Disconnector : public DataSourceSequentialReader::IValueDisconnector
  {
  private:
    Type  type_;

  public:
    Disconnector(Type type) :
      type_(type)
    {
    }

    virtual IDynamicObject* Apply(DataSourceAnswer::Item* source)
    {
      switch (type_)
      {
        case Type_Original:
        {
          std::unique_ptr<DicomDataSource::Dicom> dicom(new DicomDataSource::Dicom(source));
          return dicom->Clone();
        }

        case Type_Transcoded:
        {
          std::unique_ptr<TranscoderDataSource::Transcoded> dicom(new TranscoderDataSource::Transcoded(source));
          TranscoderDataSource::Transcoded::LockAsParsed lock(*dicom);
          return lock.GetContent().Clone(true);
        }

        default:
          throw OrthancException(ErrorCode_InternalError);
      }
    }
  };


  DicomSequentialReader::DicomSequentialReader(Type type,
                                               const boost::shared_ptr<IExecutorService>& executor,
                                               DicomTransferSyntax transferSyntax,
                                               const boost::shared_ptr<DataSourceReader>& reader,
                                               unsigned int windowSize,
                                               uint64_t windowCapacity) :
    type_(type),
    transferSyntax_(transferSyntax),
    reader_(executor, reader, new Disconnector(type), windowSize, windowCapacity)
  {
  }


  void DicomSequentialReader::SubmitInternal(const FileInfo& attachment,
                                             IDynamicObject* userData)
  {
    std::unique_ptr<IDynamicObject> protection(userData);

    std::unique_ptr<IDataIdentifier> id;

    switch (type_)
    {
      case Type_Original:
        id.reset(DicomDataSource::CreateWholeRequest(attachment));
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
                                                                  const boost::shared_ptr<DataSourceReader>& dicomReader,
                                                                  unsigned int windowSize,
                                                                  uint64_t windowCapacity)
  {
    return new DicomSequentialReader(
      Type_Original, executor, DicomTransferSyntax_LittleEndianImplicit /* dummy value */,
      dicomReader, windowSize, windowCapacity);
  }


  DicomSequentialReader* DicomSequentialReader::CreateForTranscoded(const boost::shared_ptr<IExecutorService>& executor,
                                                                    DicomTransferSyntax transferSyntax,
                                                                    const boost::shared_ptr<DataSourceReader>& transcoderReader,
                                                                    unsigned int windowSize,
                                                                    uint64_t windowCapacity)
  {
    return new DicomSequentialReader(
      Type_Transcoded, executor, transferSyntax, transcoderReader, windowSize, windowCapacity);
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


  ParsedDicomFile* DicomSequentialReader::Next()
  {
    std::unique_ptr<DataSourceSequentialReader::Item> item(reader_.Next());
    return dynamic_cast<ParsedDicomFile*>(item->ReleaseValue());
  }
}
