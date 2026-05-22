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

    static Value* CreateFromSwap(std::string& content)
    {
      return new Value(StringMemoryBuffer::CreateFromSwap(content), false);
    }
  };


  class StorageAreaDataSource::IPostProcessing : public boost::noncopyable
  {
  public:
    virtual ~IPostProcessing()
    {
    }

    virtual boost::shared_ptr<IDynamicObject> Apply(const boost::shared_ptr<IDynamicObject>& value) const = 0;
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
      if (postProcessing == NULL)
      {
        throw OrthancException(ErrorCode_NullPointer);
      }
      else
      {
        postProcessing_.reset(postProcessing);
      }
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

    boost::shared_ptr<IDynamicObject> ApplyPostProcessing(const boost::shared_ptr<IDynamicObject>& value) const
    {
      if (value.get() == NULL)
      {
        throw OrthancException(ErrorCode_NullPointer);
      }
      else if (postProcessing_.get() == NULL)
      {
        return value;
      }
      else
      {
        return postProcessing_->Apply(value);
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

    virtual boost::shared_ptr<IDynamicObject> Apply(const boost::shared_ptr<IDynamicObject>& obj) const ORTHANC_OVERRIDE
    {
      if (obj.get() == NULL)
      {
        throw OrthancException(ErrorCode_NullPointer);
      }
      else
      {
        const Value& value = dynamic_cast<Value&>(*obj);

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
                return obj;
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

              return boost::shared_ptr<IDynamicObject>(Value::CreateFromSwap(content));
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
          return obj;
        }
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


  const StorageAreaDataSource::Value& StorageAreaDataSource::Range::GetValue() const
  {
    assert(value_.get() != NULL);
    return dynamic_cast<const StorageAreaDataSource::Value&>(*value_);
  }


  StorageAreaDataSource::Range::Range(const boost::shared_ptr<IDynamicObject>& value) :
    value_(value)
  {
    if (value_.get() == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
  }


  const void* StorageAreaDataSource::Range::GetData() const
  {
    return GetValue().GetBuffer().GetData();
  }


  size_t StorageAreaDataSource::Range::GetSize() const
  {
    return GetValue().GetBuffer().GetSize();
  }


  void StorageAreaDataSource::Range::Copy(std::string& to) const
  {
    const IMemoryBuffer& buffer = GetValue().GetBuffer();
    to.assign(reinterpret_cast<const char*>(buffer.GetData()), buffer.GetSize());
  }


  StorageAreaDataSource::Range* StorageAreaDataSource::Range::ApplyPostProcessing(const IDataIdentifier& identifier) const
  {
    return new Range(dynamic_cast<const Identifier&>(identifier).ApplyPostProcessing(value_));
  }


  StorageAreaDataSource::Range* StorageAreaDataSource::Range::CreateFromSwap(std::string& content)
  {
    boost::shared_ptr<Value> value(new Value(StringMemoryBuffer::CreateFromSwap(content), false));
    return new Range(value);
  }


  StorageAreaDataSource::Range* StorageAreaDataSource::ReadRange(DataSourceReader& reader,
                                                                 const std::string& uuid,
                                                                 FileContentType type,
                                                                 uint64_t start,
                                                                 uint64_t end,
                                                                 const std::string& customData)
  {
    std::unique_ptr<IDataIdentifier> id(new Identifier(uuid, type, start, end, customData));
    boost::shared_ptr<IDynamicObject> value = reader.ReadSingle(id.release());
    return new Range(value);
  }


  StorageAreaDataSource::Range* StorageAreaDataSource::ReadAttachment(DataSourceReader& reader,
                                                                      const FileInfo& attachment,
                                                                      bool uncompress)
  {
    std::unique_ptr<Identifier> id(new Identifier(attachment.GetUuid(),
                                                  attachment.GetContentType(),
                                                  0, attachment.GetCompressedSize(),
                                                  attachment.GetCustomData()));
    id->SetPostProcessing(new AttachmentPostProcessing(attachment, uncompress));

    std::unique_ptr<DataSourceAnswer::Item> item(reader.ReadSingleWithIdentifier(id.release()));

    std::unique_ptr<StorageAreaDataSource::Range> range(new Range(item->GetValue()));

    return range->ApplyPostProcessing(item->GetId());
  }


  StorageAreaDataSource::Range* StorageAreaDataSource::ReadBeginning(DataSourceReader& reader,
                                                                     const FileInfo& attachment,
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

    return ReadRange(reader, attachment.GetUuid(), attachment.GetContentType(),
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
      std::unique_ptr<StorageAreaDataSource::Range> uncompressed(
        ReadAttachment(reader, attachment, true /* uncompress */));

      std::string content;
      range.Extract(content, uncompressed->GetData(), uncompressed->GetSize());

      return Range::CreateFromSwap(content);
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
        return ReadRange(reader, attachment.GetUuid(), attachment.GetContentType(),
                         start, endExclusive, attachment.GetCustomData());
      }
    }
  }


  class StorageAreaDataSource::MultipleReader::PImpl : public boost::noncopyable
  {
  private:
    boost::shared_ptr<DataSourceReader>  reader_;
    std::unique_ptr<DataSourceRequest>   request_;
    boost::shared_ptr<DataSourceAnswer>  answer_;

  public:
    PImpl(const boost::shared_ptr<DataSourceReader>& reader) :
      reader_(reader),
      request_(new DataSourceRequest)
    {
    }

    void Enqueue(const FileInfo& attachment,
                 bool uncompress)
    {
      if (request_.get() == NULL)
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }
      else
      {
        std::unique_ptr<Identifier> id(new Identifier(attachment.GetUuid(),
                                                      attachment.GetContentType(),
                                                      0, attachment.GetCompressedSize(),
                                                      attachment.GetCustomData()));
        id->SetPostProcessing(new AttachmentPostProcessing(attachment, uncompress));

        request_->Enqueue(id.release());
      }
    }

    Item* Dequeue()
    {
      if (answer_.get() == NULL)
      {
        assert(request_.get() != NULL);
        answer_ = reader_->Submit(request_.release());
      }

      std::unique_ptr<DataSourceAnswer::Item> item(answer_->Dequeue());

      std::unique_ptr<StorageAreaDataSource::Range> range(new Range(item->GetValue()));

      const Identifier& id = dynamic_cast<const Identifier&>(item->GetId());
      range.reset(range->ApplyPostProcessing(id));

      const AttachmentPostProcessing& postProcessing = dynamic_cast<const AttachmentPostProcessing&>(id.GetPostProcessing());
      return new Item(range.release(), postProcessing.GetAttachment());
    }
  };


  StorageAreaDataSource::MultipleReader::Item::Item(Range* range,
                                                    const FileInfo& attachment) :
    range_(range),
    attachment_(attachment)
  {
  }


  StorageAreaDataSource::MultipleReader::MultipleReader(const boost::shared_ptr<DataSourceReader>& reader) :
    pimpl_(new PImpl(reader))
  {
  }


  StorageAreaDataSource::MultipleReader::~MultipleReader()
  {
    assert(pimpl_ != NULL);
    delete pimpl_;
  }


  void StorageAreaDataSource::MultipleReader::Enqueue(const FileInfo& attachment,
                                                      bool uncompress)
  {
    pimpl_->Enqueue(attachment, uncompress);
  }


  StorageAreaDataSource::MultipleReader::Item* StorageAreaDataSource::MultipleReader::Dequeue()
  {
    return pimpl_->Dequeue();
  }
}
