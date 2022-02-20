/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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
    pendingPos_ = 0;

    for (Chunks::iterator it = chunks_.begin(); 
         it != chunks_.end(); ++it)
    {
      delete *it;
    }
  }


  void ChunkedBuffer::AddChunkInternal(const void* chunkData,
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


  void ChunkedBuffer::FlushPendingBuffer()
  {
    assert(pendingPos_ <= pendingBuffer_.size());
    
    if (!pendingBuffer_.empty())
    {
      AddChunkInternal(pendingBuffer_.c_str(), pendingPos_);
    }
    else
    {
      assert(pendingPos_ == 0);
    }

    pendingPos_ = 0;
  }


  ChunkedBuffer::ChunkedBuffer() :
    numBytes_(0),
    pendingPos_(0)
  {
    pendingBuffer_.resize(16 * 1024);  // Default size of the pending buffer: 16KB
  }


  ChunkedBuffer::~ChunkedBuffer()
  {
    Clear();
  }


  size_t ChunkedBuffer::GetNumBytes() const
  {
    return numBytes_ + pendingPos_;
  }
  

  void ChunkedBuffer::SetPendingBufferSize(size_t size)
  {
    FlushPendingBuffer();
    pendingBuffer_.resize(size);
  }
  

  size_t ChunkedBuffer::GetPendingBufferSize() const
  {
    return pendingBuffer_.size();
  }

  
  void ChunkedBuffer::AddChunk(const void* chunkData,
                               size_t chunkSize)
  {
    if (chunkSize > 0)
    {
#if 1
      assert(sizeof(char) == 1);
      
      // Optimization if Orthanc >= 1.7.3, to speed up in the presence of many small chunks
      if (pendingPos_ + chunkSize <= pendingBuffer_.size())
      {
        // There remains enough place in the pending buffer
        memcpy(&pendingBuffer_[pendingPos_], chunkData, chunkSize);
        pendingPos_ += chunkSize;
      }
      else
      {
        FlushPendingBuffer();

        if (!pendingBuffer_.empty() &&
            chunkSize < pendingBuffer_.size())
        {
          memcpy(&pendingBuffer_[0], chunkData, chunkSize);
          pendingPos_ = chunkSize;
        }
        else
        {
          AddChunkInternal(chunkData, chunkSize);
        }
      }
#else
      // Non-optimized implementation in Orthanc <= 1.7.2
      AddChunkInternal(chunkData, chunkSize);
#endif
    }
  }


  void ChunkedBuffer::AddChunk(const std::string& chunk)
  {
    if (chunk.size() > 0)
    {
      AddChunk(&chunk[0], chunk.size());
    }
  }


  void ChunkedBuffer::AddChunk(const std::string::const_iterator& begin,
                               const std::string::const_iterator& end)
  {
    const size_t s = end - begin;

    if (s > 0)
    {
      AddChunk(&begin[0], s);
    }
  }
  

  void ChunkedBuffer::Flatten(std::string& result)
  {
    FlushPendingBuffer();
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
