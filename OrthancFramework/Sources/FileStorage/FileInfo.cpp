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


#include "../PrecompiledHeaders.h"
#include "FileInfo.h"

#include "../OrthancException.h"

namespace Orthanc
{
  FileInfo::FileInfo() :
    valid_(false),
    contentType_(FileContentType_Unknown),
    uncompressedSize_(0),
    compressionType_(CompressionType_None),
    compressedSize_(0)
  {
  }

  
  FileInfo::FileInfo(const std::string& uuid,
                     FileContentType contentType,
                     uint64_t size,
                     const std::string& md5) :
    valid_(true),
    uuid_(uuid),
    contentType_(contentType),
    uncompressedSize_(size),
    uncompressedMD5_(md5),
    compressionType_(CompressionType_None),
    compressedSize_(size),
    compressedMD5_(md5)
  {
  }


  FileInfo::FileInfo(const std::string& uuid,
                     FileContentType contentType,
                     uint64_t uncompressedSize,
                     const std::string& uncompressedMD5,
                     CompressionType compressionType,
                     uint64_t compressedSize,
                     const std::string& compressedMD5) :
    valid_(true),
    uuid_(uuid),
    contentType_(contentType),
    uncompressedSize_(uncompressedSize),
    uncompressedMD5_(uncompressedMD5),
    compressionType_(compressionType),
    compressedSize_(compressedSize),
    compressedMD5_(compressedMD5)
  {
  }

  
  bool FileInfo::IsValid() const
  {
    return valid_;
  }

  
  const std::string& FileInfo::GetUuid() const
  {
    if (valid_)
    {
      return uuid_;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }

  
  FileContentType FileInfo::GetContentType() const
  {
    if (valid_)
    {
      return contentType_;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }
  

  uint64_t FileInfo::GetUncompressedSize() const
  {
    if (valid_)
    {
      return uncompressedSize_;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }
  

  CompressionType FileInfo::GetCompressionType() const
  {
    if (valid_)
    {
      return compressionType_;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }
  

  uint64_t FileInfo::GetCompressedSize() const
  {
    if (valid_)
    {
      return compressedSize_;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }

  
  const std::string& FileInfo::GetCompressedMD5() const
  {
    if (valid_)
    {
      return compressedMD5_;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }

  
  const std::string& FileInfo::GetUncompressedMD5() const
  {
    if (valid_)
    {
      return uncompressedMD5_;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }
}
