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
#include "Semaphore.h"

#include "../OrthancException.h"


namespace Orthanc
{
  Semaphore::Semaphore(unsigned int availableResources) :
    availableResources_(availableResources)
  {
  }

  unsigned int Semaphore::GetAvailableResourcesCount() const
  {
    return availableResources_;
  }

  void Semaphore::Release(unsigned int resourceCount)
  {
    boost::mutex::scoped_lock lock(mutex_);

    availableResources_ += resourceCount;
    condition_.notify_one();
  }

  void Semaphore::Acquire(unsigned int resourceCount)
  {
    boost::mutex::scoped_lock lock(mutex_);

    while (availableResources_ < resourceCount)
    {
      condition_.wait(lock);
    }

    availableResources_ -= resourceCount;
  }

  bool Semaphore::TryAcquire(unsigned int resourceCount)
  {
    boost::mutex::scoped_lock lock(mutex_);

    if (availableResources_ < resourceCount)
    {
      return false;
    }

    availableResources_ -= resourceCount;
    return true;
  }
}
