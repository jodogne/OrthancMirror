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
#include "HttpStreamTranscoder.h"

#include "../OrthancException.h"
#include "../Compression/ZlibCompressor.h"

#include <string.h>   // For memcpy()
#include <cassert>

#include <stdio.h>

namespace Orthanc
{
  void HttpStreamTranscoder::ReadSource(std::string& buffer)
  {
    if (source_.SetupHttpCompression(false, false) != HttpCompression_None)
    {
      throw OrthancException(ErrorCode_InternalError);
    }
    
    uint64_t size = source_.GetContentLength();
    if (static_cast<uint64_t>(static_cast<size_t>(size)) != size)
    {
      throw OrthancException(ErrorCode_NotEnoughMemory);
    }

    buffer.resize(static_cast<size_t>(size));
    size_t offset = 0;

    while (source_.ReadNextChunk())
    {
      size_t chunkSize = static_cast<size_t>(source_.GetChunkSize());
      memcpy(&buffer[offset], source_.GetChunkContent(), chunkSize);
      offset += chunkSize;
    }

    if (offset != size)
    {
      throw OrthancException(ErrorCode_InternalError);
    }
  }


  HttpCompression HttpStreamTranscoder::SetupZlibCompression(bool deflateAllowed)
  {
    uint64_t size = source_.GetContentLength();

    if (size == 0)
    {
      return HttpCompression_None;
    }

    if (size < sizeof(uint64_t))
    {
      throw OrthancException(ErrorCode_CorruptedFile);
    }

    if (deflateAllowed)
    {
      bytesToSkip_ = sizeof(uint64_t);

      return HttpCompression_Deflate;
    }
    else
    {
      // TODO Use stream-based zlib decoding to reduce memory usage
      std::string compressed;
      ReadSource(compressed);

      uncompressed_.reset(new BufferHttpSender);

      ZlibCompressor compressor;
      IBufferCompressor::Uncompress(uncompressed_->GetBuffer(), compressor, compressed);

      return HttpCompression_None;
    }
  }

  HttpStreamTranscoder::HttpStreamTranscoder(IHttpStreamAnswer &source, CompressionType compression) :
    source_(source),
    sourceCompression_(compression),
    bytesToSkip_(0),
    skipped_(0),
    currentChunkOffset_(0),
    ready_(false)
  {
  }


  HttpCompression HttpStreamTranscoder::SetupHttpCompression(bool gzipAllowed,
                                                             bool deflateAllowed)
  {
    if (ready_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    ready_ = true;

    switch (sourceCompression_)
    {
      case CompressionType_None:
        return HttpCompression_None;

      case CompressionType_ZlibWithSize:
        return SetupZlibCompression(deflateAllowed);

      default:
        throw OrthancException(ErrorCode_NotImplemented);
    }
  }

  bool HttpStreamTranscoder::HasContentFilename(std::string &filename)
  {
    return source_.HasContentFilename(filename);
  }

  std::string HttpStreamTranscoder::GetContentType()
  {
    return source_.GetContentType();
  }


  uint64_t HttpStreamTranscoder::GetContentLength()
  {
    if (!ready_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    if (uncompressed_.get() != NULL)
    {
      return uncompressed_->GetContentLength();
    }
    else
    {
      uint64_t length = source_.GetContentLength();
      if (length < bytesToSkip_)
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      return length - bytesToSkip_;
    }
  }


  bool HttpStreamTranscoder::ReadNextChunk()
  {
    if (!ready_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    if (uncompressed_.get() != NULL)
    {
      return uncompressed_->ReadNextChunk();
    }

    assert(skipped_ <= bytesToSkip_);
    if (skipped_ == bytesToSkip_)
    {
      // We have already skipped the first bytes of the stream
      currentChunkOffset_ = 0;
      return source_.ReadNextChunk();
    }

    // This condition can only be true on the first call to "ReadNextChunk()"
    for (;;)
    {
      assert(skipped_ < bytesToSkip_);

      bool ok = source_.ReadNextChunk();
      if (!ok)
      {
        throw OrthancException(ErrorCode_CorruptedFile);
      }

      size_t remaining = static_cast<size_t>(bytesToSkip_ - skipped_);
      size_t s = source_.GetChunkSize();

      if (s < remaining)
      {
        skipped_ += s;
      }
      else if (s == remaining)
      {
        // We have skipped enough bytes, but we must read a new chunk
        currentChunkOffset_ = 0;            
        skipped_ = bytesToSkip_;
        return source_.ReadNextChunk();
      }
      else
      {
        // We have skipped enough bytes, and we have enough data in the current chunk
        assert(s > remaining);
        currentChunkOffset_ = remaining;
        skipped_ = bytesToSkip_;
        return true;
      }
    }
  }


  const char* HttpStreamTranscoder::GetChunkContent()
  {
    if (!ready_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    if (uncompressed_.get() != NULL)
    {
      return uncompressed_->GetChunkContent();
    }
    else
    {
      return source_.GetChunkContent() + currentChunkOffset_;
    }
  }

  size_t HttpStreamTranscoder::GetChunkSize()
  {
    if (!ready_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    if (uncompressed_.get() != NULL)
    {
      return uncompressed_->GetChunkSize();
    }
    else
    {
      return static_cast<size_t>(source_.GetChunkSize() - currentChunkOffset_);
    }
  }
}
