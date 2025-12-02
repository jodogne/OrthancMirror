/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2025 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2025 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#pragma once

#include "../Compatibility.h"
#include "../IDynamicObject.h"

#include <stdint.h>
#include <list>
#include <boost/thread.hpp>

namespace Orthanc
{
  // Compared to SharedMessageQueue that is discarding old messages when it is full,
  // this queue blocks the Enqueue method until there is room for a new message.
  class ORTHANC_PUBLIC BlockingSharedMessageQueue : public boost::noncopyable
  {
  private:
    typedef std::list<IDynamicObject*>  Queue;

    unsigned int maxSize_;
    Queue queue_;
    boost::mutex mutex_;
    boost::condition_variable elementAvailable_;
    boost::condition_variable roomAvailable_;

  public:
    explicit BlockingSharedMessageQueue(unsigned int maxSize = 0);
    
    ~BlockingSharedMessageQueue();

    // This transfers the ownership of the message only if it is actually pushed in the queue (hence the unique_ptr)
    bool Enqueue(std::unique_ptr<IDynamicObject>& message, int32_t millisecondsTimeout);

    // This transfers the ownership of the message
    void Enqueue(IDynamicObject* message);

    // The caller is responsible to delete the dequeued message!
    IDynamicObject* Dequeue(int32_t millisecondsTimeout);

    void Clear();

    size_t GetSize();
  };
}
