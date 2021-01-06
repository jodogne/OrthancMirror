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
#include "Mutex.h"

#include "../OrthancException.h"

#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__) || defined(__FreeBSD_kernel__) || defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__)
#include <pthread.h>
#else
#error Support your platform here
#endif

namespace Orthanc
{
#if defined (_WIN32)

  struct Mutex::PImpl
  {
    CRITICAL_SECTION criticalSection_;
  };

  Mutex::Mutex()
  {
    pimpl_ = new PImpl;
    ::InitializeCriticalSection(&pimpl_->criticalSection_);
  }

  Mutex::~Mutex()
  {
    ::DeleteCriticalSection(&pimpl_->criticalSection_);
    delete pimpl_;
  }

  void Mutex::Lock()
  {
    ::EnterCriticalSection(&pimpl_->criticalSection_);
  }

  void Mutex::Unlock()
  {
    ::LeaveCriticalSection(&pimpl_->criticalSection_);
  }


#elif defined(__linux__) || defined(__FreeBSD_kernel__) || defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__)

  struct Mutex::PImpl
  {
    pthread_mutex_t mutex_;
  };

  Mutex::Mutex()
  {
    pimpl_ = new PImpl;

    if (pthread_mutex_init(&pimpl_->mutex_, NULL) != 0)
    {
      delete pimpl_;
      throw OrthancException(ErrorCode_InternalError);
    }
  }

  Mutex::~Mutex()
  {
    pthread_mutex_destroy(&pimpl_->mutex_);
    delete pimpl_;
  }

  void Mutex::Lock()
  {
    if (pthread_mutex_lock(&pimpl_->mutex_) != 0)
    {
      throw OrthancException(ErrorCode_InternalError);    
    }
  }

  void Mutex::Unlock()
  {
    if (pthread_mutex_unlock(&pimpl_->mutex_) != 0)
    {
      throw OrthancException(ErrorCode_InternalError);    
    }
  }

#else
#error Support your plateform here
#endif
}
