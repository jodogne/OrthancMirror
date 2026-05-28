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

#include "../Compatibility.h"
#include "../FileStorage/FileInfo.h"
#include "../IMemoryBuffer.h"
#include "../MultiThreading/IExecutorService.h"


namespace Orthanc
{
  class DataSourceReader;
  class DataSourceSequentialReader;
  class ParsedDicomFile;

  class ORTHANC_PUBLIC DicomSequentialReader : public boost::noncopyable
  {
  public:
    class ORTHANC_PUBLIC Item : public IDynamicObject
    {
    private:
      std::unique_ptr<ParsedDicomFile>  parsed_;
      boost::shared_ptr<IMemoryBuffer>  raw_;
      std::unique_ptr<IDynamicObject>   userData_;

    public:
      explicit Item(ParsedDicomFile* parsed /* takes ownership */);

      explicit Item(const boost::shared_ptr<IMemoryBuffer>& raw);

      Item(ParsedDicomFile* parsed /* takes ownership */,
           IMemoryBuffer* raw /* takes ownership */);

      ParsedDicomFile& GetParsedDicomFile();

      // There is a "const" here, as the value could be shared through the cache, so it cannot be modified
      const IMemoryBuffer& GetRawMemoryBuffer();

      void SetUserData(IDynamicObject* userData /* takes ownership */);

      bool HasUserData() const
      {
        return userData_.get() != NULL;
      }

      const IDynamicObject& GetUserData() const;
    };

  private:
    class Disconnector;

    enum SourceType
    {
      SourceType_StorageArea,
      SourceType_Dicom,
      SourceType_Transcoder
    };

    SourceType                                     sourceType_;
    DicomTransferSyntax                            targetSyntax_;
    TranscodingSopInstanceUidMode                  mode_;
    bool                                           hasLossyQuality_;
    unsigned int                                   lossyQuality_;
    boost::shared_ptr<DataSourceSequentialReader>  reader_;  // boost::shared_ptr<> for PImpl

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
    // The factory can be called from multiple threads
    class ORTHANC_PUBLIC Factory : public boost::noncopyable
    {
    private:
      boost::shared_ptr<IExecutorService>  executor_;
      boost::shared_ptr<DataSourceReader>  storageAreaReader_;
      boost::shared_ptr<DataSourceReader>  dicomReader_;
      boost::shared_ptr<DataSourceReader>  transcoderReader_;
      unsigned int                         windowSize_;
      uint64_t                             windowCapacity_;

    public:
      // The readers are allowed to be NULL, for unit tests
      Factory(const boost::shared_ptr<IExecutorService>& executor,
              const boost::shared_ptr<DataSourceReader>& storageAreaReader,
              const boost::shared_ptr<DataSourceReader>& dicomReader,
              const boost::shared_ptr<DataSourceReader>& transcoderReader,
              unsigned int windowSize,
              uint64_t windowCapacity);

      /**
       * Methods below prioritize the downloading of the raw DICOM
       * files (and avoid DICOM parsing if possible).
       **/

      DicomSequentialReader* CreateForOriginalRawMemoryBuffer() const;

      DicomSequentialReader* CreateForTranscodedRawMemoryBuffer(DicomTransferSyntax targetSyntax,
                                                                TranscodingSopInstanceUidMode mode,
                                                                bool hasLossyQuality,
                                                                unsigned int lossyQuality) const;

      /**
       * Methods below prioritize the parsing of the DICOM files (and
       * exploit the DICOM cache if possible).
       **/

      DicomSequentialReader* CreateForOriginalParsedDicomFile() const;

      DicomSequentialReader* CreateForTranscodedParsedDicomFile(DicomTransferSyntax targetSyntax,
                                                                TranscodingSopInstanceUidMode mode,
                                                                bool hasLossyQuality,
                                                                unsigned int lossyQuality) const;
    };

    void Submit(const FileInfo& attachment);

    void Submit(const FileInfo& attachment,
                IDynamicObject* userData /* takes ownership */);

    void Start();

    bool HasNext();

    Item* Next();
  };
}
