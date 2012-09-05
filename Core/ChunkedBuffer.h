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

#include <list>
#include <string>

namespace Palanthir
{
  class ChunkedBuffer
  {
  private:
    typedef std::list<std::string*>  Chunks;
    size_t numBytes_;
    Chunks chunks_;
  
    void Clear();

  public:
    ChunkedBuffer() : numBytes_(0)
    {
    }

    ~ChunkedBuffer()
    {
      Clear();
    }

    size_t GetNumBytes() const
    {
      return numBytes_;
    }

    void AddChunk(const char* chunkData,
                  size_t chunkSize);

    void Flatten(std::string& result);
  };
}
