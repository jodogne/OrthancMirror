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


#pragma once

#include "../OrthancFramework.h"

#include <boost/noncopyable.hpp>
#include <boost/thread.hpp>

namespace Orthanc
{
  class ORTHANC_PUBLIC Semaphore : public boost::noncopyable
  {
  private:
    unsigned int availableResources_;
    boost::mutex mutex_;
    boost::condition_variable condition_;

  public:
    explicit Semaphore(unsigned int availableResources);

    unsigned int GetAvailableResourcesCount() const;

    void Release(unsigned int resourceCount = 1);

    void Acquire(unsigned int resourceCount = 1);

    bool TryAcquire(unsigned int resourceCount = 1);

    class Locker : public boost::noncopyable
    {
    private:
      Semaphore&  that_;
      unsigned int resourceCount_;

    public:
      explicit Locker(Semaphore& that, unsigned int resourceCount = 1) :
        that_(that),
        resourceCount_(resourceCount)
      {
        that_.Acquire(resourceCount_);
      }

      ~Locker()
      {
        that_.Release(resourceCount_);
      }
    };

    class TryLocker : public boost::noncopyable
    {
    private:
      Semaphore&    that_;
      unsigned int  resourceCount_;
      bool          isAcquired_;

    public:
      explicit TryLocker(Semaphore& that, unsigned int resourceCount = 1) :
        that_(that),
        resourceCount_(resourceCount)
      {
        isAcquired_ = that_.TryAcquire(resourceCount_);
      }

      ~TryLocker()
      {
        if (isAcquired_)
        {
          that_.Release(resourceCount_);
        }
      }

      bool IsAcquired() const
      {
        return isAcquired_;
      }
    };

  };
}
