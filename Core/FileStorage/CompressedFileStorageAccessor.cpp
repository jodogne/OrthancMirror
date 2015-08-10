/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2015 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * In addition, as a special exception, the copyright holders of this
 * program give permission to link the code of its release with the
 * OpenSSL project's "OpenSSL" library (or with modified versions of it
 * that use the same license as the "OpenSSL" library), and distribute
 * the linked executables. You must obey the GNU General Public License
 * in all respects for all of the code used other than "OpenSSL". If you
 * modify file(s) with this exception, you may extend this exception to
 * your version of the file(s), but you are not obligated to do so. If
 * you do not wish to do so, delete this exception statement from your
 * version. If you delete this exception statement from all source files
 * in the program, then also delete it here.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#include "../PrecompiledHeaders.h"
#include "CompressedFileStorageAccessor.h"

#include "../HttpServer/BufferHttpSender.h"
#include "../Logging.h"
#include "../OrthancException.h"
#include "../Uuid.h"
#include "FileStorageAccessor.h"

#include <memory>

namespace Orthanc
{
  FileInfo CompressedFileStorageAccessor::WriteInternal(const void* data,
                                                        size_t size,
                                                        FileContentType type)
  {
    std::string uuid = Toolbox::GenerateUuid();

    std::string md5;

    if (storeMD5_)
    {
      Toolbox::ComputeMD5(md5, data, size);
    }

    switch (compressionType_)
    {
    case CompressionType_None:
    {
      GetStorageArea().Create(uuid.c_str(), data, size, type);
      return FileInfo(uuid, type, size, md5);
    }

    case CompressionType_ZlibWithSize:
    {
      std::string compressed;
      zlib_.Compress(compressed, data, size);

      std::string compressedMD5;
      
      if (storeMD5_)
      {
        Toolbox::ComputeMD5(compressedMD5, compressed);
      }

      if (compressed.size() > 0)
      {
        GetStorageArea().Create(uuid.c_str(), &compressed[0], compressed.size(), type);
      }
      else
      {
        GetStorageArea().Create(uuid.c_str(), NULL, 0, type);
      }

      return FileInfo(uuid, type, size, md5,
                      CompressionType_ZlibWithSize, compressed.size(), compressedMD5);
    }

    default:
      throw OrthancException(ErrorCode_NotImplemented);
    }
  }


  CompressedFileStorageAccessor::CompressedFileStorageAccessor() : 
    storage_(NULL),
    compressionType_(CompressionType_None)
  {
  }


  CompressedFileStorageAccessor::CompressedFileStorageAccessor(IStorageArea& storage) : 
    storage_(&storage),
    compressionType_(CompressionType_None)
  {
  }


  IStorageArea& CompressedFileStorageAccessor::GetStorageArea()
  {
    if (storage_ == NULL)
    {
      LOG(ERROR) << "No storage area is currently available";
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    return *storage_;
  }


  void CompressedFileStorageAccessor::Read(std::string& content,
                                           const std::string& uuid,
                                           FileContentType type)
  {
    switch (compressionType_)
    {
    case CompressionType_None:
      GetStorageArea().Read(content, uuid, type);
      break;

    case CompressionType_ZlibWithSize:
    {
      std::string compressed;
      GetStorageArea().Read(compressed, uuid, type);
      IBufferCompressor::Uncompress(content, zlib_, compressed);
      break;
    }

    default:
      throw OrthancException(ErrorCode_NotImplemented);
    }
  }

  HttpFileSender* CompressedFileStorageAccessor::ConstructHttpFileSender(const std::string& uuid,
                                                                         FileContentType type)
  {
    switch (compressionType_)
    {
    case CompressionType_None:
    {
      FileStorageAccessor uncompressedAccessor(GetStorageArea());
      return uncompressedAccessor.ConstructHttpFileSender(uuid, type);
    }

    case CompressionType_ZlibWithSize:
    {
      std::string compressed;
      GetStorageArea().Read(compressed, uuid, type);

      std::auto_ptr<BufferHttpSender> sender(new BufferHttpSender);
      IBufferCompressor::Uncompress(sender->GetBuffer(), zlib_, compressed);

      return sender.release();
    }        

    default:
      throw OrthancException(ErrorCode_NotImplemented);
    }
  }


  void  CompressedFileStorageAccessor::Remove(const std::string& uuid,
                                              FileContentType type)
  {
    GetStorageArea().Remove(uuid, type);
  }
}
