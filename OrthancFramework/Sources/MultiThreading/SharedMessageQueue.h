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


#pragma once

#include "../IDynamicObject.h"

#include <stdint.h>
#include <list>
#include <boost/thread.hpp>

namespace Orthanc
{
  class ORTHANC_PUBLIC SharedMessageQueue : public boost::noncopyable
  {
  private:
    typedef std::list<IDynamicObject*>  Queue;

    bool isFifo_;
    unsigned int maxSize_;
    Queue queue_;
    boost::mutex mutex_;
    boost::condition_variable elementAvailable_;
    boost::condition_variable emptied_;

  public:
    explicit SharedMessageQueue(unsigned int maxSize = 0);
    
    ~SharedMessageQueue();

    // This transfers the ownership of the message
    void Enqueue(IDynamicObject* message);

    // The caller is responsible to delete the dequeud message!
    IDynamicObject* Dequeue(int32_t millisecondsTimeout);

    bool WaitEmpty(int32_t millisecondsTimeout);

    bool IsFifoPolicy() const;

    bool IsLifoPolicy() const;

    void SetFifoPolicy();

    void SetLifoPolicy();

    void Clear();

    size_t GetSize();
  };
}
