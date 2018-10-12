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
#include "GzipCompressor.h"

#include <stdio.h>
#include <string.h>
#include <zlib.h>

#include "../OrthancException.h"
#include "../Logging.h"

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
     * is the uncompressed length of that entry modulo 232 in little
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



  void GzipCompressor::Compress(std::string& compressed,
                                const void* uncompressed,
                                size_t uncompressedSize)
  {
    uLongf compressedSize = compressBound(uncompressedSize) + 1024 /* security margin */;
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


  void GzipCompressor::Uncompress(std::string& uncompressed,
                                  const void* compressed,
                                  size_t compressedSize)
  {
    uint64_t uncompressedSize;
    const uint8_t* source = reinterpret_cast<const uint8_t*>(compressed);

    if (HasPrefixWithUncompressedSize())
    {
      uncompressedSize = ReadUncompressedSizePrefix(compressed, compressedSize);
      source += sizeof(uint64_t);
      compressedSize -= sizeof(uint64_t);
    }
    else
    {
      uncompressedSize = GuessUncompressedSize(compressed, compressedSize);
    }

    try
    {
      uncompressed.resize(static_cast<size_t>(uncompressedSize));
    }
    catch (...)
    {
      throw OrthancException(ErrorCode_NotEnoughMemory);
    }

    z_stream stream;
    memset(&stream, 0, sizeof(stream));

    char dummy = '\0';  // zlib does not like NULL output buffers (even if the uncompressed data is empty)
    stream.next_in = const_cast<Bytef*>(source);
    stream.next_out = reinterpret_cast<Bytef*>(uncompressedSize == 0 ? &dummy : &uncompressed[0]);

    stream.avail_in = static_cast<uInt>(compressedSize);
    stream.avail_out = static_cast<uInt>(uncompressedSize);

    // Ensure no overflow (if the buffer is too large for the current archicture)
    if (static_cast<size_t>(stream.avail_in) != compressedSize ||
        static_cast<size_t>(stream.avail_out) != uncompressedSize)
    {
      throw OrthancException(ErrorCode_NotEnoughMemory);
    }

    // Initialize the compression engine
    int error = inflateInit2(&stream, 
                             MAX_WBITS + 16);  // this is a gzip input

    if (error != Z_OK)
    {
      // Cannot initialize zlib
      uncompressed.clear();
      throw OrthancException(ErrorCode_InternalError);
    }

    // Uncompress the input buffer
    error = inflate(&stream, Z_FINISH);

    if (error != Z_STREAM_END)
    {
      inflateEnd(&stream);
      uncompressed.clear();

      switch (error)
      {
        case Z_MEM_ERROR:
          throw OrthancException(ErrorCode_NotEnoughMemory);
          
        case Z_BUF_ERROR:
        case Z_NEED_DICT:
          throw OrthancException(ErrorCode_BadFileFormat);
          
        default:
          throw OrthancException(ErrorCode_InternalError);
      }
    }

    size_t size = stream.total_out;

    if (inflateEnd(&stream) != Z_OK)
    {
      uncompressed.clear();
      throw OrthancException(ErrorCode_InternalError);
    }

    if (size != uncompressedSize)
    {
      uncompressed.clear();

      // The uncompressed size was not that properly guess, presumably
      // because of a file size over 4GB. Should fallback to
      // stream-based decompression.
      LOG(ERROR) << "The uncompressed size of a gzip-encoded buffer was not properly guessed";
      throw OrthancException(ErrorCode_NotImplemented);
    }
  }
}
