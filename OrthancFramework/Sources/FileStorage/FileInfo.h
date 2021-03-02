/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
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
#include "../Enumerations.h"

#include <stdint.h>

namespace Orthanc
{
  struct ORTHANC_PUBLIC FileInfo
  {
  private:
    bool             valid_;
    std::string      uuid_;
    FileContentType  contentType_;
    uint64_t         uncompressedSize_;
    std::string      uncompressedMD5_;
    CompressionType  compressionType_;
    uint64_t         compressedSize_;
    std::string      compressedMD5_;

  public:
    FileInfo();

    /**
     * Constructor for an uncompressed attachment.
     **/
    FileInfo(const std::string& uuid,
             FileContentType contentType,
             uint64_t size,
             const std::string& md5);

    /**
     * Constructor for a compressed attachment.
     **/
    FileInfo(const std::string& uuid,
             FileContentType contentType,
             uint64_t uncompressedSize,
             const std::string& uncompressedMD5,
             CompressionType compressionType,
             uint64_t compressedSize,
             const std::string& compressedMD5);

    bool IsValid() const;
    
    const std::string& GetUuid() const;

    FileContentType GetContentType() const;

    uint64_t GetUncompressedSize() const;

    CompressionType GetCompressionType() const;

    uint64_t GetCompressedSize() const;

    const std::string& GetCompressedMD5() const;

    const std::string& GetUncompressedMD5() const;
  };
}
