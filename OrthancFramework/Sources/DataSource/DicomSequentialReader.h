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

#include "../FileStorage/FileInfo.h"
#include "../IMemoryBuffer.h"
#include "../MultiThreading/IExecutorService.h"
#include "DataSourceSequentialReader.h"


namespace Orthanc
{
  class ParsedDicomFile;

  class ORTHANC_PUBLIC DicomSequentialReader : public boost::noncopyable
  {
  public:
    class ORTHANC_PUBLIC Item : public IDynamicObject
    {
    private:
      std::unique_ptr<ParsedDicomFile>  parsed_;
      boost::shared_ptr<IMemoryBuffer>  raw_;

    public:
      explicit Item(ParsedDicomFile* parsed /* takes ownership */);

      explicit Item(const boost::shared_ptr<IMemoryBuffer>& raw);

    private:
      Item(ParsedDicomFile* parsed /* takes ownership */,
           IMemoryBuffer* raw /* takes ownership */);

    public:
      ParsedDicomFile& GetParsedDicomFile();

      const IMemoryBuffer& GetRawMemoryBuffer();
    };

  private:
    class Disconnector;

    enum SourceType
    {
      SourceType_StorageArea,
      SourceType_Dicom,
      SourceType_Transcoder
    };

    SourceType                     sourceType_;
    DicomTransferSyntax            targetSyntax_;
    TranscodingSopInstanceUidMode  mode_;
    bool                           hasLossyQuality_;
    unsigned int                   lossyQuality_;
    DataSourceSequentialReader     reader_;

    DicomSequentialReader(SourceType sourceType,
                          const boost::shared_ptr<IExecutorService>& executor,
                          DicomTransferSyntax targetSyntax,
                          TranscodingSopInstanceUidMode mode,
                          bool hasLossyQuality,
                          unsigned int lossyQuality,
                          const boost::shared_ptr<DataSourceReader>& reader,
                          unsigned int windowSize,
                          uint64_t windowCapacity);

    void SubmitInternal(const FileInfo& attachment,
                        IDynamicObject* userData);

  public:
    static DicomSequentialReader* CreateForOriginal(const boost::shared_ptr<IExecutorService>& executor,
                                                    const boost::shared_ptr<DataSourceReader>& dicomReader,
                                                    unsigned int windowSize,
                                                    uint64_t windowCapacity);

    static DicomSequentialReader* CreateForTranscoded(const boost::shared_ptr<IExecutorService>& executor,
                                                      DicomTransferSyntax targetSyntax,
                                                      TranscodingSopInstanceUidMode mode,
                                                      bool hasLossyQuality,
                                                      unsigned int lossyQuality,
                                                      const boost::shared_ptr<DataSourceReader>& transcoderReader,
                                                      unsigned int windowSize,
                                                      uint64_t windowCapacity);

    void Submit(const FileInfo& attachment);

    void Submit(const FileInfo& attachment,
                IDynamicObject* userData /* takes ownership */);

    void Start();

    bool HasNext();

    Item* Next();
  };
}
