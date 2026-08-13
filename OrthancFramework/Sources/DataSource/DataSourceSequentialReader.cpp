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
#include "DataSourceSequentialReader.h"

#include "DataSourceReader.h"
#include "../OrthancException.h"

namespace Orthanc
{
  DataSourceSequentialReader::Item::Item(IDynamicObject* value,
                                         size_t estimatedSize) :
    value_(value),
    estimatedSize_(estimatedSize)
  {
    if (value == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
  }


  DataSourceSequentialReader::Item::Item(const OrthancException& error) :
    error_(new OrthancException(error)),
    estimatedSize_(0)
  {
  }


  const IDynamicObject& DataSourceSequentialReader::Item::GetValue() const
  {
    if (value_.get() != NULL)
    {
      assert(error_.get() == NULL);
      return *value_;
    }
    else if (error_.get() != NULL)
    {
      throw *error_;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }


  void DataSourceSequentialReader::Item::SetUserData(IDynamicObject* userData)
  {
    if (userData == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
    else
    {
      userData_.reset(userData);
    }
  }


  const IDynamicObject& DataSourceSequentialReader::Item::GetUserData() const
  {
    if (userData_.get() == NULL)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      return *userData_;
    }
  }


  IDynamicObject* DataSourceSequentialReader::Item::ReleaseValue()
  {
    if (value_.get() != NULL)
    {
      assert(error_.get() == NULL);
      return value_.release();
    }
    else if (error_.get() != NULL)
    {
      throw *error_;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }


  IDynamicObject* DataSourceSequentialReader::Item::ReleaseUserData()
  {
    if (userData_.get() == NULL)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      return userData_.release();
    }
  }


  class DataSourceSequentialReader::Callable : public ICallable
  {
  private:
    boost::shared_ptr<DataSourceReader>    reader_;
    boost::shared_ptr<IValueDisconnector>  disconnector_;
    std::unique_ptr<IDataIdentifier>       request_;
    size_t                                 estimatedSize_;

  public:
    Callable(const boost::shared_ptr<DataSourceReader>& reader,
             const boost::shared_ptr<IValueDisconnector>& disconnector,
             IDataIdentifier* request,
             size_t estimatedSize) :
      reader_(reader),
      disconnector_(disconnector),
      request_(request),
      estimatedSize_(estimatedSize)
    {
      if (reader.get() == NULL ||
          disconnector.get() == NULL ||
          request_ == NULL)
      {
        throw OrthancException(ErrorCode_NullPointer);
      }
    }

    virtual IDynamicObject* Call() ORTHANC_OVERRIDE
    {
      std::unique_ptr<DataSourceAnswer::Item> answer(reader_->ReadSingle(request_.release()));

      std::unique_ptr<IDynamicObject> userData;

      if (answer->HasUserData())
      {
        userData.reset(answer->ReleaseUserData());
      }

      std::unique_ptr<Item> item;

      try
      {
        std::unique_ptr<IDynamicObject> detached(disconnector_->Apply(answer.release()));
        if (detached.get() == NULL)
        {
          throw OrthancException(ErrorCode_NullPointer);
        }

        item.reset(new Item(detached.release(), estimatedSize_));
      }
      catch (OrthancException& e)
      {
        item.reset(new Item(e));
      }
      catch (...)
      {
        item.reset(new Item(OrthancException(ErrorCode_InternalError)));
      }

      if (userData.get() != NULL)
      {
        item->SetUserData(userData.release());
      }

      return item.release();
    }
  };


  void DataSourceSequentialReader::FillWindow()
  {
    while (!pendingRequests_.empty() &&
           runningRequests_.size() < windowSize_)
    {
      size_t estimatedSize;
      if (pendingRequests_.front()->EstimateValueSize(estimatedSize))
      {
        // Ensure that at least 1 callable is running even if too large for capacity
        if (!runningRequests_.empty() &&
            windowCapacity_ != 0 &&
            runningSize_ + estimatedSize > windowCapacity_)
        {
          return;
        }
      }
      else
      {
        estimatedSize = 0;
      }

      std::unique_ptr<IDataIdentifier> front(pendingRequests_.front());
      pendingRequests_.pop_front();

      runningSize_ += estimatedSize;

      std::unique_ptr<ICallable> callable(new Callable(reader_, disconnector_, front.release(), estimatedSize));
      runningRequests_.push_back(executor_->Submit(callable.release()));
    }
  }


  DataSourceSequentialReader::DataSourceSequentialReader(const boost::shared_ptr<IExecutorService>& executor,
                                                         const boost::shared_ptr<DataSourceReader>& reader,
                                                         IValueDisconnector* disconnector /* takes ownership */,
                                                         unsigned int windowSize,
                                                         uint64_t windowCapacity) :
    executor_(executor),
    reader_(reader),
    disconnector_(disconnector),
    windowSize_(windowSize),
    windowCapacity_(windowCapacity),
    started_(false),
    runningSize_(0)
  {
    if (executor.get() == NULL ||
        reader.get() == NULL ||
        disconnector == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }

    if (windowSize == 0)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }


  DataSourceSequentialReader::~DataSourceSequentialReader()
  {
    for (PendingRequests::iterator it = pendingRequests_.begin(); it != pendingRequests_.end(); ++it)
    {
      assert(*it != NULL);
      delete *it;
    }

    for (RunningRequests::iterator it = runningRequests_.begin(); it != runningRequests_.end(); ++it)
    {
      assert(*it != NULL);
      delete *it;
    }
  }


  void DataSourceSequentialReader::Submit(IDataIdentifier* request /* takes ownership */)
  {
    std::unique_ptr<IDataIdentifier> protection(request);

    if (request == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
    else if (started_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      pendingRequests_.push_back(protection.release());
    }
  }


  void DataSourceSequentialReader::Start()
  {
    if (started_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      started_ = true;
      FillWindow();
    }
  }


  bool DataSourceSequentialReader::HasNext() const
  {
    if (!started_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      return !runningRequests_.empty();
    }
  }


  DataSourceSequentialReader::Item* DataSourceSequentialReader::Next()
  {
    if (!started_ ||
        !HasNext())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      std::unique_ptr<Future> future(runningRequests_.front());
      runningRequests_.pop_front();

      std::unique_ptr<Item> item(dynamic_cast<Item*>(future->ReleaseResult()));
      runningSize_ -= item->GetEstimatedSize();

      FillWindow();

      return item.release();
    }
  }
}
