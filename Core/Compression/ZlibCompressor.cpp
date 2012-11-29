/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012 Medical Physics Department, CHU of Liege,
 * Belgium
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


#include "ZlibCompressor.h"

#include <stdio.h>
#include <string.h>
#include <zlib.h>
#include "../OrthancException.h"

namespace Orthanc
{
  void ZlibCompressor::SetCompressionLevel(uint8_t level)
  {
    if (level >= 10)
    {
      throw OrthancException("Zlib compression level must be between 0 (no compression) and 9 (highest compression");
    }

    compressionLevel_ = level;
  }


  void ZlibCompressor::Compress(std::string& compressed,
                                const void* uncompressed,
                                size_t uncompressedSize)
  {
    if (uncompressedSize == 0)
    {
      compressed.clear();
      return;
    }

    uLongf compressedSize = compressBound(uncompressedSize);
    compressed.resize(compressedSize + sizeof(size_t));

    int error = compress2
      (reinterpret_cast<uint8_t*>(&compressed[0]) + sizeof(size_t),
       &compressedSize,
       const_cast<Bytef *>(static_cast<const Bytef *>(uncompressed)), 
       uncompressedSize,
       compressionLevel_);

    memcpy(&compressed[0], &uncompressedSize, sizeof(size_t));
  
    if (error == Z_OK)
    {
      compressed.resize(compressedSize + sizeof(size_t));
      return;
    }
    else
    {
      compressed.clear();

      switch (error)
      {
      case Z_MEM_ERROR:
        throw OrthancException(ErrorCode_NotEnoughMemory);

      default:
        throw OrthancException(ErrorCode_InternalError);
      }  
    }
  }


  void ZlibCompressor::Uncompress(std::string& uncompressed,
                                  const void* compressed,
                                  size_t compressedSize)
  {
    if (compressedSize == 0)
    {
      uncompressed.clear();
      return;
    }

    if (compressedSize < sizeof(size_t))
    {
      throw OrthancException("Zlib: The compressed buffer is ill-formed");
    }

    size_t uncompressedLength;
    memcpy(&uncompressedLength, compressed, sizeof(size_t));
    
    try
    {
      uncompressed.resize(uncompressedLength);
    }
    catch (...)
    {
      throw OrthancException("Zlib: Corrupted compressed buffer");
    }

    uLongf tmp = uncompressedLength;
    int error = uncompress
      (reinterpret_cast<uint8_t*>(&uncompressed[0]), 
       &tmp,
       reinterpret_cast<const uint8_t*>(compressed) + sizeof(size_t),
       compressedSize - sizeof(size_t));

    if (error != Z_OK)
    {
      uncompressed.clear();

      switch (error)
      {
      case Z_DATA_ERROR:
        throw OrthancException("Zlib: Corrupted or incomplete compressed buffer");

      case Z_MEM_ERROR:
        throw OrthancException(ErrorCode_NotEnoughMemory);

      default:
        throw OrthancException(ErrorCode_InternalError);
      }  
    }
  }
}
