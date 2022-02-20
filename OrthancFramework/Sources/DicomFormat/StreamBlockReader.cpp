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


#include "../PrecompiledHeaders.h"
#include "StreamBlockReader.h"

#include "../OrthancException.h"


namespace Orthanc
{
  StreamBlockReader::StreamBlockReader(std::istream& stream) :
    stream_(stream),
    blockPos_(0),
    processedBytes_(0)
  {
  }


  void StreamBlockReader::Schedule(size_t blockSize)
  {
    if (!block_.empty())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      block_.resize(blockSize);
      blockPos_ = 0;
    }
  }


  bool StreamBlockReader::Read(std::string& block)
  {
    if (block_.empty())
    {
      if (blockPos_ != 0)
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }
        
      block.clear();
      return true;
    }
    else
    {
      while (blockPos_ < block_.size())
      {
        /**
         * WARNING: Do NOT use "stream_.readsome()", as it does not
         * work properly on non-buffered stream (which is the case in
         * "DicomStreamReader::LookupPixelDataOffset()" for buffers)
         **/
        
        size_t remainingBytes = block_.size() - blockPos_;
        stream_.read(&block_[blockPos_], remainingBytes);
        
        std::streamsize r = stream_.gcount();
        if (r == 0)
        {
          return false;
        }
        else
        {
          blockPos_ += r;
        }
      }

      processedBytes_ += block_.size();

      block.swap(block_);
      block_.clear();
      return true;
    }
  }

  uint64_t StreamBlockReader::GetProcessedBytes() const
  {
    return processedBytes_;
  }
}
