/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

#include "../OrthancFramework.h"

#if !defined(ORTHANC_ENABLE_ZLIB)
#  error The macro ORTHANC_ENABLE_ZLIB must be defined
#endif

#if ORTHANC_ENABLE_ZLIB != 1
#  error ZLIB support must be enabled to include this file
#endif

#if ORTHANC_BUILD_UNIT_TESTS == 1
#  include <gtest/gtest_prod.h>
#endif

#include "../ChunkedBuffer.h"
#include "../Compatibility.h"


#include <stdint.h>
#include <string>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>

namespace Orthanc
{
  class ORTHANC_PUBLIC ZipWriter : public boost::noncopyable
  {
#if ORTHANC_BUILD_UNIT_TESTS == 1
    FRIEND_TEST(ZipWriter, BufferWithSeek);
#endif

  public:
    // New in Orthanc 1.9.4
    class ORTHANC_PUBLIC IOutputStream : public boost::noncopyable
    {
    public:
      virtual ~IOutputStream()
      {
      }

      virtual void Write(const std::string& chunk) = 0;

      virtual void Close() = 0;

      virtual uint64_t GetArchiveSize() const = 0;
    };


    // The lifetime of the "target" buffer must be larger than that of ZipWriter
    class ORTHANC_PUBLIC MemoryStream : public IOutputStream
    {
    private:
      std::string&   target_;
      ChunkedBuffer  chunked_;
      uint64_t       archiveSize_;
      
    public:
      explicit MemoryStream(std::string& target);
      
      virtual void Write(const std::string& chunk) ORTHANC_OVERRIDE;
      
      virtual void Close() ORTHANC_OVERRIDE;

      virtual uint64_t GetArchiveSize() const ORTHANC_OVERRIDE;
    };


  private:
    // This class is only public for unit tests
    class ORTHANC_PUBLIC BufferWithSeek : public boost::noncopyable
    {
    private:
      size_t         currentPosition_;
      ChunkedBuffer  chunks_;
      std::string    flattened_;

      void CheckInvariants() const;
  
    public:
      BufferWithSeek();

      ~BufferWithSeek();

      size_t GetPosition() const;
  
      size_t GetSize() const;

      void Write(const void* data,
                 size_t size);

      void Write(const std::string& data);

      void Seek(size_t position);

      void Flush(std::string& target);
    };

    
  private:
    class StreamBuffer;
    
    struct PImpl;
    boost::shared_ptr<PImpl> pimpl_;

    bool isZip64_;
    bool hasFileInZip_;
    bool append_;
    uint8_t compressionLevel_;
    std::string path_;

    std::unique_ptr<IOutputStream> outputStream_;

  public:
    ZipWriter();

    ~ZipWriter();

    void SetZip64(bool isZip64);

    bool IsZip64() const;

    void SetCompressionLevel(uint8_t level);

    uint8_t GetCompressionLevel() const;

    void SetAppendToExisting(bool append);
    
    bool IsAppendToExisting() const;
    
    void Open();

    void Close();

    bool IsOpen() const;

    void SetOutputPath(const char* path);

    const std::string& GetOutputPath() const;

    void OpenFile(const char* path);

    void Write(const void* data, size_t length);

    void Write(const std::string& data);

    void AcquireOutputStream(IOutputStream* stream, // transfers ownership
                             bool isZip64);

    // The lifetime of the "target" buffer must be larger than that of ZipWriter
    void SetMemoryOutput(std::string& target,
                         bool isZip64);

    void CancelStream();

    // WARNING: "GetArchiveSize()" only has its final value after
    // "Close()" has been called
    uint64_t GetArchiveSize() const;
  };
}
