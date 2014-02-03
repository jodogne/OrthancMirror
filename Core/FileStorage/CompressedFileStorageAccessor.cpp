/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2014 Medical Physics Department, CHU of Liege,
 * Belgium
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


#include "CompressedFileStorageAccessor.h"

#include "../OrthancException.h"
#include "FileStorageAccessor.h"
#include "../HttpServer/BufferHttpSender.h"

namespace Orthanc
{
  FileInfo CompressedFileStorageAccessor::WriteInternal(const void* data,
                                                        size_t size,
                                                        FileContentType type)
  {
    switch (compressionType_)
    {
    case CompressionType_None:
    {
      std::string uuid = storage_.Create(data, size);
      return FileInfo(uuid, type, size);
    }

    case CompressionType_Zlib:
    {
      std::string compressed;
      zlib_.Compress(compressed, data, size);
      std::string uuid = storage_.Create(compressed);
      return FileInfo(uuid, type, size, 
                      CompressionType_Zlib, compressed.size());
    }

    default:
      throw OrthancException(ErrorCode_NotImplemented);
    }
  }

  CompressedFileStorageAccessor::CompressedFileStorageAccessor(FileStorage& storage) : 
    storage_(storage)
  {
    compressionType_ = CompressionType_None;
  }

  void CompressedFileStorageAccessor::Read(std::string& content,
                                           const std::string& uuid)
  {
    switch (compressionType_)
    {
    case CompressionType_None:
      storage_.ReadFile(content, uuid);
      break;

    case CompressionType_Zlib:
    {
      std::string compressed;
      storage_.ReadFile(compressed, uuid);
      zlib_.Uncompress(content, compressed);
      break;
    }

    default:
      throw OrthancException(ErrorCode_NotImplemented);
    }
  }

  HttpFileSender* CompressedFileStorageAccessor::ConstructHttpFileSender(const std::string& uuid)
  {
    switch (compressionType_)
    {
    case CompressionType_None:
    {
      FileStorageAccessor uncompressedAccessor(storage_);
      return uncompressedAccessor.ConstructHttpFileSender(uuid);
    }

    case CompressionType_Zlib:
    {
      std::string compressed;
      storage_.ReadFile(compressed, uuid);

      std::auto_ptr<BufferHttpSender> sender(new BufferHttpSender);
      zlib_.Uncompress(sender->GetBuffer(), compressed);

      return sender.release();
    }        

    default:
      throw OrthancException(ErrorCode_NotImplemented);
    }
  }
}
