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


#pragma once

#include "OrthancFramework.h"

#include <boost/noncopyable.hpp>
#include <list>
#include <string>

namespace Orthanc
{
  class ORTHANC_PUBLIC ChunkedBuffer : public boost::noncopyable
  {
  private:
    typedef std::list<std::string*>  Chunks;
    
    size_t       numBytes_;
    Chunks       chunks_;
    std::string  pendingBuffer_;   // Buffer to speed up if adding many small chunks
    size_t       pendingPos_;
  
    void Clear();

    void AddChunkInternal(const void* chunkData,
                          size_t chunkSize);

    void FlushPendingBuffer();

  public:
    ChunkedBuffer();

    ~ChunkedBuffer();

    size_t GetNumBytes() const;

    void SetPendingBufferSize(size_t size);

    size_t GetPendingBufferSize() const;

    void AddChunk(const void* chunkData,
                  size_t chunkSize);

    void AddChunk(const std::string& chunk);

    void AddChunk(const std::string::const_iterator& begin,
                  const std::string::const_iterator& end);

    void Flatten(std::string& result);
  };
}
