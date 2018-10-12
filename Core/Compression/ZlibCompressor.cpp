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
#include "ZlibCompressor.h"

#include "../OrthancException.h"
#include "../Logging.h"

#include <stdio.h>
#include <string.h>
#include <zlib.h>

namespace Orthanc
{
  void ZlibCompressor::Compress(std::string& compressed,
                                const void* uncompressed,
                                size_t uncompressedSize)
  {
    if (uncompressedSize == 0)
    {
      compressed.clear();
      return;
    }

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

    int error = compress2(target,
                          &compressedSize,
                          const_cast<Bytef *>(static_cast<const Bytef *>(uncompressed)), 
                          uncompressedSize,
                          GetCompressionLevel());

    if (error != Z_OK)
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

    // The compression was successful
    if (HasPrefixWithUncompressedSize())
    {
      uint64_t s = static_cast<uint64_t>(uncompressedSize);
      memcpy(&compressed[0], &s, sizeof(uint64_t));
      compressed.resize(compressedSize + sizeof(uint64_t));
    }
    else
    {
      compressed.resize(compressedSize);
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

    if (!HasPrefixWithUncompressedSize())
    {
      LOG(ERROR) << "Cannot guess the uncompressed size of a zlib-encoded buffer";
      throw OrthancException(ErrorCode_InternalError);
    }

    uint64_t uncompressedSize = ReadUncompressedSizePrefix(compressed, compressedSize);
    
    try
    {
      uncompressed.resize(static_cast<size_t>(uncompressedSize));
    }
    catch (...)
    {
      throw OrthancException(ErrorCode_NotEnoughMemory);
    }

    uLongf tmp = static_cast<uLongf>(uncompressedSize);
    int error = uncompress
      (reinterpret_cast<uint8_t*>(&uncompressed[0]), 
       &tmp,
       reinterpret_cast<const uint8_t*>(compressed) + sizeof(uint64_t),
       compressedSize - sizeof(uint64_t));

    if (error != Z_OK)
    {
      uncompressed.clear();

      switch (error)
      {
      case Z_DATA_ERROR:
        throw OrthancException(ErrorCode_CorruptedFile);

      case Z_MEM_ERROR:
        throw OrthancException(ErrorCode_NotEnoughMemory);

      default:
        throw OrthancException(ErrorCode_InternalError);
      }  
    }
  }
}
