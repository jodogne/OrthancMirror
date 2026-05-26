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
  class StorageAreaDataSource::Value : public IDynamicObject
  {
  private:
    std::unique_ptr<IMemoryBuffer>  buffer_;
    bool                            checkMD5_;

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

    const IMemoryBuffer& GetBuffer() const
    {
      assert(buffer_.get() != NULL);
      return *buffer_;
    }

    bool IsCheckMD5() const
    {
      return checkMD5_;
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
      customData_(customData)
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

    IMemoryBuffer* Read(IPluginStorageArea& area) const
    {
      return area.ReadRange(uuid_, type_, start_, end_, customData_);
    }

    virtual bool GetCacheKey(std::string& key) const ORTHANC_OVERRIDE
    {
      // custom data is not part of the cache key, as it is for internal use by the plugin
      // (it is opaque to the Orthanc core)
      key = (uuid_ + "|" +
             boost::lexical_cast<std::string>(type_) + "|" +
             boost::lexical_cast<std::string>(start_) + "|" +
             boost::lexical_cast<std::string>(end_));
      return true;
    }

    virtual bool EstimateValueSize(size_t& target) const ORTHANC_OVERRIDE
    {
      assert(start_ <= end_);
      return end_ - start_;
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
        Toolbox::ComputeMD5(md5, value.GetBuffer().GetData(), value.GetBuffer().GetSize());

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
            zlib.Uncompress(content, value.GetBuffer().GetData(), value.GetBuffer().GetSize());

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


  size_t StorageAreaDataSource::GetValueSize(const IDynamicObject& value) const
  {
    const Value& tmp = dynamic_cast<const Value&>(value);
    return tmp.GetBuffer().GetSize();
  }


  IDynamicObject* StorageAreaDataSource::Load(const IDataIdentifier& identifier)
  {
    const Identifier& id = dynamic_cast<const Identifier&>(identifier);
    return new Value(id.Read(area_), checkMD5_);
  }


  const IMemoryBuffer& StorageAreaDataSource::Range::GetBuffer() const
  {
    if (item_.get() != NULL)
    {
      assert(buffer_.get() == NULL);
      return dynamic_cast<const Value&>(*item_->GetValue()).GetBuffer();
    }
    else if (buffer_.get() != NULL)
    {
      assert(item_.get() == NULL);
      return *buffer_;
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

    return CreateRangeRequest(attachment.GetUuid(), attachment.GetContentType(),
                              0, untilPosition, attachment.GetCustomData());
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
        return Execute(reader, CreateRangeRequest(attachment.GetUuid(), attachment.GetContentType(),
                                                  start, endExclusive, attachment.GetCustomData()));
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
}
