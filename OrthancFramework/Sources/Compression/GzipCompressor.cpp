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
#include "GzipCompressor.h"

#include <stdio.h>
#include <string.h>
#include <zlib.h>

#include "../ChunkedBuffer.h"
#include "../Logging.h"
#include "../MultiThreading/ReaderWriterLock.h"
#include "../OrthancException.h"


static Orthanc::ReaderWriterLock maximumUncompressedFileSizeMutex_;
static bool hasMaximumUncompressedFileSize_ = false;
static size_t maximumUncompressedFileSize_ = 0;


namespace Orthanc
{
  uint64_t GzipCompressor::GuessUncompressedSize(const void* compressed,
                                                 size_t compressedSize)
  {
    /**
     * "Is there a way to find out the size of the original file which
     * is inside a GZIP file? [...] There is no truly reliable way,
     * other than gunzipping the stream. You do not need to save the
     * result of the decompression, so you can determine the size by
     * simply reading and decoding the entire file without taking up
     * space with the decompressed result.
     *
     * There is an unreliable way to determine the uncompressed size,
     * which is to look at the last four bytes of the gzip file, which
     * is the uncompressed length of that entry modulo 2^32 in little
     * endian order.
     * 
     * It is unreliable because a) the uncompressed data may be longer
     * than 2^32 bytes, and b) the gzip file may consist of multiple
     * gzip streams, in which case you would find the length of only
     * the last of those streams.
     * 
     * If you are in control of the source of the gzip files, you know
     * that they consist of single gzip streams, and you know that
     * they are less than 2^32 bytes uncompressed, then and only then
     * can you use those last four bytes with confidence."
     *
     * http://stackoverflow.com/a/9727599/881731
     **/

    if (compressedSize < 4)
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    const uint8_t* p = reinterpret_cast<const uint8_t*>(compressed) + compressedSize - 4;

    return ((static_cast<uint32_t>(p[0]) << 0) +
            (static_cast<uint32_t>(p[1]) << 8) +
            (static_cast<uint32_t>(p[2]) << 16) +
            (static_cast<uint32_t>(p[3]) << 24));            
  }


  GzipCompressor::GzipCompressor()
  {
    SetPrefixWithUncompressedSize(false);
  }


  void GzipCompressor::Compress(std::string& compressed,
                                const void* uncompressed,
                                size_t uncompressedSize)
  {
    uLongf compressedSize = compressBound(static_cast<uLong>(uncompressedSize))
      + 1024 /* security margin */;
    
    if (compressedSize == 0)
    {
      compressedSize = 1;
    }

    uint8_t* target;
    if (HasPrefixWithUncompressedSize())
    {
      compressed.resize(compressedSize + sizeof(uint64_t));
      target = reinterpret_cast<uint8_t*>(&compressed[0]) + sizeof(uint64_t);
    }
    else
    {
      compressed.resize(compressedSize);
      target = reinterpret_cast<uint8_t*>(&compressed[0]);
    }

    z_stream stream;
    memset(&stream, 0, sizeof(stream));

    stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(uncompressed));
    stream.next_out = reinterpret_cast<Bytef*>(target);

    stream.avail_in = static_cast<uInt>(uncompressedSize);
    stream.avail_out = static_cast<uInt>(compressedSize);

    // Ensure no overflow (if the buffer is too large for the current archicture)
    if (static_cast<size_t>(stream.avail_in) != uncompressedSize ||
        static_cast<size_t>(stream.avail_out) != compressedSize)
    {
      throw OrthancException(ErrorCode_NotEnoughMemory);
    }
    
    // Initialize the compression engine
    int error = deflateInit2(&stream, 
                             GetCompressionLevel(), 
                             Z_DEFLATED,
                             MAX_WBITS + 16,      // ask for gzip output
                             8,                   // default memory level
                             Z_DEFAULT_STRATEGY);

    if (error != Z_OK)
    {
      // Cannot initialize zlib
      compressed.clear();
      throw OrthancException(ErrorCode_InternalError);
    }

    // Compress the input buffer
    error = deflate(&stream, Z_FINISH);

    if (error != Z_STREAM_END)
    {
      deflateEnd(&stream);
      compressed.clear();

      switch (error)
      {
      case Z_MEM_ERROR:
        throw OrthancException(ErrorCode_NotEnoughMemory);

      default:
        throw OrthancException(ErrorCode_InternalError);
      }  
    }

    size_t size = stream.total_out;

    if (deflateEnd(&stream) != Z_OK)
    {
      throw OrthancException(ErrorCode_InternalError);
    }

