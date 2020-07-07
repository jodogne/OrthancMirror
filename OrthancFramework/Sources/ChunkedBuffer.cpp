/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
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


#include "PrecompiledHeaders.h"
#include "ChunkedBuffer.h"

#include <cassert>
#include <string.h>


namespace Orthanc
{
  void ChunkedBuffer::Clear()
  {
    numBytes_ = 0;

    for (Chunks::iterator it = chunks_.begin(); 
         it != chunks_.end(); ++it)
    {
      delete *it;
    }
  }


  void ChunkedBuffer::AddChunk(const void* chunkData,
                               size_t chunkSize)
  {
    if (chunkSize == 0)
    {
      return;
    }
    else
    {
      assert(chunkData != NULL);
      chunks_.push_back(new std::string(reinterpret_cast<const char*>(chunkData), chunkSize));
      numBytes_ += chunkSize;
    }
  }


  void ChunkedBuffer::AddChunk(const std::string& chunk)
  {
    if (chunk.size() > 0)
    {
      AddChunk(&chunk[0], chunk.size());
    }
  }


  void ChunkedBuffer::AddChunkDestructive(std::string& chunk)
  {
    size_t chunkSize = chunk.size();
    
    if (chunkSize > 0)
    {
      chunks_.push_back(new std::string);
      chunks_.back()->swap(chunk);
      numBytes_ += chunkSize;
    }
  }


  void ChunkedBuffer::Flatten(std::string& result)
  {
    result.resize(numBytes_);

    size_t pos = 0;
    for (Chunks::iterator it = chunks_.begin(); 
         it != chunks_.end(); ++it)
    {
      assert(*it != NULL);

      size_t s = (*it)->size();
      if (s != 0)
      {
        memcpy(&result[pos], (*it)->c_str(), s);
        pos += s;
      }

      delete *it;
    }

    chunks_.clear();
    numBytes_ = 0;
  }
}
