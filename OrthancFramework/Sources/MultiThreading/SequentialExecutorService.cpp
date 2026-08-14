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
#include "SequentialExecutorService.h"

#include "../OrthancException.h"


namespace Orthanc
{
  class SequentialExecutorService::Future : public IFuture
  {
  private:
    std::unique_ptr<IDynamicObject>  value_;

  public:
    Future(IDynamicObject* value) :
      value_(value)
    {
      if (value_ == NULL)
      {
        throw OrthancException(ErrorCode_NullPointer);
      }
    }

    virtual IDynamicObject* ReleaseResult() ORTHANC_OVERRIDE
    {
      return value_.release();
    }
  };


  SequentialExecutorService::SequentialExecutorService() :
    working_(true)
  {
  }


  bool SequentialExecutorService::IsWorking()
  {
    Mutex::ScopedLock lock(mutex_);
    return working_;
  }


  IFuture* SequentialExecutorService::Submit(ICallable* callable /* takes ownership */)
  {
    Mutex::ScopedLock lock(mutex_);

    if (working_)
    {
      return new Future(callable->Call());
    }
    else
    {
      throw OrthancException(ErrorCode_InternalError);
    }
  }


  void SequentialExecutorService::Submit(IRunnable* runnable /* takes ownership */)
  {
    Mutex::ScopedLock lock(mutex_);

    if (runnable == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
    else
    {
      std::unique_ptr<IRunnable> protection(runnable);
      protection->Run();
    }
  }


  void SequentialExecutorService::Stop()
  {
    Mutex::ScopedLock lock(mutex_);

    if (working_)
    {
      working_ = false;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }
}
