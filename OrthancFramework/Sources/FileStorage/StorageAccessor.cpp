/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2021-2023 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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
#include "StorageAccessor.h"
#include "StorageCache.h"

#include "../Logging.h"
#include "../StringMemoryBuffer.h"
#include "../Compatibility.h"
#include "../Compression/ZlibCompressor.h"
#include "../MetricsRegistry.h"
#include "../OrthancException.h"
#include "../Toolbox.h"

#if ORTHANC_ENABLE_CIVETWEB == 1 || ORTHANC_ENABLE_MONGOOSE == 1
#  include "../HttpServer/HttpStreamTranscoder.h"
#endif


static const std::string METRICS_CREATE_DURATION = "orthanc_storage_create_duration_ms";
static const std::string METRICS_READ_DURATION = "orthanc_storage_read_duration_ms";
static const std::string METRICS_REMOVE_DURATION = "orthanc_storage_remove_duration_ms";
static const std::string METRICS_READ_BYTES = "orthanc_storage_read_bytes";
static const std::string METRICS_WRITTEN_BYTES = "orthanc_storage_written_bytes";


namespace Orthanc
{
  class StorageAccessor::MetricsTimer : public boost::noncopyable
  {
  private:
    std::unique_ptr<MetricsRegistry::Timer>  timer_;

  public:
    MetricsTimer(StorageAccessor& that,
                 const std::string& name)
    {
      if (that.metrics_ != NULL)
      {
        timer_.reset(new MetricsRegistry::Timer(*that.metrics_, name));
      }
    }
  };


  StorageAccessor::StorageAccessor(IStorageArea& area) :
    area_(area),
    cache_(NULL),
    metrics_(NULL)
  {
  }
  

  StorageAccessor::StorageAccessor(IStorageArea& area, 
                                   StorageCache& cache) :
    area_(area),
    cache_(&cache),
    metrics_(NULL)
  {
  }


  StorageAccessor::StorageAccessor(IStorageArea& area,
                                   MetricsRegistry& metrics) :
    area_(area),
    cache_(NULL),
    metrics_(&metrics)
  {
  }

  StorageAccessor::StorageAccessor(IStorageArea& area, 
                                   StorageCache& cache,
                                   MetricsRegistry& metrics) :
    area_(area),
    cache_(&cache),
    metrics_(&metrics)
  {
  }


  FileInfo StorageAccessor::Write(const void* data,
                                  size_t size,
                                  FileContentType type,
                                  CompressionType compression,
                                  bool storeMd5)
  {
    std::string uuid = Toolbox::GenerateUuid();

    std::string md5;

    if (storeMd5)
    {
      Toolbox::ComputeMD5(md5, data, size);
    }

    switch (compression)
    {
      case CompressionType_None:
      {
        {
          MetricsTimer timer(*this, METRICS_CREATE_DURATION);
          area_.Create(uuid, data, size, type);
        }

        if (metrics_ != NULL)
        {
          metrics_->IncrementIntegerValue(METRICS_WRITTEN_BYTES, size);
        }
        
        if (cache_ != NULL)
        {
          cache_->Add(uuid, type, data, size);
        }

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

        {
          MetricsTimer timer(*this, METRICS_CREATE_DURATION);

          if (compressed.size() > 0)
          {
            area_.Create(uuid, &compressed[0], compressed.size(), type);
          }
          else
          {
            area_.Create(uuid, NULL, 0, type);
          }
        }

        if (metrics_ != NULL)
        {
          metrics_->IncrementIntegerValue(METRICS_WRITTEN_BYTES, compressed.size());
        }

        if (cache_ != NULL)
        {
          cache_->Add(uuid, type, data, size);  // always add uncompressed data to cache
        }

        return FileInfo(uuid, type, size, md5,
                        CompressionType_ZlibWithSize, compressed.size(), compressedMD5);
      }

      default:
        throw OrthancException(ErrorCode_NotImplemented);
    }
  }

  FileInfo StorageAccessor::Write(const std::string &data,
                                  FileContentType type,
                                  CompressionType compression,
                                  bool storeMd5)
  {
    return Write((data.size() == 0 ? NULL : data.c_str()),
                 data.size(), type, compression, storeMd5);
  }


