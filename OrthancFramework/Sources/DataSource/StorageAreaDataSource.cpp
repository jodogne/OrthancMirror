/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2026 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2026 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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
#include "StorageAreaDataSource.h"

#include "../Cache/SharedObjectCache.h"
#include "../Logging.h"
#include "../MetricsRegistry.h"
#include "../OrthancException.h"
#include "../StringMemoryBuffer.h"
#include "../Toolbox.h"
#include "BaseDataIdentifier.h"
#include "DataSourceReader.h"

#if ORTHANC_ENABLE_ZLIB == 1
#  include "../Compression/ZlibCompressor.h"
#endif

#include <boost/lexical_cast.hpp>
#include <cassert>


namespace Orthanc
{
  static std::string ComputeCacheKey(std::string uuid,
                                     FileContentType type,
                                     uint64_t start,
                                     uint64_t end)
  {
    // custom data is not part of the cache key, as it is for internal use by the plugin
    // (it is opaque to the Orthanc core)
    return (uuid + "|" +
            boost::lexical_cast<std::string>(type) + "|" +
            boost::lexical_cast<std::string>(start) + "|" +
            boost::lexical_cast<std::string>(end));
  }


  static std::string ComputeCacheKeyForWholeAttachment(const FileInfo& attachment)
  {
    return ComputeCacheKey(attachment.GetUuid(), attachment.GetContentType(), 0, attachment.GetCompressedSize());
  }


  class StorageAreaDataSource::Value : public IDynamicObject
  {
  private:
    boost::shared_ptr<IMemoryBuffer>  buffer_;
    bool                              checkMD5_;

  public:
    explicit Value(IMemoryBuffer* buffer,
                   bool checkMD5) :
      buffer_(buffer),
      checkMD5_(checkMD5)
    {
      if (buffer == NULL)
      {
        throw OrthancException(ErrorCode_NullPointer);
      }
    }

    const boost::shared_ptr<IMemoryBuffer>& GetBuffer() const
    {
      assert(buffer_.get() != NULL);
      return buffer_;
    }

    bool IsCheckMD5() const
    {
      return checkMD5_;
    }

    void ExtractRange(std::string& target,
                      uint64_t start,
                      uint64_t end) const
    {
      if (start >= buffer_->GetSize() ||
          end > buffer_->GetSize())
      {
        throw OrthancException(ErrorCode_BadRange);
      }
      else
      {
        target.assign(reinterpret_cast<const char*>(buffer_->GetData()) + start, end - start);
      }
    }
  };


  class StorageAreaDataSource::IPostProcessing : public boost::noncopyable
  {
  public:
    virtual ~IPostProcessing()
    {
    }

    // This method can return NULL if no post-processing is required
    virtual IMemoryBuffer* Apply(const Value& value) const = 0;
  };


  class StorageAreaDataSource::Identifier : public BaseDataIdentifier
  {
  private:
    std::string                       uuid_;
    FileContentType                   type_;
    uint64_t                          start_;
    uint64_t                          end_;
    std::string                       customData_;
    std::unique_ptr<IPostProcessing>  postProcessing_;
    bool                              hasWholeKey_;
    std::string                       wholeKey_;

  public:
    Identifier(const std::string& uuid,
               FileContentType type,
               uint64_t start,
               uint64_t end,
               const std::string& customData) :
      uuid_(uuid),
      type_(type),
      start_(start),
      end_(end),
      customData_(customData),
      hasWholeKey_(false)
    {
      if (start > end)
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange);
      }

