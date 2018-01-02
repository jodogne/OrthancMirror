/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2018 Osimis S.A., Belgium
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
#include "StorageAccessor.h"

#include "../Compression/ZlibCompressor.h"
#include "../OrthancException.h"
#include "../Toolbox.h"
#include "../SystemToolbox.h"

#if ORTHANC_ENABLE_CIVETWEB == 1 || ORTHANC_ENABLE_MONGOOSE == 1
#  include "../HttpServer/HttpStreamTranscoder.h"
#endif

namespace Orthanc
{
  FileInfo StorageAccessor::Write(const void* data,
                                  size_t size,
                                  FileContentType type,
                                  CompressionType compression,
                                  bool storeMd5)
  {
    std::string uuid = SystemToolbox::GenerateUuid();

    std::string md5;

    if (storeMd5)
    {
      Toolbox::ComputeMD5(md5, data, size);
    }

    switch (compression)
    {
      case CompressionType_None:
      {
        area_.Create(uuid, data, size, type);
        return FileInfo(uuid, type, size, md5);
      }

      case CompressionType_ZlibWithSize:
      {
        ZlibCompressor zlib;

        std::string compressed;
        zlib.Compress(compressed, data, size);

        std::string compressedMD5;
      
        if (storeMd5)
        {
          Toolbox::ComputeMD5(compressedMD5, compressed);
        }

        if (compressed.size() > 0)
        {
          area_.Create(uuid, &compressed[0], compressed.size(), type);
        }
        else
        {
          area_.Create(uuid, NULL, 0, type);
        }

        return FileInfo(uuid, type, size, md5,
                        CompressionType_ZlibWithSize, compressed.size(), compressedMD5);
      }

      default:
        throw OrthancException(ErrorCode_NotImplemented);
    }
  }


  void StorageAccessor::Read(std::string& content,
                             const FileInfo& info)
  {
    switch (info.GetCompressionType())
    {
      case CompressionType_None:
      {
        area_.Read(content, info.GetUuid(), info.GetContentType());
        break;
      }

      case CompressionType_ZlibWithSize:
      {
        ZlibCompressor zlib;

        std::string compressed;
        area_.Read(compressed, info.GetUuid(), info.GetContentType());
        IBufferCompressor::Uncompress(content, zlib, compressed);
        break;
      }

      default:
      {
        throw OrthancException(ErrorCode_NotImplemented);
      }
    }

    // TODO Check the validity of the uncompressed MD5?
  }


  void StorageAccessor::Read(Json::Value& content,
                             const FileInfo& info)
  {
    std::string s;
    Read(s, info);

    Json::Reader reader;
    if (!reader.parse(s, content))
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }
  }


#if ORTHANC_ENABLE_CIVETWEB == 1 || ORTHANC_ENABLE_MONGOOSE == 1
  void StorageAccessor::SetupSender(BufferHttpSender& sender,
                                    const FileInfo& info,
                                    const std::string& mime)
  {
    area_.Read(sender.GetBuffer(), info.GetUuid(), info.GetContentType());
    sender.SetContentType(mime);

    const char* extension;
    switch (info.GetContentType())
    {
      case FileContentType_Dicom:
        extension = ".dcm";
        break;

      case FileContentType_DicomAsJson:
        extension = ".json";
        break;

      default:
        // Non-standard content type
        extension = "";
    }

    sender.SetContentFilename(info.GetUuid() + std::string(extension));
  }
#endif


#if ORTHANC_ENABLE_CIVETWEB == 1 || ORTHANC_ENABLE_MONGOOSE == 1
  void StorageAccessor::AnswerFile(HttpOutput& output,
                                   const FileInfo& info,
                                   const std::string& mime)
  {
    BufferHttpSender sender;
    SetupSender(sender, info, mime);
  
    HttpStreamTranscoder transcoder(sender, info.GetCompressionType());
    output.Answer(transcoder);
  }
#endif


#if ORTHANC_ENABLE_CIVETWEB == 1 || ORTHANC_ENABLE_MONGOOSE == 1
  void StorageAccessor::AnswerFile(RestApiOutput& output,
                                   const FileInfo& info,
                                   const std::string& mime)
  {
    BufferHttpSender sender;
    SetupSender(sender, info, mime);
  
    HttpStreamTranscoder transcoder(sender, info.GetCompressionType());
    output.AnswerStream(transcoder);
  }
#endif
}
