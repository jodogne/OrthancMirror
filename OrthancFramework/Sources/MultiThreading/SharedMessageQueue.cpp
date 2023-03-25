/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2021-2023 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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
#include "SharedMessageQueue.h"


#include "../Compatibility.h"


/**
 * FIFO (queue):
 * 
 *            back                         front
 *            +--+--+--+--+--+--+--+--+--+--+--+
 * Enqueue -> |  |  |  |  |  |  |  |  |  |  |  |
 *            |  |  |  |  |  |  |  |  |  |  |  | -> Dequeue
 *            +--+--+--+--+--+--+--+--+--+--+--+
 *                                            ^
 *                                            |
 *                                      Make room here
 *
 *
 * LIFO (stack):
 * 
 *            back                         front
 *            +--+--+--+--+--+--+--+--+--+--+--+
 *            |  |  |  |  |  |  |  |  |  |  |  | <- Enqueue
 *            |  |  |  |  |  |  |  |  |  |  |  | -> Dequeue
 *            +--+--+--+--+--+--+--+--+--+--+--+
 *              ^
 *              |
 *        Make room here
 **/


namespace Orthanc
{
  SharedMessageQueue::SharedMessageQueue(unsigned int maxSize) :
    isFifo_(true),
    maxSize_(maxSize)
  {
  }


  SharedMessageQueue::~SharedMessageQueue()
  {
    for (Queue::iterator it = queue_.begin(); it != queue_.end(); ++it)
    {
      delete *it;
    }
  }


  void SharedMessageQueue::Enqueue(IDynamicObject* message)
  {
    boost::mutex::scoped_lock lock(mutex_);

    if (maxSize_ != 0 && queue_.size() > maxSize_)
    {
      if (isFifo_)
      {
        // Too many elements in the queue: Make room
        delete queue_.front();
        queue_.pop_front();
      }
      else
      {
        // Too many elements in the stack: Make room
        delete queue_.back();
        queue_.pop_back();
      }
    }

    if (isFifo_)
    {
      // Queue policy (FIFO)
      queue_.push_back(message);
    }
    else
    {
      // Stack policy (LIFO)
      queue_.push_front(message);
    }

    elementAvailable_.notify_one();
  }


  IDynamicObject* SharedMessageQueue::Dequeue(int32_t millisecondsTimeout)
  {
    boost::mutex::scoped_lock lock(mutex_);

    // Wait for a message to arrive in the FIFO queue
    while (queue_.empty())
    {
      if (millisecondsTimeout == 0)
      {
        elementAvailable_.wait(lock);
      }
      else
      {
        bool success = elementAvailable_.timed_wait
          (lock, boost::posix_time::milliseconds(millisecondsTimeout));
        if (!success)
        {
          return NULL;
        }
      }
    }

    std::unique_ptr<IDynamicObject> message(queue_.front());
    queue_.pop_front();

    if (queue_.empty())
    {
      emptied_.notify_all();
    }

    return message.release();
  }



  bool SharedMessageQueue::WaitEmpty(int32_t millisecondsTimeout)
  {
    boost::mutex::scoped_lock lock(mutex_);
    
    // Wait for the queue to become empty
    while (!queue_.empty())
    {
      if (millisecondsTimeout == 0)
      {
        emptied_.wait(lock);
      }
      else
      {
        if (!emptied_.timed_wait
            (lock, boost::posix_time::milliseconds(millisecondsTimeout)))
        {
          return false;
        }
      }
    }

    return true;
  }

  bool SharedMessageQueue::IsFifoPolicy() const
  {
    return isFifo_;
  }

  bool SharedMessageQueue::IsLifoPolicy() const
  {
    return !isFifo_;
  }


  void SharedMessageQueue::SetFifoPolicy()
  {
    boost::mutex::scoped_lock lock(mutex_);
    isFifo_ = true;
  }

  void SharedMessageQueue::SetLifoPolicy()
  {
    boost::mutex::scoped_lock lock(mutex_);
    isFifo_ = false;
  }

  void SharedMessageQueue::Clear()
  {
    boost::mutex::scoped_lock lock(mutex_);

    if (queue_.empty())
    {
      return;
    }
    else
    {
      while (!queue_.empty())
      {
        std::unique_ptr<IDynamicObject> message(queue_.front());
        queue_.pop_front();
      }

      emptied_.notify_all();
    }
  }

  size_t SharedMessageQueue::GetSize()
  {
    boost::mutex::scoped_lock lock(mutex_);
    return queue_.size();
  }
}
