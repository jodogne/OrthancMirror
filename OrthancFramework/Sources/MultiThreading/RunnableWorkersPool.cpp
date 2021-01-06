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
#include "RunnableWorkersPool.h"

#include "SharedMessageQueue.h"
#include "../Compatibility.h"
#include "../OrthancException.h"
#include "../Logging.h"

namespace Orthanc
{
  struct RunnableWorkersPool::PImpl
  {
    class Worker
    {
    private:
      const bool&           continue_;
      SharedMessageQueue&   queue_;
      boost::thread         thread_;
 
      static void WorkerThread(Worker* that)
      {
        while (that->continue_)
        {
          try
          {
            std::unique_ptr<IDynamicObject>  obj(that->queue_.Dequeue(100));
            if (obj.get() != NULL)
            {
              IRunnableBySteps& runnable = *dynamic_cast<IRunnableBySteps*>(obj.get());
              
              bool wishToContinue = runnable.Step();
              
              if (wishToContinue)
              {
                // The runnable wishes to continue, reinsert it at the beginning of the queue
                that->queue_.Enqueue(obj.release());
              }
            }
          }
          catch (OrthancException& e)
          {
            LOG(ERROR) << "Exception while handling some runnable object: " << e.What();
          }
          catch (std::bad_alloc&)
          {
            LOG(ERROR) << "Not enough memory to handle some runnable object";
          }
          catch (std::exception& e)
          {
            LOG(ERROR) << "std::exception while handling some runnable object: " << e.what();
          }
          catch (...)
          {
            LOG(ERROR) << "Native exception while handling some runnable object";
          }
        }
      }

    public:
      Worker(const bool& globalContinue,
             SharedMessageQueue& queue) : 
        continue_(globalContinue),
        queue_(queue)
      {
        thread_ = boost::thread(WorkerThread, this);
      }

      void Join()
      {
        if (thread_.joinable())
        {
          thread_.join();
        }
      }
    };


    bool                  continue_;
    std::vector<Worker*>  workers_;
    SharedMessageQueue    queue_;
  };



  RunnableWorkersPool::RunnableWorkersPool(size_t countWorkers) : pimpl_(new PImpl)
  {
    pimpl_->continue_ = true;

    if (countWorkers == 0)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    pimpl_->workers_.resize(countWorkers);

    for (size_t i = 0; i < countWorkers; i++)
    {
      pimpl_->workers_[i] = new PImpl::Worker(pimpl_->continue_, pimpl_->queue_);
    }
  }


  void RunnableWorkersPool::Stop()
  {
    if (pimpl_->continue_)
    {
      pimpl_->continue_ = false;

      for (size_t i = 0; i < pimpl_->workers_.size(); i++)
      {
        PImpl::Worker* worker = pimpl_->workers_[i];

        if (worker != NULL)
        {
          worker->Join();
          delete worker;
        }
      }
    }
  }


  RunnableWorkersPool::~RunnableWorkersPool()
  {
    Stop();
  }


  void RunnableWorkersPool::Add(IRunnableBySteps* runnable)
  {
    if (!pimpl_->continue_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    pimpl_->queue_.Enqueue(runnable);
  }
}