    // The compression was successful
    if (HasPrefixWithUncompressedSize())
    {
      uint64_t s = static_cast<uint64_t>(uncompressedSize);
      memcpy(&compressed[0], &s, sizeof(uint64_t));
      compressed.resize(size + sizeof(uint64_t));
    }
    else
    {
      compressed.resize(size);
    }
  }


  namespace
  {
    class GzipRaii : public boost::noncopyable
    {
    private:
      z_stream&  stream_;

    public:
      GzipRaii(z_stream& stream) :
        stream_(stream)
      {
        int error = inflateInit2(&stream_,
                                 MAX_WBITS + 16);  // this is a gzip input

        if (error != Z_OK)
        {
          throw OrthancException(ErrorCode_InternalError, "Cannot initialize zlib");
        }
      }

      ~GzipRaii()
      {
        inflateEnd(&stream_);
      }
    };
  }


  void GzipCompressor::Uncompress(std::string& uncompressed,
                                  const void* compressed,
                                  size_t compressedSize)
  {
    const uint8_t* source = reinterpret_cast<const uint8_t*>(compressed);

    bool hasMaximumSize = false;
    size_t maximumSize = 0;

    if (HasPrefixWithUncompressedSize())
    {
      hasMaximumSize = true;
      maximumSize = ReadUncompressedSizePrefix(compressed, compressedSize);
      source += sizeof(uint64_t);
      compressedSize -= sizeof(uint64_t);
    }
    else
    {
      ReaderWriterLock::ReadLock lock(maximumUncompressedFileSizeMutex_);

      if (hasMaximumUncompressedFileSize_)
      {
        hasMaximumSize = true;
        maximumSize = maximumUncompressedFileSize_;
      }
    }

    z_stream stream;
    memset(&stream, 0, sizeof(stream));

    stream.next_in = const_cast<Bytef*>(source);
    stream.avail_in = static_cast<uInt>(compressedSize);

    // Ensure no overflow (if the buffer is too large for the current zlib archicture)
    if (static_cast<size_t>(stream.avail_in) != compressedSize)
    {
      throw OrthancException(ErrorCode_NotEnoughMemory);
    }

    ChunkedBuffer buffer;

    {
      GzipRaii raii(stream);

      std::string chunk;
      chunk.resize(10 * 1024 * 1024); // Read by chunks of 10MB

      int ret = Z_OK;

      while (ret != Z_STREAM_END)
      {
        stream.next_out  = reinterpret_cast<Bytef*>(chunk.data());
        stream.avail_out = static_cast<uInt>(chunk.size());

        ret = inflate(&stream, Z_NO_FLUSH);

        switch (ret)
        {
        case Z_STREAM_END:
        case Z_OK:
          break; // Normal

        case Z_NEED_DICT:
        case Z_DATA_ERROR:
        case Z_STREAM_ERROR:
          throw OrthancException(ErrorCode_BadFileFormat);

        case Z_MEM_ERROR:
          throw OrthancException(ErrorCode_NotEnoughMemory);

        case Z_BUF_ERROR:
          // Not fatal: means no progress was possible this round.
          // If avail_in is also 0 here, the input is truncated.
          if (stream.avail_in == 0)
          {
            throw OrthancException(ErrorCode_BadFileFormat, "Truncated gzip input");
          }
          break;

        default:
          throw OrthancException(ErrorCode_InternalError); // Unknown error
        }

        const size_t produced = chunk.size() - stream.avail_out;

        if (hasMaximumSize &&
            buffer.GetNumBytes() + produced > maximumSize)
        {
          char s[32];
          sprintf(s, "%0.1f", static_cast<float>(maximumSize) / (1024.0f * 1024.0f));
          throw OrthancException(ErrorCode_BadFileFormat, "Uncompressed size exceeds limit (" + std::string(s) + "MB)");
        }
        else
        {
          // OK, add the chunk
          buffer.AddChunk(&chunk[0], produced);
        }
      }
    }

    buffer.Flatten(uncompressed);
  }


  void GzipCompressor::SetMaximumUncompressedFileSize(uint64_t size)
  {
    if (static_cast<uint64_t>(static_cast<size_t>(size)) != size)
    {
      throw OrthancException(ErrorCode_NotEnoughMemory);
    }
    else
    {
      ReaderWriterLock::WriteLock lock(maximumUncompressedFileSizeMutex_);
      hasMaximumUncompressedFileSize_ = true;
      maximumUncompressedFileSize_ = size;
    }
  }
}
