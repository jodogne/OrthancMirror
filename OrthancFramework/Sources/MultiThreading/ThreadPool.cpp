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
#include "ThreadPool.h"

#include "../Logging.h"
#include "../OrthancException.h"
#include "FutureState.h"

static const unsigned int DEFAULT_DEQUEUE_TIMEOUT_MS = 100;


namespace Orthanc
{
  class ThreadPool::Task : public IDynamicObject
  {
  private:
    std::unique_ptr<ICallable>               callable_;
    boost::weak_ptr<Internals::FutureState>  state_;

  public:
    Task(ICallable* callable,
         boost::shared_ptr<Internals::FutureState>& state) :
      callable_(callable),
      state_(state)
    {
      assert(callable != NULL);
    }

    void Execute()
    {
      boost::shared_ptr<Internals::FutureState> locked = state_.lock();

      if (locked)
      {
        try
        {
          locked->AcquireResult(callable_->Call());
        }
        catch (const OrthancException& e)
        {
          locked->SetError(e);
        }
        catch (...)
        {
          locked->SetError(OrthancException(ErrorCode_InternalError, "Unknown exception in ICallable::Call()"));
        }
      }
      else
      {
        // Nothing to do: The future was canceled before we even started
      }
    }

    void Cancel()
    {
      boost::shared_ptr<Internals::FutureState> locked = state_.lock();

      if (locked)
      {
        locked->SetError(OrthancException(ErrorCode_CanceledJob));
      }
      else
      {
        // Nothing to do: The future was canceled before we even started
      }
    }
  };


  void ThreadPool::StopInternal(bool throws)
  {
    {
      boost::mutex::scoped_lock lock(mutex_);

      switch (state_)
      {
        case State_Initialization:
          if (throws)
          {
            throw OrthancException(ErrorCode_BadSequenceOfCalls, "Start() has not been called");
          }
          else
          {
            return;  // This is for the destructor
          }

        case State_Finalization:
          if (throws)
          {
            throw OrthancException(ErrorCode_BadSequenceOfCalls, "Concurrent access to Stop()");
          }
          else
          {
            return;  // This is for the destructor, should never happen
          }

        case State_Running:
          state_ = State_Finalization;
          break;

        default:
          if (throws)
          {
            throw OrthancException(ErrorCode_InternalError);
          }
          else
          {
            return;  // This is for the destructor
          }
      }
    }

    assert(workers_.get() != NULL);
    workers_->join_all();
    workers_.reset(NULL);

    // Cancel all the remaining tasks in the queue
    for (;;)
    {
      std::unique_ptr<IDynamicObject> task(queue_.Dequeue(1));

      if (task.get() == NULL)
      {
        break;
      }
      else
      {
        try
        {
          dynamic_cast<Task&>(*task).Cancel();
        }
        catch (OrthancException& e)
        {
          LOG(ERROR) << "Error while canceling task during shutdown: " << e.What();
        }
      }
    }

    {
      boost::mutex::scoped_lock lock(mutex_);
      state_ = State_Initialization;
    }
  }


  void ThreadPool::WorkerLoop()
  {
    while (true)
    {
      unsigned int timeout;

      {
        boost::mutex::scoped_lock lock(mutex_);

        if (state_ == State_Running)
        {
          timeout = dequeueTimeoutMilliseconds_;
        }
        else
        {
          assert(state_ == State_Finalization);
          return;
        }
      }

      std::unique_ptr<IDynamicObject> task(queue_.Dequeue(timeout));

      if (task.get() != NULL)
      {
        dynamic_cast<Task&>(*task).Execute();
      }
    }
  }


  ThreadPool::ThreadPool() :
    countThreads_(1),
    state_(State_Initialization),
    dequeueTimeoutMilliseconds_(DEFAULT_DEQUEUE_TIMEOUT_MS)
  {
  }


  void ThreadPool::SetCountThreads(unsigned int count)
  {
    if (count < 1)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    {
      boost::mutex::scoped_lock lock(mutex_);

      if (state_ == State_Initialization)
      {
        countThreads_ = count;
      }
      else
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls, "Start() has already been called");
      }
    }
  }


  unsigned int ThreadPool::GetCountThreads()
  {
    boost::mutex::scoped_lock lock(mutex_);
    return countThreads_;
  }


  void ThreadPool::SetDequeueTimeout(unsigned int milliseconds)
  {
    if (milliseconds < 1)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    {
      boost::mutex::scoped_lock lock(mutex_);

      if (state_ == State_Initialization)
      {
        dequeueTimeoutMilliseconds_ = milliseconds;
      }
      else
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls, "Start() has already been called");
      }
    }
  }


  unsigned int ThreadPool::GetDequeueTimeout()
  {
    boost::mutex::scoped_lock lock(mutex_);
    return dequeueTimeoutMilliseconds_;
  }


  void ThreadPool::Start()
  {
    boost::mutex::scoped_lock lock(mutex_);

    if (state_ == State_Initialization)
    {
      assert(countThreads_ >= 1);

      state_ = State_Running;

      assert(workers_.get() == NULL);
      workers_.reset(new boost::thread_group);

      for (unsigned int i = 0; i < countThreads_; i++)
      {
        workers_->create_thread(boost::bind(&ThreadPool::WorkerLoop, this));
      }
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls, "Start() has not been called");
    }
  }


  Future* ThreadPool::Submit(ICallable* callable)
  {
    {
      boost::mutex::scoped_lock lock(mutex_);

      if (state_ != State_Running)
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls, "The thread pool is not running");
      }
    }

    std::unique_ptr<ICallable> protection(callable);

    if (callable == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }

    boost::shared_ptr<Internals::FutureState> state(boost::make_shared<Internals::FutureState>());

    queue_.Enqueue(new Task(protection.release(), state));

    return new Future(state);
  }
}
