/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
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

#include <string>
#include <stdint.h>
#include "../Enumerations.h"

namespace Orthanc
{
  struct FileInfo
  {
  private:
    std::string uuid_;
    FileContentType contentType_;

    uint64_t uncompressedSize_;
    std::string uncompressedMD5_;

    CompressionType compressionType_;
    uint64_t compressedSize_;
    std::string compressedMD5_;

  public:
    FileInfo()
    {
    }

    /**
     * Constructor for an uncompressed attachment.
     **/
    FileInfo(const std::string& uuid,
             FileContentType contentType,
             uint64_t size,
             const std::string& md5) :
      uuid_(uuid),
      contentType_(contentType),
      uncompressedSize_(size),
      uncompressedMD5_(md5),
      compressionType_(CompressionType_None),
      compressedSize_(size),
      compressedMD5_(md5)
    {
    }

    /**
     * Constructor for a compressed attachment.
     **/
    FileInfo(const std::string& uuid,
             FileContentType contentType,
             uint64_t uncompressedSize,
             const std::string& uncompressedMD5,
             CompressionType compressionType,
             uint64_t compressedSize,
             const std::string& compressedMD5) :
      uuid_(uuid),
      contentType_(contentType),
      uncompressedSize_(uncompressedSize),
      uncompressedMD5_(uncompressedMD5),
      compressionType_(compressionType),
      compressedSize_(compressedSize),
      compressedMD5_(compressedMD5)
    {
    }

    const std::string& GetUuid() const
    {
      return uuid_;
    }

    FileContentType GetContentType() const
    {
      return contentType_;
    }

    uint64_t GetUncompressedSize() const
    {
      return uncompressedSize_;
    }

    CompressionType GetCompressionType() const
    {
      return compressionType_;
    }

    uint64_t GetCompressedSize() const
    {
      return compressedSize_;
    }

    const std::string& GetCompressedMD5() const
    {
      return compressedMD5_;
    }

    const std::string& GetUncompressedMD5() const
    {
      return uncompressedMD5_;
    }
  };
}
