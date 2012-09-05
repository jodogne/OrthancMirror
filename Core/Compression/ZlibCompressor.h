/**
 * Palanthir - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012 Medical Physics Department, CHU of Liege,
 * Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#pragma once

#include "BufferCompressor.h"

namespace Palanthir
{
  class ZlibCompressor : public BufferCompressor
  {
  private:
    uint8_t compressionLevel_;

  public:
    using BufferCompressor::Compress;
    using BufferCompressor::Uncompress;

    ZlibCompressor()
    {
      compressionLevel_ = 6;
    }

    void SetCompressionLevel(uint8_t level);

    uint8_t GetCompressionLevel() const
    {
      return compressionLevel_;
    }

    virtual void Compress(std::string& compressed,
                          const void* uncompressed,
                          size_t uncompressedSize);

    virtual void Uncompress(std::string& uncompressed,
                            const void* compressed,
                            size_t compressedSize);
  };
}
