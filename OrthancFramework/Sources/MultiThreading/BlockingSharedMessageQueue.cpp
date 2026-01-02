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
#include "BlockingSharedMessageQueue.h"


#include "../Compatibility.h"


namespace Orthanc
{
  BlockingSharedMessageQueue::BlockingSharedMessageQueue(unsigned int maxSize) :
    maxSize_(maxSize)
  {
  }


  BlockingSharedMessageQueue::~BlockingSharedMessageQueue()
  {
    for (Queue::iterator it = queue_.begin(); it != queue_.end(); ++it)
    {
      delete *it;
    }
  }


  bool BlockingSharedMessageQueue::Enqueue(std::unique_ptr<IDynamicObject>& message, int32_t millisecondsTimeout)
  {
    boost::mutex::scoped_lock lock(mutex_);

    if (maxSize_ != 0 && queue_.size() >= maxSize_)
    {
      if (!roomAvailable_.timed_wait(lock, boost::posix_time::milliseconds(millisecondsTimeout)))
      {
        return false;
      }
    }

    queue_.push_back(message.release());  // take ownership only when pushed into the queue
    elementAvailable_.notify_one();
    
    return true;
  }

  void BlockingSharedMessageQueue::Enqueue(IDynamicObject* message)
  {
    boost::mutex::scoped_lock lock(mutex_);

    if (maxSize_ != 0 && queue_.size() >= maxSize_)
    {
      roomAvailable_.wait(lock);
    }

    queue_.push_back(message);  // take ownership
    elementAvailable_.notify_one();
  }


  IDynamicObject* BlockingSharedMessageQueue::Dequeue(int32_t millisecondsTimeout)
  {
    boost::mutex::scoped_lock lock(mutex_);

    // If it is empty, wait for a message to arrive in the queue
    while (queue_.empty())
    {
      if (millisecondsTimeout == 0)
      {
        elementAvailable_.wait(lock);
      }
      else
      {
        if (!elementAvailable_.timed_wait(lock, boost::posix_time::milliseconds(millisecondsTimeout)))
        {
          return NULL;
        }
      }
    }

    std::unique_ptr<IDynamicObject> message(queue_.front());
    queue_.pop_front();

    roomAvailable_.notify_one();

    if (queue_.empty())
    {
      emptied_.notify_all();
    }

    return message.release();
  }

  bool BlockingSharedMessageQueue::WaitEmpty(int32_t millisecondsTimeout)
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


  void BlockingSharedMessageQueue::Clear()
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
        
        roomAvailable_.notify_one();
      }
    }

    emptied_.notify_all();
  }

  size_t BlockingSharedMessageQueue::GetSize()
  {
    boost::mutex::scoped_lock lock(mutex_);
    return queue_.size();
  }
}
