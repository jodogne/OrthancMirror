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

#if !defined(ORTHANC_ENABLE_DCMTK)
#  error The macro ORTHANC_ENABLE_DCMTK must be defined
#endif

#if ORTHANC_ENABLE_DCMTK != 1
#  error The macro ORTHANC_ENABLE_DCMTK must be set to 1 to use this file
#endif

#include "../Compatibility.h"
#include "../FileStorage/FileInfo.h"
#include "../MultiThreading/Mutex.h"
#include "IDataIdentifier.h"
#include "IDataSource.h"

#include <boost/shared_ptr.hpp>


namespace Orthanc
{
  class DataSourceReader;
  class ParsedDicomFile;

  class DicomDataSource : public IDataSource
  {
  private:
    class BaseIdentifier;
    class WholeIdentifier;
    class BeginningIdentifier;
    class Value;

    boost::shared_ptr<DataSourceReader>  storageAreaReader_;

  public:
    explicit DicomDataSource(const boost::shared_ptr<DataSourceReader>& storageAreaReader);

    virtual IDynamicObject* Load(const IDataIdentifier& obj) ORTHANC_OVERRIDE;

    virtual size_t GetValueSize(const IDynamicObject& obj) const ORTHANC_OVERRIDE;

    class Dicom : public boost::noncopyable
    {
    private:
      Mutex                              mutex_;
      boost::shared_ptr<IDynamicObject>  value_;

    public:
      explicit Dicom(const boost::shared_ptr<IDynamicObject>& value);

      /**
       * Access to the DICOM value must be protected by a mutex, as it
       * could be shared by multiple threads if caching is enabled in
       * the DataSourceReader.
       **/
      class Lock : public boost::noncopyable
      {
      private:
        Dicom&             that_;
        Mutex::ScopedLock  lock_;

      public:
        explicit Lock(Dicom& that) :
          that_(that),
          lock_(that.mutex_)
        {
        }

        ParsedDicomFile& GetContent() const;

        bool HasRawBuffer() const;

        const void* GetRawBufferData() const;

        size_t GetRawBufferSize() const;
      };
    };

    static Dicom* ReadWhole(DataSourceReader& reader,
                            const FileInfo& attachment,
                            bool keepRawBuffer,  // Must be set to "true" for transcoding
                            bool checkMD5);

    static Dicom* ReadUntilPixelData(DataSourceReader& reader,
                                     const FileInfo& attachment,
                                     uint64_t pixelDataOffset);
  };
}
