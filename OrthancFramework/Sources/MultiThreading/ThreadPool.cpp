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

#include "../OrthancException.h"
#include "FutureState.h"

static const unsigned int DEQUEUE_TIMEOUT_MS = 100;


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
  };


  void ThreadPool::WorkerLoop()
  {
    while (true)
    {
      std::unique_ptr<IDynamicObject> task(queue_.Dequeue(DEQUEUE_TIMEOUT_MS));

      if (task.get() == NULL)
      {
        boost::mutex::scoped_lock lock(shutdownMutex_);
        if (shutdown_)
        {
          return;
        }
      }
      else
      {
        dynamic_cast<Task&>(*task).Execute();
      }
    }
  }


  ThreadPool::ThreadPool(unsigned int countThreads) :
    shutdown_(false)
  {
    if (countThreads < 1)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    for (unsigned int i = 0; i < countThreads; i++)
    {
      workers_.create_thread(boost::bind(&ThreadPool::WorkerLoop, this));
    }
  }


  ThreadPool::~ThreadPool()
  {
    {
      boost::mutex::scoped_lock lock(shutdownMutex_);
      shutdown_ = true;
    }

    workers_.join_all();
  }


  Future* ThreadPool::Submit(ICallable* callable)
  {
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
