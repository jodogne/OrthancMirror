/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2026 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2026 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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
  void BufferHttpSender::ResetFromInternalBuffer()
  {
    if (internalBuffer_.get() == NULL)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else if (internalBuffer_->empty())
    {
      SetBuffer(NULL, 0);
    }
    else
    {
      SetBuffer(internalBuffer_->c_str(), internalBuffer_->size());
    }
  }


  BufferHttpSender::BufferHttpSender() :
    data_(NULL),
    size_(0),
    position_(0), 
    chunkSize_(0),
    currentChunkSize_(0)
  {
  }

  void BufferHttpSender::SetBuffer(const void* data,
                                   size_t size)
  {
    if (position_ != 0)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      data_ = reinterpret_cast<const char*>(data);
      size_ = size;
    }
  }

  void BufferHttpSender::SetBuffer(const std::string& buffer)
  {
    internalBuffer_.reset(new std::string);
    *internalBuffer_ = buffer;
    ResetFromInternalBuffer();
  }

  void BufferHttpSender::SwapBuffer(std::string& buffer)
  {
    internalBuffer_.reset(new std::string);
    internalBuffer_->swap(buffer);
    ResetFromInternalBuffer();
  }

  void BufferHttpSender::SetChunkSize(size_t chunkSize)
  {
    chunkSize_ = chunkSize;
  }

  uint64_t BufferHttpSender::GetContentLength()
  {
    return size_;
  }


  bool BufferHttpSender::ReadNextChunk()
  {
    assert(position_ + currentChunkSize_ <= size_);

    position_ += currentChunkSize_;

    if (position_ == size_)
    {
      return false;
    }
    else
    {
      currentChunkSize_ = size_ - position_;

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
    return data_ + position_;
  }


  size_t BufferHttpSender::GetChunkSize()
  {
    return currentChunkSize_;
  }
}
