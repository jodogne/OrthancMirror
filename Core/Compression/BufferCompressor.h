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

#include <string>
#include <cstddef>
#include <stdint.h>
#include <vector>

namespace Palanthir
{
  class BufferCompressor
  {
  public:
    virtual ~BufferCompressor()
    {
    }

    virtual void Compress(std::string& compressed,
                          const void* uncompressed,
                          size_t uncompressedSize) = 0;

    virtual void Uncompress(std::string& uncompressed,
                            const void* compressed,
                            size_t compressedSize) = 0;

    virtual void Compress(std::string& compressed,
                          const std::vector<uint8_t>& uncompressed);

    virtual void Uncompress(std::string& uncompressed,
                            const std::vector<uint8_t>& compressed);

    virtual void Compress(std::string& compressed,
                          const std::string& uncompressed);

    virtual void Uncompress(std::string& uncompressed,
                            const std::string& compressed);
  };
}