  void StorageAccessor::Read(std::string& content,
                             const FileInfo& info)
  {
    if (cache_ == NULL ||
        !cache_->Fetch(content, info.GetUuid(), info.GetContentType()))
    {
      switch (info.GetCompressionType())
      {
        case CompressionType_None:
        {
          std::unique_ptr<IMemoryBuffer> buffer;

          {
            MetricsTimer timer(*this, METRICS_READ_DURATION);
            buffer.reset(area_.Read(info.GetUuid(), info.GetContentType()));
          }

          if (metrics_ != NULL)
          {
            metrics_->IncrementIntegerValue(METRICS_READ_BYTES, buffer->GetSize());
          }

          buffer->MoveToString(content);

          break;
        }

        case CompressionType_ZlibWithSize:
        {
          ZlibCompressor zlib;

          std::unique_ptr<IMemoryBuffer> compressed;
          
          {
            MetricsTimer timer(*this, METRICS_READ_DURATION);
            compressed.reset(area_.Read(info.GetUuid(), info.GetContentType()));
          }
          
          if (metrics_ != NULL)
          {
            metrics_->IncrementIntegerValue(METRICS_READ_BYTES, compressed->GetSize());
          }

          zlib.Uncompress(content, compressed->GetData(), compressed->GetSize());

          break;
        }

        default:
        {
          throw OrthancException(ErrorCode_NotImplemented);
        }
      }

      // always store the uncompressed data in cache
      if (cache_ != NULL)
      {
        cache_->Add(info.GetUuid(), info.GetContentType(), content);
      }
    }

    // TODO Check the validity of the uncompressed MD5?
  }


  void StorageAccessor::ReadRaw(std::string& content,
                                const FileInfo& info)
  {
    if (cache_ == NULL || !cache_->Fetch(content, info.GetUuid(), info.GetContentType()))
    {
      std::unique_ptr<IMemoryBuffer> buffer;

      {
        MetricsTimer timer(*this, METRICS_READ_DURATION);
        buffer.reset(area_.Read(info.GetUuid(), info.GetContentType()));
      }

      if (metrics_ != NULL)
      {
        metrics_->IncrementIntegerValue(METRICS_READ_BYTES, buffer->GetSize());
      }

      buffer->MoveToString(content);
    }
  }


  void StorageAccessor::Remove(const std::string& fileUuid,
                               FileContentType type)
  {
    if (cache_ != NULL)
    {
      cache_->Invalidate(fileUuid, type);
    }

    {
      MetricsTimer timer(*this, METRICS_REMOVE_DURATION);
      area_.Remove(fileUuid, type);
    }
  }
  

  void StorageAccessor::Remove(const FileInfo &info)
  {
    Remove(info.GetUuid(), info.GetContentType());
  }


  void StorageAccessor::ReadStartRange(std::string& target,
                                       const std::string& fileUuid,
                                       FileContentType contentType,
                                       uint64_t end /* exclusive */)
  {
    if (cache_ == NULL || !cache_->FetchStartRange(target, fileUuid, contentType, end))
    {
      std::unique_ptr<IMemoryBuffer> buffer;

      {
        MetricsTimer timer(*this, METRICS_READ_DURATION);
        buffer.reset(area_.ReadRange(fileUuid, contentType, 0, end));
        assert(buffer->GetSize() == end);
      }

      if (metrics_ != NULL)
      {
        metrics_->IncrementIntegerValue(METRICS_READ_BYTES, buffer->GetSize());
      }

      buffer->MoveToString(target);

      if (cache_ != NULL)
      {
        cache_->AddStartRange(fileUuid, contentType, target);
      }
    }
  }


#if ORTHANC_ENABLE_CIVETWEB == 1 || ORTHANC_ENABLE_MONGOOSE == 1
  void StorageAccessor::SetupSender(BufferHttpSender& sender,
                                    const FileInfo& info,
                                    const std::string& mime)
  {
    Read(sender.GetBuffer(), info);

    sender.SetContentType(mime);

    const char* extension;
    switch (info.GetContentType())
    {
      case FileContentType_Dicom:
      case FileContentType_DicomUntilPixelData:
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
                                   MimeType mime)
  {
    AnswerFile(output, info, EnumerationToString(mime));
  }
#endif


#if ORTHANC_ENABLE_CIVETWEB == 1 || ORTHANC_ENABLE_MONGOOSE == 1
  void StorageAccessor::AnswerFile(HttpOutput& output,
                                   const FileInfo& info,
                                   const std::string& mime)
  {
    BufferHttpSender sender;
    SetupSender(sender, info, mime);
  
    HttpStreamTranscoder transcoder(sender, CompressionType_None); // since 1.11.2, the storage accessor only returns uncompressed buffers
    output.Answer(transcoder);
  }
#endif


#if ORTHANC_ENABLE_CIVETWEB == 1 || ORTHANC_ENABLE_MONGOOSE == 1
  void StorageAccessor::AnswerFile(RestApiOutput& output,
                                   const FileInfo& info,
                                   MimeType mime)
  {
    AnswerFile(output, info, EnumerationToString(mime));
  }
#endif


#if ORTHANC_ENABLE_CIVETWEB == 1 || ORTHANC_ENABLE_MONGOOSE == 1
  void StorageAccessor::AnswerFile(RestApiOutput& output,
                                   const FileInfo& info,
                                   const std::string& mime)
  {
    BufferHttpSender sender;
    SetupSender(sender, info, mime);
  
    HttpStreamTranscoder transcoder(sender, CompressionType_None); // since 1.11.2, the storage accessor only returns uncompressed buffers
    output.AnswerStream(transcoder);
  }
#endif
}
