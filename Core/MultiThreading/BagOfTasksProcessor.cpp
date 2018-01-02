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
#include "BagOfTasksProcessor.h"

#include "../Logging.h"
#include "../OrthancException.h"

#include <stdio.h>

namespace Orthanc
{
  class BagOfTasksProcessor::Task : public IDynamicObject
  {
  private:
    uint64_t                 bag_;
    std::auto_ptr<ICommand>  command_;

  public:
    Task(uint64_t  bag,
         ICommand* command) :
      bag_(bag),
      command_(command)
    {
    }

    bool Execute()
    {
      try
      {
        return command_->Execute();
      }
      catch (OrthancException& e)
      {
        LOG(ERROR) << "Exception while processing a bag of tasks: " << e.What();
        return false;
      }
      catch (std::runtime_error& e)
      {
        LOG(ERROR) << "Runtime exception while processing a bag of tasks: " << e.what();
        return false;
      }
      catch (...)
      {
        LOG(ERROR) << "Native exception while processing a bag of tasks";
        return false;
      }
    }

    uint64_t GetBag()
    {
      return bag_;
    }
  };


  void BagOfTasksProcessor::SignalProgress(Task& task,
                                           Bag& bag)
  {
    assert(bag.done_ < bag.size_);

    bag.done_ += 1;

    if (bag.done_ == bag.size_)
    {
      exitStatus_[task.GetBag()] = (bag.status_ == BagStatus_Running);
      bagFinished_.notify_all();
    }
  }

  void BagOfTasksProcessor::Worker(BagOfTasksProcessor* that)
  {
    while (that->continue_)
    {
      std::auto_ptr<IDynamicObject> obj(that->queue_.Dequeue(100));
      if (obj.get() != NULL)
      {
        Task& task = *dynamic_cast<Task*>(obj.get());

        {
          boost::mutex::scoped_lock lock(that->mutex_);

          Bags::iterator bag = that->bags_.find(task.GetBag());
          assert(bag != that->bags_.end());
          assert(bag->second.done_ < bag->second.size_);

          if (bag->second.status_ != BagStatus_Running)
          {
            // Do not execute this task, as its parent bag of tasks
            // has failed or is tagged as canceled
            that->SignalProgress(task, bag->second);
            continue;
          }
        }

        bool success = task.Execute();

        {
          boost::mutex::scoped_lock lock(that->mutex_);

          Bags::iterator bag = that->bags_.find(task.GetBag());
          assert(bag != that->bags_.end());

          if (!success)
          {
            bag->second.status_ = BagStatus_Failed;
          }

          that->SignalProgress(task, bag->second);
        }
      }
    }
  }


  void BagOfTasksProcessor::Cancel(int64_t bag)
  {
    boost::mutex::scoped_lock  lock(mutex_);

    Bags::iterator it = bags_.find(bag);
    if (it != bags_.end())
    {
      it->second.status_ = BagStatus_Canceled;
    }
  }


  bool BagOfTasksProcessor::Join(int64_t bag)
  {
    boost::mutex::scoped_lock  lock(mutex_);

    while (continue_)
    {
      ExitStatus::iterator it = exitStatus_.find(bag);
      if (it == exitStatus_.end())  // The bag is still running
      {
        bagFinished_.wait(lock);
      }
      else
      {
        bool status = it->second;
        exitStatus_.erase(it);
        return status;
      }
    }

    return false;   // The processor is stopping
  }


  float BagOfTasksProcessor::GetProgress(int64_t bag)
  {
    boost::mutex::scoped_lock  lock(mutex_);

    Bags::const_iterator it = bags_.find(bag);
    if (it == bags_.end())
    {
      // The bag of tasks has finished
      return 1.0f;
    }
    else
    {
      return (static_cast<float>(it->second.done_) / 
              static_cast<float>(it->second.size_));
    }
  }


  bool BagOfTasksProcessor::Handle::Join()
  {
    if (hasJoined_)
    {
      return status_;
    }
    else
    {
      status_ = that_.Join(bag_);
      hasJoined_ = true;
      return status_;
    }
  }


  BagOfTasksProcessor::BagOfTasksProcessor(size_t countThreads) : 
    countBags_(0),
    continue_(true)
  {
    if (countThreads == 0)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    threads_.resize(countThreads);

    for (size_t i = 0; i < threads_.size(); i++)
    {
      threads_[i] = new boost::thread(Worker, this);
    }
  }


  BagOfTasksProcessor::~BagOfTasksProcessor()
  {
    continue_ = false;

    bagFinished_.notify_all();   // Wakes up all the pending "Join()"

    for (size_t i = 0; i < threads_.size(); i++)
    {
      if (threads_[i])
      {
        if (threads_[i]->joinable())
        {
          threads_[i]->join();
        }

        delete threads_[i];
        threads_[i] = NULL;
      }
    }
  }


  BagOfTasksProcessor::Handle* BagOfTasksProcessor::Submit(BagOfTasks& tasks)
  {
    if (tasks.GetSize() == 0)
    {
      return new Handle(*this, 0, true);
    }

    boost::mutex::scoped_lock lock(mutex_);

    uint64_t id = countBags_;
    countBags_ += 1;

    Bag bag(tasks.GetSize());
    bags_[id] = bag;

    while (!tasks.IsEmpty())
    {
      queue_.Enqueue(new Task(id, tasks.Pop()));
    }

    return new Handle(*this, id, false);
  }
}
