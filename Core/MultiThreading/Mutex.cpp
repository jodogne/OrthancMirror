/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2018 Osimis S.A., Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * In addition, as a special exception, the copyright holders of this
 * program give permission to link the code of its release with the
 * OpenSSL project's "OpenSSL" library (or with modified versions of it
 * that use the same license as the "OpenSSL" library), and distribute
 * the linked executables. You must obey the GNU General Public License
 * in all respects for all of the code used other than "OpenSSL". If you
 * modify file(s) with this exception, you may extend this exception to
 * your version of the file(s), but you are not obligated to do so. If
 * you do not wish to do so, delete this exception statement from your
 * version. If you delete this exception statement from all source files
 * in the program, then also delete it here.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
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
