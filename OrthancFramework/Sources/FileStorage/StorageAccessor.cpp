/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2025 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2025 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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
#include "../Compression/ZlibCompressor.h"
#include "../MetricsRegistry.h"
#include "../OrthancException.h"
#include "../SerializationToolbox.h"
#include "../Toolbox.h"

#if ORTHANC_ENABLE_CIVETWEB == 1 || ORTHANC_ENABLE_MONGOOSE == 1
#  include "../HttpServer/HttpStreamTranscoder.h"
#endif

#include <boost/algorithm/string.hpp>


static const std::string METRICS_CREATE_DURATION = "orthanc_storage_create_duration_ms";
static const std::string METRICS_READ_DURATION = "orthanc_storage_read_duration_ms";
static const std::string METRICS_REMOVE_DURATION = "orthanc_storage_remove_duration_ms";
static const std::string METRICS_READ_BYTES = "orthanc_storage_read_bytes";
static const std::string METRICS_WRITTEN_BYTES = "orthanc_storage_written_bytes";
static const std::string METRICS_CACHE_HIT_COUNT = "orthanc_storage_cache_hit_count";
static const std::string METRICS_CACHE_MISS_COUNT = "orthanc_storage_cache_miss_count";


namespace Orthanc
{
  void StorageAccessor::Range::SanityCheck() const
  {
    if (hasStart_ && hasEnd_ && start_ > end_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }

  StorageAccessor::Range::Range():
    hasStart_(false),
    start_(0),
    hasEnd_(false),
    end_(0)
  {
  }

  void StorageAccessor::Range::SetStartInclusive(uint64_t start)
  {
    hasStart_ = true;
    start_ = start;
  }

  void StorageAccessor::Range::SetEndInclusive(uint64_t end)
  {
    hasEnd_ = true;
    end_ = end;
  }

  uint64_t StorageAccessor::Range::GetStartInclusive() const
  {
    if (!hasStart_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else if (hasEnd_ && start_ > end_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      return start_;
    }
  }

  uint64_t StorageAccessor::Range::GetEndInclusive() const
  {
    if (!hasEnd_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else if (hasStart_ && start_ > end_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      return end_;
    }
  }

  std::string StorageAccessor::Range::FormatHttpContentRange(uint64_t fullSize) const
  {
    SanityCheck();

    if (fullSize == 0 ||
        (hasStart_ && start_ >= fullSize) ||
        (hasEnd_ && end_ >= fullSize))
    {
      throw OrthancException(ErrorCode_BadRange);
    }

    std::string s = "bytes ";

    if (hasStart_)
    {
      s += boost::lexical_cast<std::string>(start_);
    }
    else
    {
      s += "0";
    }

    s += "-";

    if (hasEnd_)
    {
      s += boost::lexical_cast<std::string>(end_);
    }
    else
    {
      s += boost::lexical_cast<std::string>(fullSize - 1);
    }

    return s + "/" + boost::lexical_cast<std::string>(fullSize);
  }

  void StorageAccessor::Range::Extract(std::string &target,
                                       const std::string &source) const
  {
    SanityCheck();

    if (hasStart_ && start_ >= source.size())
    {
      throw OrthancException(ErrorCode_BadRange);
    }

    if (hasEnd_ && end_ >= source.size())
    {
      throw OrthancException(ErrorCode_BadRange);
    }

    if (hasStart_ && hasEnd_)
    {
      target = source.substr(start_, end_ - start_ + 1);
    }
    else if (hasStart_)
    {
      target = source.substr(start_, source.size() - start_);
    }
    else if (hasEnd_)
    {
      target = source.substr(0, end_ + 1);
    }
    else
    {
      target = source;
    }
  }

  uint64_t StorageAccessor::Range::GetContentLength(uint64_t fullSize) const
  {
    SanityCheck();

    if (fullSize == 0)
    {
      throw OrthancException(ErrorCode_BadRange);
    }

    if (hasStart_ && start_ >= fullSize)
    {
      throw OrthancException(ErrorCode_BadRange);
    }

    if (hasEnd_ && end_ >= fullSize)
    {
      throw OrthancException(ErrorCode_BadRange);
    }

    if (hasStart_ && hasEnd_)
    {
      return end_ - start_ + 1;
    }
    else if (hasStart_)
    {
      return fullSize - start_;
    }
    else if (hasEnd_)
    {
      return end_ + 1;
    }
    else
    {
      return fullSize;
    }
  }

  StorageAccessor::Range StorageAccessor::Range::ParseHttpRange(const std::string& s)
  {
    static const std::string BYTES = "bytes=";

    if (!boost::starts_with(s, BYTES))
    {
      throw OrthancException(ErrorCode_BadRange);  // Range not satisfiable
    }

    std::vector<std::string> tokens;
    Orthanc::Toolbox::TokenizeString(tokens, s.substr(BYTES.length()), '-');

    if (tokens.size() != 2)
    {
      throw OrthancException(ErrorCode_BadRange);
    }

    Range range;

    uint64_t tmp;
    if (!tokens[0].empty())
    {
      if (SerializationToolbox::ParseUnsignedInteger64(tmp, tokens[0]))
      {
        range.SetStartInclusive(tmp);
      }
    }

    if (!tokens[1].empty())
    {
      if (SerializationToolbox::ParseUnsignedInteger64(tmp, tokens[1]))
      {
        range.SetEndInclusive(tmp);
      }
    }

    range.SanityCheck();
    return range;
  }

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
          StorageCache::Accessor cacheAccessor(*cache_);
          cacheAccessor.Add(uuid, type, data, size);
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
          StorageCache::Accessor cacheAccessor(*cache_);
          cacheAccessor.Add(uuid, type, data, size);    // always add uncompressed data to cache
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
    if (cache_ == NULL)
    {
      ReadWholeInternal(content, info);
    }
    else
    {
      StorageCache::Accessor cacheAccessor(*cache_);

      if (!cacheAccessor.Fetch(content, info.GetUuid(), info.GetContentType()))
      {
        if (metrics_ != NULL)
        {
          metrics_->IncrementIntegerValue(METRICS_CACHE_MISS_COUNT, 1);
        }

        ReadWholeInternal(content, info);

        // always store the uncompressed data in cache
        cacheAccessor.Add(info.GetUuid(), info.GetContentType(), content);
      } 
      else if (metrics_ != NULL)
      {
        metrics_->IncrementIntegerValue(METRICS_CACHE_HIT_COUNT, 1);
      }
    }
  }

  void StorageAccessor::ReadWholeInternal(std::string& content,
                                          const FileInfo& info)
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

    // TODO Check the validity of the uncompressed MD5?
  }


  void StorageAccessor::ReadRaw(std::string& content,
                                const FileInfo& info)
  {
    if (cache_ == NULL || info.GetCompressionType() != CompressionType_None)
    {
      ReadRawInternal(content, info);
    }
    else
    {// use the cache only if the data is uncompressed.
      StorageCache::Accessor cacheAccessor(*cache_);

      if (!cacheAccessor.Fetch(content, info.GetUuid(), info.GetContentType()))
      {
        if (metrics_ != NULL)
        {
          metrics_->IncrementIntegerValue(METRICS_CACHE_MISS_COUNT, 1);
        }

        ReadRawInternal(content, info);

        cacheAccessor.Add(info.GetUuid(), info.GetContentType(), content);
      }
      else if (metrics_ != NULL)
      {
        metrics_->IncrementIntegerValue(METRICS_CACHE_HIT_COUNT, 1);
      }
    }
  }

  void StorageAccessor::ReadRawInternal(std::string& content,
                                        const FileInfo& info)
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
                                       const FileInfo& info,
                                       uint64_t end /* exclusive */)
  {
    if (cache_ == NULL)
    {
      ReadStartRangeInternal(target, info, end);
    }
    else
    {
      StorageCache::Accessor accessorStartRange(*cache_);
      if (!accessorStartRange.FetchStartRange(target, info.GetUuid(), info.GetContentType(), end))
      {
        // the start range is not in cache, let's check if the whole file is
        StorageCache::Accessor accessorWhole(*cache_);
        if (!accessorWhole.Fetch(target, info.GetUuid(), info.GetContentType()))
        {
          if (metrics_ != NULL)
          {
            metrics_->IncrementIntegerValue(METRICS_CACHE_MISS_COUNT, 1);
          }

          // if nothing is in the cache, let's read and cache only the start
          ReadStartRangeInternal(target, info, end);
          accessorStartRange.AddStartRange(info.GetUuid(), info.GetContentType(), target);
        }
        else
        {
          if (metrics_ != NULL)
          {
            metrics_->IncrementIntegerValue(METRICS_CACHE_HIT_COUNT, 1);
          }

          // we have read the whole file, check size and resize if needed
          if (target.size() < end)
          {
            throw OrthancException(ErrorCode_CorruptedFile);
          }

          target.resize(end);
        }
      }
      else if (metrics_ != NULL)
      {
        metrics_->IncrementIntegerValue(METRICS_CACHE_HIT_COUNT, 1);
      }
    }
  }

