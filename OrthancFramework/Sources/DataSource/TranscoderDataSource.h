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


#pragma once

#include "../DicomParsing/IDicomTranscoder.h"
#include "../FileStorage/FileInfo.h"
#include "../MultiThreading/Mutex.h"
#include "IDataSource.h"

#include <boost/shared_ptr.hpp>


namespace Orthanc
{
  class DataSourceReader;

  class TranscoderDataSource : public IDataSource
  {
  private:
    class Value;
    class Identifier;

    boost::shared_ptr<IDicomTranscoder>  transcoder_;
    boost::shared_ptr<DataSourceReader>  storageAreaReader_;

  public:
    TranscoderDataSource(const boost::shared_ptr<IDicomTranscoder>& transcoder,
                         const boost::shared_ptr<DataSourceReader>& storageAreaReader);

    virtual IDynamicObject* Load(const IDataIdentifier& id) ORTHANC_OVERRIDE;

    virtual size_t GetValueSize(const IDynamicObject& value) const ORTHANC_OVERRIDE;

    class Transcoded : public boost::noncopyable
    {
    private:
      Mutex                              mutex_;
      boost::shared_ptr<IDynamicObject>  value_;

    public:
      explicit Transcoded(const boost::shared_ptr<IDynamicObject>& value);

      class LockAsParsed : public boost::noncopyable
      {
      private:
        /**
         * Access to the transcoded DICOM must be protected by a
         * mutex, as it could be shared by multiple threads if caching
         * is enabled in the DataSourceReader.
         **/

        Mutex::ScopedLock                 lock_;
        std::unique_ptr<ParsedDicomFile>  parsed_;
        const ParsedDicomFile*            content_;

      public:
        explicit LockAsParsed(Transcoded& that);

        const ParsedDicomFile& GetContent() const
        {
          return *content_;
        }
      };

      class LockAsBuffer : public boost::noncopyable
      {
      private:
        /**
         * Access to the transcoded DICOM must only be protected in
         * the constructor, if serialization to the buffer is required.
         **/

        std::unique_ptr<std::string>  serialized_;
        const std::string*            content_;

      public:
        explicit LockAsBuffer(Transcoded& that);

        const std::string& GetContent() const
        {
          return *content_;
        }
      };
    };

    static Transcoded* Transcode(DataSourceReader& reader,
                                 const FileInfo& attachment,
                                 DicomTransferSyntax targetSyntax);

    static Transcoded* Transcode(DataSourceReader& reader,
                                 const FileInfo& attachment,
                                 DicomTransferSyntax targetSyntax,
                                 TranscodingSopInstanceUidMode mode,
                                 unsigned int lossyQuality);
  };
}
