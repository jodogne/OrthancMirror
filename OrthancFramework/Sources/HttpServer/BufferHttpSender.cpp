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
#include "BufferHttpSender.h"

#include "../OrthancException.h"

#include <cassert>

namespace Orthanc
{
  BufferHttpSender::BufferHttpSender() :
    position_(0), 
    chunkSize_(0),
    currentChunkSize_(0)
  {
  }

  std::string &BufferHttpSender::GetBuffer()
  {
    return buffer_;
  }

  const std::string &BufferHttpSender::GetBuffer() const
  {
    return buffer_;
  }

  void BufferHttpSender::SetChunkSize(size_t chunkSize)
  {
    chunkSize_ = chunkSize;
  }

  uint64_t BufferHttpSender::GetContentLength()
  {
    return buffer_.size();
  }


  bool BufferHttpSender::ReadNextChunk()
  {
    assert(position_ + currentChunkSize_ <= buffer_.size());

    position_ += currentChunkSize_;

    if (position_ == buffer_.size())
    {
      return false;
    }
    else
    {
      currentChunkSize_ = buffer_.size() - position_;

      if (chunkSize_ != 0 &&
          currentChunkSize_ > chunkSize_)
      {
        currentChunkSize_ = chunkSize_;
      }

      return true;
    }
  }


  const char* BufferHttpSender::GetChunkContent()
  {
    return buffer_.c_str() + position_;
  }


  size_t BufferHttpSender::GetChunkSize()
  {
    return currentChunkSize_;
  }
}
