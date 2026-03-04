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


#include "MemoryManagedString.h"

#include "OrthancException.h"
#include "MultiThreading/Semaphore.h"
#include <boost/lexical_cast.hpp>
#include "Logging.h"


namespace Orthanc
{
  static std::unique_ptr<Semaphore> availableMemorySemaphore_;
  static size_t maximumMemorySize_ = 0;

  void LimitedMemoryAllocator::Initialize(size_t maximumMemorySize)
  {
    if (availableMemorySemaphore_.get() != NULL)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    
    maximumMemorySize_ = maximumMemorySize;
    availableMemorySemaphore_.reset(new Semaphore(maximumMemorySize));
  }

  void* LimitedMemoryAllocator::Allocate(size_t elemSize, size_t numElem)
  {
    if (availableMemorySemaphore_.get() == NULL)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    size_t requestedSize = numElem * elemSize;

    if (requestedSize > maximumMemorySize_)
    {
      throw OrthancException(ErrorCode_NotEnoughMemory, "Trying to allocate a buffer ( " + boost::lexical_cast<std::string>(requestedSize) + " bytes) that is larger than the maximum memory size ( " + boost::lexical_cast<std::string>(maximumMemorySize_) + " bytes)");
    }

    // wait until there is enough memory available
    availableMemorySemaphore_->Acquire(requestedSize);

    // LOG(TRACE) << "Reserved " << requestedSize << " bytes in memory.  Remaining size: " << availableMemorySemaphore_->GetAvailableResourcesCount() << " bytes";

    void* p = malloc(numElem * elemSize);
    if (!p) 
    {
      throw std::bad_alloc();  // the allocation might still fail since we don't track the whole memory
    }
    return p;
  }

  void LimitedMemoryAllocator::Deallocate(void* p, size_t elemSize, size_t numElem)
  {
    free(p);
    availableMemorySemaphore_->Release(elemSize * numElem);

    // LOG(TRACE) << "Released " << (elemSize * numElem) << " bytes in memory.  Remaining size: " << availableMemorySemaphore_->GetAvailableResourcesCount() << " bytes";
  }


}
