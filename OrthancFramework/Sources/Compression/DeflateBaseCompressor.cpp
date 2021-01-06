/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
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
#include "DeflateBaseCompressor.h"

#include "../OrthancException.h"
#include "../Logging.h"

#include <string.h>

namespace Orthanc
{
  DeflateBaseCompressor::DeflateBaseCompressor() :
    compressionLevel_(6),
    prefixWithUncompressedSize_(false)
  {
  }


  void DeflateBaseCompressor::SetCompressionLevel(uint8_t level)
  {
    if (level >= 10)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange,
                             "Zlib compression level must be between 0 (no compression) and 9 (highest compression)");
    }

    compressionLevel_ = level;
  }


  uint64_t DeflateBaseCompressor::ReadUncompressedSizePrefix(const void* compressed,
                                                             size_t compressedSize)
  {
    if (compressedSize == 0)
    {
      return 0;
    }

    if (compressedSize < sizeof(uint64_t))
    {
      throw OrthancException(ErrorCode_CorruptedFile, "The compressed buffer is ill-formed");
    }

    uint64_t size;
    memcpy(&size, compressed, sizeof(uint64_t));

    return size;
  }


  void DeflateBaseCompressor::SetPrefixWithUncompressedSize(bool prefix)
  {
    prefixWithUncompressedSize_ = prefix;
  }

  bool DeflateBaseCompressor::HasPrefixWithUncompressedSize() const
  {
    return prefixWithUncompressedSize_;
  }

  uint8_t DeflateBaseCompressor::GetCompressionLevel() const
  {
    return compressionLevel_;
  }
}
