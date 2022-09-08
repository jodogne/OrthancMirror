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


static const std::string METRICS_CREATE = "orthanc_storage_create_duration_ms";
static const std::string METRICS_READ = "orthanc_storage_read_duration_ms";
static const std::string METRICS_REMOVE = "orthanc_storage_remove_duration_ms";


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


  StorageAccessor::StorageAccessor(IStorageArea &area, StorageCache* cache) :
    area_(area),
    cache_(cache),
    metrics_(NULL)
  {
  }

  StorageAccessor::StorageAccessor(IStorageArea &area, 
                                   StorageCache* cache,
                                   MetricsRegistry &metrics) :
    area_(area),
    cache_(cache),
    metrics_(&metrics)
  {
  }


  FileInfo StorageAccessor::WriteInstance(std::string& customData,
                                          const DicomInstanceToStore& instance,
                                          const void* data,
                                          size_t size,
                                          FileContentType type,
                                          CompressionType compression,
                                          bool storeMd5,
                                          const std::string& uuid)
  {
    std::string md5;

    if (storeMd5)
    {
      Toolbox::ComputeMD5(md5, data, size);
    }

    switch (compression)
    {
      case CompressionType_None:
      {
        MetricsTimer timer(*this, METRICS_CREATE);

        area_.CreateInstance(customData, instance, uuid, data, size, type, false);
        
        if (cache_ != NULL)
        {
          cache_->Add(uuid, type, data, size);
        }

        return FileInfo(uuid, type, size, md5, customData);
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
          MetricsTimer timer(*this, METRICS_CREATE);

          if (compressed.size() > 0)
          {
            area_.CreateInstance(customData, instance, uuid, &compressed[0], compressed.size(), type, true);
          }
          else
          {
            area_.CreateInstance(customData, instance, uuid, NULL, 0, type, true);
          }
        }

        if (cache_ != NULL)
        {
          cache_->Add(uuid, type, data, size);  // always add uncompressed data to cache
        }

        return FileInfo(uuid, type, size, md5,
                        CompressionType_ZlibWithSize, compressed.size(), compressedMD5, customData);
      }

      default:
        throw OrthancException(ErrorCode_NotImplemented);
    }
  }

  FileInfo StorageAccessor::WriteAttachment(std::string& customData,
                                            const std::string& resourceId,
                                            ResourceType resourceType,
                                            const void* data,
                                            size_t size,
                                            FileContentType type,
                                            CompressionType compression,
                                            bool storeMd5,
                                            const std::string& uuid)
  {
    std::string md5;

    if (storeMd5)
    {
      Toolbox::ComputeMD5(md5, data, size);
    }

    switch (compression)
    {
      case CompressionType_None:
      {
        MetricsTimer timer(*this, METRICS_CREATE);

        area_.CreateAttachment(customData, resourceId, resourceType, uuid, data, size, type, false);
        
        if (cache_ != NULL)
        {
          cache_->Add(uuid, type, data, size);
        }

        return FileInfo(uuid, type, size, md5, customData);
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
          MetricsTimer timer(*this, METRICS_CREATE);

          if (compressed.size() > 0)
          {
            area_.CreateAttachment(customData, resourceId, resourceType, uuid, &compressed[0], compressed.size(), type, true);
          }
          else
          {
            area_.CreateAttachment(customData, resourceId, resourceType, uuid, NULL, 0, type, true);
          }
        }

        if (cache_ != NULL)
        {
          cache_->Add(uuid, type, data, size);  // always add uncompressed data to cache
        }

        return FileInfo(uuid, type, size, md5,
                        CompressionType_ZlibWithSize, compressed.size(), compressedMD5, customData);
      }

      default:
        throw OrthancException(ErrorCode_NotImplemented);
    }
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
          MetricsTimer timer(*this, METRICS_READ);
          std::unique_ptr<IMemoryBuffer> buffer(area_.Read(info.GetUuid(), info.GetContentType(), info.GetCustomData()));
          buffer->MoveToString(content);

          break;
        }

        case CompressionType_ZlibWithSize:
        {
          ZlibCompressor zlib;

          std::unique_ptr<IMemoryBuffer> compressed;
          
          {
            MetricsTimer timer(*this, METRICS_READ);
            compressed.reset(area_.Read(info.GetUuid(), info.GetContentType(), info.GetCustomData()));
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
      MetricsTimer timer(*this, METRICS_READ);
      std::unique_ptr<IMemoryBuffer> buffer(area_.Read(info.GetUuid(), info.GetContentType(), info.GetCustomData()));
      buffer->MoveToString(content);
    }
  }


  void StorageAccessor::Remove(const std::string& fileUuid,
                               FileContentType type,
                               const std::string& customData)
  {
    if (cache_ != NULL)
    {
      cache_->Invalidate(fileUuid, type);
    }

    {
      MetricsTimer timer(*this, METRICS_REMOVE);
      area_.Remove(fileUuid, type, customData);
    }
  }
  

  void StorageAccessor::Remove(const FileInfo &info)
  {
    Remove(info.GetUuid(), info.GetContentType(), info.GetCustomData());
  }


  void StorageAccessor::ReadStartRange(std::string& target,
                                       const std::string& fileUuid,
                                       FileContentType contentType,
                                       uint64_t end /* exclusive */,
                                       const std::string& customData)
  {
    if (cache_ == NULL || !cache_->FetchStartRange(target, fileUuid, contentType, end))
    {
      MetricsTimer timer(*this, METRICS_READ);
      std::unique_ptr<IMemoryBuffer> buffer(area_.ReadRange(fileUuid, contentType, 0, end, customData));
      assert(buffer->GetSize() == end);
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

  // bool StorageAccessor::HandlesCustomData()
  // {
  //   return area_.HandlesCustomData();
  // }

  // void StorageAccessor::GetCustomData(std::string& customData,
  //                                     const std::string& uuid,
  //                                     const DicomInstanceToStore* instance,
  //                                     const void* content, 
  //                                     size_t size,
  //                                     FileContentType type,
  //                                     bool compression)
  // {
  //   area_.GetCustomData(customData, uuid, instance, content, size, type, compression);
  // }

}
