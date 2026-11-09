/**
 * Stone of Orthanc
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
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


#include "PeriodicRunnableThread.h"

#include "../OrthancException.h"


namespace Orthanc
{
  static boost::posix_time::ptime GetNow()
  {
    return boost::posix_time::microsec_clock::universal_time();
  }


  void PeriodicRunnableThread::Worker(PeriodicRunnableThread* that)
  {
    boost::posix_time::ptime nextExecutionTime = GetNow();

    for (;;)
    {
      {
        boost::mutex::scoped_lock lock(that->mutex_);
        if (that->state_ != State_Running)
        {
          return;
        }
      }

      const boost::posix_time::ptime now = GetNow();

      if (now >= nextExecutionTime)
      {
        that->runnable_->Run();
        nextExecutionTime = now + boost::posix_time::milliseconds(that->delay_);
      }
      else
      {
        if (that->timeResolution_ != 0)
        {
          boost::this_thread::sleep(boost::posix_time::milliseconds(that->timeResolution_));
        }
      }
    }
  }


  void PeriodicRunnableThread::StopInternal(bool throws)
  {
    {
      boost::mutex::scoped_lock lock(mutex_);

      if (state_ != State_Running)
      {
        if (throws)
        {
          throw OrthancException(ErrorCode_BadSequenceOfCalls);
        }
      }
      else
      {
        state_ = State_Done;
      }
    }

    if (thread_.joinable())
    {
      thread_.join();
    }
  }


  PeriodicRunnableThread::PeriodicRunnableThread(IRunnable* runnable,
                                                 unsigned int delay) :
    runnable_(runnable),
    delay_(delay),
    timeResolution_(50),  // By default, use a time granularity of 50ms
    state_(State_Initialization)
  {
    if (runnable == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }

    // Reduce the default granularity if the runnable must be executed faster
    if (delay_ < timeResolution_)
    {
      timeResolution_ = delay_;
    }
  }


  PeriodicRunnableThread::~PeriodicRunnableThread()
  {
    try
    {
      StopInternal(false /* no exception in destructors */);
    }
    catch (...)
    {
      // This should never happen
    }
  }


  void PeriodicRunnableThread::SetTimeResolution(unsigned int resolution)
  {
    boost::mutex::scoped_lock lock(mutex_);

    if (state_ != State_Initialization)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      timeResolution_ = resolution;
    }
  }


  void PeriodicRunnableThread::Start()
  {
    boost::mutex::scoped_lock lock(mutex_);

    if (state_ != State_Initialization)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      state_ = State_Running;
      thread_ = boost::thread(Worker, this);
    }
  }
}
