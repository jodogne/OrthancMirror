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
#include "CallableGroup.h"

#include "../OrthancException.h"

#include <cassert>


namespace Orthanc
{
  void CallableGroup::FillWindow()
  {
    while (!pending_.empty() &&
           (windowSize_ == 0 ||
            futures_.size() < windowSize_))
    {
      std::unique_ptr<ICallable> callable(pending_.front());
      assert(callable.get() != NULL);

      pending_.pop_front();

      futures_.push_back(executor_->Submit(callable.release()));
    }
  }


  CallableGroup::CallableGroup(boost::shared_ptr<IExecutorService>  executor,
                               unsigned int windowSize) :
    executor_(executor),
    windowSize_(windowSize),
    hasIterator_(false)
  {
  }


  CallableGroup::~CallableGroup()
  {
    for (std::list<ICallable*>::iterator it = pending_.begin(); it != pending_.end(); ++it)
    {
      assert(*it != NULL);
      delete *it;
    }

    for (std::list<Future*>::iterator it = futures_.begin(); it != futures_.end(); ++it)
    {
      assert(*it != NULL);
      delete *it;
    }
  }


  void CallableGroup::Submit(ICallable* callable /* takes ownership */)
  {
    std::unique_ptr<ICallable> protection(callable);

    if (callable == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }

    if (hasIterator_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    pending_.push_back(protection.release());
  }


  CallableGroup::Iterator::Iterator(CallableGroup& that) :
    that_(that)
  {
    if (that_.hasIterator_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      that_.hasIterator_ = true;
      that_.FillWindow();
    }
  }


  bool CallableGroup::Iterator::HasNext() const
  {
    return !that_.futures_.empty();
  }


  IDynamicObject* CallableGroup::Iterator::Next()
  {
    if (HasNext())
    {
      std::unique_ptr<IDynamicObject> result;

      {
        std::unique_ptr<Future> future(that_.futures_.front());
        assert(future.get() != NULL);

        result.reset(future->ReleaseResult());

        that_.futures_.pop_front();
      }

      that_.FillWindow();

      return result.release();
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }
}