      uint64_t size = end - start;
      if (static_cast<uint64_t>(static_cast<size_t>(size)) != size)
      {
        throw OrthancException(ErrorCode_NotEnoughMemory);  // For 32bit architectures
      }
    }

    uint64_t GetStart() const
    {
      return start_;
    }

    uint64_t GetEnd() const
    {
      return end_;
    }

    /**
     * WARNING: SetWholeKey() must only be used to retrieve compressed
     * date from the storage area (or if there is no compression).
     **/
    void SetWholeKey(const std::string& key)
    {
      if (hasWholeKey_)
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }
      else if (key.empty())
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
      else
      {
        hasWholeKey_ = true;
        wholeKey_ = key;
      }
    }

    bool HasWholeKey() const
    {
      return hasWholeKey_;
    }

    const std::string GetWholeKey() const
    {
      if (hasWholeKey_)
      {
        return wholeKey_;
      }
      else
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }
    }

    IMemoryBuffer* Read(IPluginStorageArea& area) const
    {
      return area.ReadRange(uuid_, type_, start_, end_, customData_);
    }

    virtual bool GetCacheKey(std::string& key) const ORTHANC_OVERRIDE
    {
      key = ComputeCacheKey(uuid_, type_, start_, end_);
      return true;
    }

    virtual bool EstimateValueSize(size_t& target) const ORTHANC_OVERRIDE
    {
      assert(start_ <= end_);
      target = end_ - start_;
      return true;
    }

    void SetPostProcessing(IPostProcessing* postProcessing)
    {
      std::unique_ptr<IPostProcessing> protection(postProcessing);

      if (postProcessing == NULL)
      {
        throw OrthancException(ErrorCode_NullPointer);
      }
      else if (postProcessing_.get() != NULL)
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }
      else
      {
        postProcessing_.reset(protection.release());
      }
    }

    bool HasPostProcessing() const
    {
      return postProcessing_.get() != NULL;
    }

    const IPostProcessing& GetPostProcessing() const
    {
      if (postProcessing_.get() == NULL)
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }
      else
      {
        return *postProcessing_;
      }
    }
  };


  class StorageAreaDataSource::AttachmentPostProcessing : public StorageAreaDataSource::IPostProcessing
  {
  private:
    FileInfo  attachment_;
    bool      uncompress_;

  public:
    AttachmentPostProcessing(const FileInfo& attachment,
                             bool uncompress) :
      attachment_(attachment),
      uncompress_(uncompress)
    {
      if (attachment.GetCompressionType() == CompressionType_None)
      {
        if (attachment.GetCompressedMD5() != attachment.GetUncompressedMD5() ||
            attachment.GetCompressedSize() != attachment.GetUncompressedSize())
        {
          throw OrthancException(ErrorCode_CorruptedFile);
        }
      }
    }

    const FileInfo& GetAttachment() const
    {
      return attachment_;
    }

    virtual IMemoryBuffer* Apply(const Value& value) const ORTHANC_OVERRIDE
    {
      if (value.IsCheckMD5())
      {
        std::string md5;
        Toolbox::ComputeMD5(md5, value.GetBuffer()->GetData(), value.GetBuffer()->GetSize());

        if (md5 != attachment_.GetCompressedMD5())
        {
          throw OrthancException(ErrorCode_CorruptedFile);
        }
      }

      if (uncompress_)
      {
        switch (attachment_.GetCompressionType())
        {
          case CompressionType_None:
            if (value.IsCheckMD5() &&
                attachment_.GetCompressedMD5() != attachment_.GetUncompressedMD5())
            {
              throw OrthancException(ErrorCode_CorruptedFile);
            }
            else
            {
              return NULL;
            }

          case CompressionType_ZlibWithSize:
          {
#if ORTHANC_ENABLE_ZLIB == 1
            ZlibCompressor zlib;

            std::string content;
            zlib.Uncompress(content, value.GetBuffer()->GetData(), value.GetBuffer()->GetSize());

            if (value.IsCheckMD5())
            {
              std::string md5;
              Toolbox::ComputeMD5(md5, content);

              if (md5 != attachment_.GetUncompressedMD5())
              {
                throw OrthancException(ErrorCode_CorruptedFile);
              }
            }

            return StringMemoryBuffer::CreateFromSwap(content);
#else
            throw OrthancException(ErrorCode_InternalError, "Support for zlib is disabled, cannot uncompress attachment");
#endif
          }

          default:
            throw OrthancException(ErrorCode_NotImplemented);
        }
      }
      else
      {
        return NULL;
      }
    }
  };


  StorageAreaDataSource::StorageAreaDataSource(IPluginStorageArea& area,
                                               bool checkMD5) :
    area_(area),
    checkMD5_(checkMD5)
  {
  }


  void StorageAreaDataSource::SetMetricsRegistry(const boost::shared_ptr<MetricsRegistry>& metrics,
                                                 const std::string& metricsReadBytesName,
                                                 const std::string& metricsReadDurationName)
  {
    if (metrics.get() == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
    else
    {
      metrics_ = metrics;
      metricsReadBytesName_ = metricsReadBytesName;
      metricsReadDurationName_ = metricsReadDurationName;

      metrics_->SetIntegerValue(metricsReadBytesName_, 0);
      metrics_->SetIntegerValue(metricsReadDurationName_, 0);
    }
  }


  size_t StorageAreaDataSource::GetValueSize(const IDynamicObject& value) const
  {
    const Value& tmp = dynamic_cast<const Value&>(value);
    return tmp.GetBuffer()->GetSize();
  }


  IDynamicObject* StorageAreaDataSource::Load(const IDataIdentifier& identifier,
                                              const boost::shared_ptr<SharedObjectCache>& readerCache)
  {
    const Identifier& id = dynamic_cast<const Identifier&>(identifier);

    if (id.HasWholeKey() &&
        readerCache)
    {
      // Extract the request range from the whole compressed attachment if the latter is already present in the cache
      boost::shared_ptr<IDynamicObject> wholeCached = readerCache->GetCachedValue(id.GetWholeKey());
      if (wholeCached)
      {
        const Value& wholeRange = dynamic_cast<const Value&>(*wholeCached);

        std::string content;
        wholeRange.ExtractRange(content, id.GetStart(), id.GetEnd());

        return new Value(StringMemoryBuffer::CreateFromSwap(content), false);
      }
    }

    std::unique_ptr<IMemoryBuffer> buffer;

    {
      std::unique_ptr<MetricsRegistry::Timer> timer;
      if (metrics_)
      {
        timer.reset(new MetricsRegistry::Timer(*metrics_, metricsReadDurationName_));
      }

      buffer.reset(id.Read(area_));
    }

    if (metrics_)
    {
      metrics_->IncrementIntegerValue(metricsReadBytesName_, static_cast<int64_t>(buffer->GetSize()));
    }

    return new Value(buffer.release(), checkMD5_);
  }


  const boost::shared_ptr<IMemoryBuffer>& StorageAreaDataSource::Range::GetBuffer() const
  {
    if (item_.get() != NULL)
    {
      assert(buffer_.get() == NULL);
      return dynamic_cast<const Value&>(*item_->GetValue()).GetBuffer();
    }
    else if (buffer_.get() != NULL)
    {
      assert(item_.get() == NULL);
      return buffer_;
    }
    else
    {
      throw OrthancException(ErrorCode_InternalError);
    }
  }


  StorageAreaDataSource::Range::Range(IMemoryBuffer* buffer) :
    buffer_(buffer)
  {
    if (buffer == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
  }


  StorageAreaDataSource::Range::Range(DataSourceAnswer::Item* item) :
    item_(item)
  {
    if (item == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }

    const Identifier& id = dynamic_cast<const Identifier&>(item->GetId());

    if (id.HasPostProcessing())
    {
      const Value& value = dynamic_cast<const Value&>(*item->GetValue());
      std::unique_ptr<IMemoryBuffer> postProcessed(id.GetPostProcessing().Apply(value));

      if (postProcessed.get() != NULL)
      {
        // In this case, the backpressure on the source "item" is released
        item_.reset(NULL);
        buffer_.reset(postProcessed.release());
      }
      else
      {
        // No postprocessing was applied, keep backpressure on "item"
      }
    }
  }


  IDataIdentifier* StorageAreaDataSource::CreateRangeRequest(const std::string& uuid,
                                                             FileContentType type,
                                                             uint64_t start,
                                                             uint64_t end,
                                                             const std::string& pluginCustomData)
  {
    return new Identifier(uuid, type, start, end, pluginCustomData);
  }


  IDataIdentifier* StorageAreaDataSource::CreateAttachmentRequest(const FileInfo& attachment,
                                                                  bool uncompress)
  {
    std::unique_ptr<Identifier> id(new Identifier(attachment.GetUuid(),
                                                  attachment.GetContentType(),
                                                  0, attachment.GetCompressedSize(),
                                                  attachment.GetCustomData()));
    id->SetPostProcessing(new AttachmentPostProcessing(attachment, uncompress));

    return id.release();
  }


  IDataIdentifier* StorageAreaDataSource::CreateBeginningRequest(const FileInfo& attachment,
                                                                 uint64_t untilPosition)
  {
    if (attachment.GetCompressionType() != CompressionType_None)
    {
      throw OrthancException(ErrorCode_BadParameterType);
    }

    if (attachment.GetCompressedMD5() != attachment.GetUncompressedMD5() ||
        attachment.GetCompressedSize() != attachment.GetUncompressedSize())
    {
      throw OrthancException(ErrorCode_CorruptedFile);
    }

    if (untilPosition > attachment.GetUncompressedSize())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    std::unique_ptr<Identifier> id(new Identifier(attachment.GetUuid(), attachment.GetContentType(),
                                                  0, untilPosition, attachment.GetCustomData()));

    // Using "SetWholeKey()" allows to extract a range if the whole attachment is already in the reader cache
    assert(attachment.GetCompressionType() == CompressionType_None);
    id->SetWholeKey(ComputeCacheKeyForWholeAttachment(attachment));

    return id.release();
  }


  StorageAreaDataSource::Range* StorageAreaDataSource::ReadRange(DataSourceReader& reader,
                                                                 const FileInfo& attachment,
                                                                 const StorageRange& range,
                                                                 bool uncompress)
  {
    // This mimics "StorageAccessor::ReadRange()"

    if (uncompress &&
        attachment.GetCompressionType() != CompressionType_None)
    {
      // An uncompression is needed in this case
      std::unique_ptr<Range> uncompressed(
        Execute(reader, CreateAttachmentRequest(attachment, true /* uncompress */)));

      std::string content;
      range.Extract(content, uncompressed->GetData(), uncompressed->GetSize());

      return new Range(StringMemoryBuffer::CreateFromSwap(content));
    }
    else
    {
      // Access to the raw attachment is sufficient in this case, but
      // no MD5 checking can be done in general
      const uint64_t start = range.HasStart() ? range.GetStartInclusive() : 0;
      const uint64_t endExclusive = range.HasEnd() ? range.GetEndInclusive() + 1 : attachment.GetCompressedSize();

      if (start >= attachment.GetCompressedSize() ||
          endExclusive > attachment.GetCompressedSize())
      {
        throw OrthancException(ErrorCode_BadRange);
      }
      else
      {
        std::unique_ptr<Identifier> id(new Identifier(attachment.GetUuid(), attachment.GetContentType(),
                                                      start, endExclusive, attachment.GetCustomData()));

        // Using "SetWholeKey()" allows to extract a range if the whole compressed attachment is already in the reader cache
        assert(!uncompress || attachment.GetCompressionType() == CompressionType_None);
        id->SetWholeKey(ComputeCacheKeyForWholeAttachment(attachment));

        return Execute(reader, id.release());
      }
    }
  }


  StorageAreaDataSource::Range* StorageAreaDataSource::Execute(DataSourceReader& reader,
                                                               IDataIdentifier* request /* takes ownership */)
  {
    if (request == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
    else
    {
      return new Range(reader.ReadSingle(request));
    }
  }


  void StorageAreaDataSource::StoreIntoCache(DataSourceReader& reader,
                                             const FileInfo& attachment,
                                             const void* data,
                                             size_t size)
  {
    if (size != attachment.GetCompressedSize())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    const size_t capacity = reader.GetCacheCapacity();

    if (capacity != 0 &&
        size <= capacity)
    {
      const std::string key = ComputeCacheKeyForWholeAttachment(attachment);
      std::unique_ptr<Value> value(new Value(StringMemoryBuffer::CreateFromBuffer(data, size), false /* no MD5 */));

      reader.StoreIntoCache(key, value.release());
    }
  }
}