  void StorageAccessor::ReadStartRangeInternal(std::string& target,
                                                const FileInfo& info,
                                                uint64_t end /* exclusive */)
  {
    std::unique_ptr<IMemoryBuffer> buffer;

    {
      MetricsTimer timer(*this, METRICS_READ_DURATION);
      buffer.reset(area_.ReadRange(info.GetUuid(), info.GetContentType(), 0, end));
      assert(buffer->GetSize() == end);
    }

    if (metrics_ != NULL)
    {
      metrics_->IncrementIntegerValue(METRICS_READ_BYTES, buffer->GetSize());
    }

    buffer->MoveToString(target);
  }


  void StorageAccessor::ReadRange(std::string &target,
                                  const FileInfo &info,
                                  const Range &range,
                                  bool uncompressIfNeeded)
  {
    if (uncompressIfNeeded &&
        info.GetCompressionType() != CompressionType_None)
    {
      // An uncompression is needed in this case
      if (cache_ != NULL)
      {
        StorageCache::Accessor cacheAccessor(*cache_);

        std::string content;
        if (cacheAccessor.Fetch(content, info.GetUuid(), info.GetContentType()))
        {
          range.Extract(target, content);
          return;
        }
      }

      std::string content;
      Read(content, info);
      range.Extract(target, content);
    }
    else
    {
      // Access to the raw attachment is sufficient in this case
      if (info.GetCompressionType() == CompressionType_None &&
          cache_ != NULL)
      {
        // Check out whether the raw attachment is already present in the cache, by chance
        StorageCache::Accessor cacheAccessor(*cache_);

        std::string content;
        if (cacheAccessor.Fetch(content, info.GetUuid(), info.GetContentType()))
        {
          range.Extract(target, content);
          return;
        }
      }

      if (range.HasEnd() &&
        range.GetEndInclusive() >= info.GetCompressedSize())
      {
        throw OrthancException(ErrorCode_BadRange);
      }

      std::unique_ptr<IMemoryBuffer> buffer;

      if (range.HasStart() &&
          range.HasEnd())
      {
        buffer.reset(area_.ReadRange(info.GetUuid(), info.GetContentType(), range.GetStartInclusive(), range.GetEndInclusive() + 1));
      }
      else if (range.HasStart())
      {
        buffer.reset(area_.ReadRange(info.GetUuid(), info.GetContentType(), range.GetStartInclusive(), info.GetCompressedSize()));
      }
      else if (range.HasEnd())
      {
        buffer.reset(area_.ReadRange(info.GetUuid(), info.GetContentType(), 0, range.GetEndInclusive() + 1));
      }
      else
      {
        buffer.reset(area_.Read(info.GetUuid(), info.GetContentType()));
      }

      buffer->MoveToString(target);
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
