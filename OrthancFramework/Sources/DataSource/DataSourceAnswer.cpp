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
#include "DataSourceAnswer.h"

#include "../OrthancException.h"
#include "DataSourceMemoryBudget.h"

#include <cassert>


namespace Orthanc
{
  DataSourceAnswer::Item::Item(IDataIdentifier* id /* takes ownership */,
                               boost::shared_ptr<IDynamicObject>& value,
                               boost::shared_ptr<Internals::DataSourceMemoryBudget>& budget,
                               size_t memorySize) :
    id_(id),
    value_(value),
    budget_(budget),
    memorySize_(memorySize)
  {
    if (id == NULL ||
        value == NULL ||
        budget_ == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
  }


  DataSourceAnswer::Item::Item(IDataIdentifier* id /* takes ownership */,
                               const OrthancException& error) :
    id_(id),
    error_(new OrthancException(error)),
    memorySize_(0)
  {
    if (id == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
  }


  DataSourceAnswer::Item::~Item()
  {
    if (budget_)
    {
      budget_->Release(memorySize_);
    }
  }


  const IDataIdentifier& DataSourceAnswer::Item::GetId() const
  {
    if (id_.get() == NULL)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      return *id_;
    }
  }


  const boost::shared_ptr<IDynamicObject>& DataSourceAnswer::Item::GetValue() const
  {
    if (error_)
    {
      throw *error_;
    }
    else if (value_)
    {
      return value_;
    }
    else
    {
      throw OrthancException(ErrorCode_InternalError);
    }
  }


  IDataIdentifier* DataSourceAnswer::Item::ReleaseIdentifier()
  {
    if (id_.get() == NULL)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      return id_.release();
    }
  }


  void DataSourceAnswer::EnqueueInternal(Item* item)
  {
    std::unique_ptr<Item> protection(item);

    if (item == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }

    {
      boost::mutex::scoped_lock lock(mutex_);

      if (countEnqueued_ >= finalSize_)
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }
      else
      {
        content_.push_back(protection.release());
        countEnqueued_++;

        cond_.notify_all();
      }
    }
  }


  DataSourceAnswer::DataSourceAnswer(unsigned int finalSize) :
    countEnqueued_(0),
    finalSize_(finalSize)
  {
  }


  DataSourceAnswer::~DataSourceAnswer()
  {
    for (Content::iterator it = content_.begin(); it != content_.end(); ++it)
    {
      assert(*it != NULL);
      delete *it;
    }
  }


  DataSourceAnswer::Item* DataSourceAnswer::Dequeue()
  {
    boost::mutex::scoped_lock lock(mutex_);

    while (countEnqueued_ < finalSize_ &&
           content_.empty())
    {
      cond_.wait(lock);
    }

    assert(countEnqueued_ <= finalSize_);

    if (content_.empty())
    {
      return NULL;
    }
    else
    {
      Item* front = content_.front();
      content_.pop_front();
      return front;
    }
  }


  void DataSourceAnswer::EnqueueValue(IDataIdentifier* id,
                                      boost::shared_ptr<IDynamicObject>& value,
                                      boost::shared_ptr<Internals::DataSourceMemoryBudget>& budget,
                                      size_t memorySize)
  {
    EnqueueInternal(new Item(id, value, budget, memorySize));
  }


  void DataSourceAnswer::EnqueueError(IDataIdentifier* id,
                                      const OrthancException& error)
  {
    EnqueueInternal(new Item(id, error));
  }
}
