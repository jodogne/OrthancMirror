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
#include "MallocMemoryBuffer.h"

#include "OrthancException.h"

#include <string.h>


namespace Orthanc
{
  MallocMemoryBuffer::MallocMemoryBuffer() :
    buffer_(NULL),
    size_(0),
    free_(NULL)
  {
  }


  void MallocMemoryBuffer::Clear()
  {
    if (size_ != 0)
    {
      if (free_ == NULL)
      {
        throw OrthancException(ErrorCode_InternalError);
      }
        
      free_(buffer_);
      buffer_ = NULL;
      size_ = 0;
      free_ = NULL;
    }
  }

    
  void MallocMemoryBuffer::Assign(void* buffer,
                                  uint64_t size,
                                  FreeFunction freeFunction)
  {
    Clear();

    if (size != 0 &&
        buffer == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }

    if (static_cast<uint64_t>(static_cast<size_t>(size)) != size)
    {
      freeFunction(buffer);
      throw OrthancException(ErrorCode_InternalError, "Buffer larger than 4GB, which is too large for Orthanc running in 32bits");
    }

    buffer_ = buffer;
    size_ = size;
    free_ = freeFunction;

    if (size_ != 0 &&
        free_ == NULL)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange, "No valid free() function provided");
    }
  }

    
  void MallocMemoryBuffer::MoveToString(std::string& target)
  {
    target.resize(size_);

    if (size_ != 0)
    {
      memcpy(&target[0], buffer_, size_);
    }

    Clear();
  }
}
