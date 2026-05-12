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
#include "DataSourceReader.h"

#include "../Cache/SharedObjectCache.h"
#include "../OrthancException.h"
#include "DataSourceMemoryBudget.h"

#include <boost/make_shared.hpp>
#include <cassert>


namespace Orthanc
{
  class DataSourceReader::DataSourceRunnable : public IRunnable
  {
  private:
    boost::weak_ptr<DataSourceAnswer>                     answer_;
    IDataSource&                                          source_;
    std::unique_ptr<IDataIdentifier>                      id_;
    boost::shared_ptr<SharedObjectCache>                  cache_;
    boost::shared_ptr<Internals::DataSourceMemoryBudget>  budget_;

  public:
    DataSourceRunnable(boost::shared_ptr<DataSourceAnswer>& answer,
                       IDataSource& source,
                       IDataIdentifier* id,
                       boost::shared_ptr<SharedObjectCache>& cache,
                       boost::shared_ptr<Internals::DataSourceMemoryBudget>& budget) :
      answer_(answer),
      source_(source),
      id_(id),
      cache_(cache),
      budget_(budget)
    {
      if (id == NULL ||
          answer.get() == NULL)
      {
        throw OrthancException(ErrorCode_NullPointer);
      }
    }

    virtual void Run() ORTHANC_OVERRIDE
    {
      // Phase 1: Do all the work WITHOUT holding a strong reference to "DataSourceAnswer".
      boost::shared_ptr<IDynamicObject> value;
      std::unique_ptr<OrthancException> error;

      if (cache_)
      {
        value = cache_->GetCachedValue(id_->GetCacheKey());
      }

      if (!value)
      {
        try
        {
          value.reset(source_.Load(*id_));
        }
        catch (OrthancException& e)
        {
          error.reset(new OrthancException(e));
        }
        catch (...)
        {
          error.reset(new OrthancException(ErrorCode_InternalError));
        }

        if (!error && !value)
        {
          error.reset(new OrthancException(ErrorCode_NullPointer));
        }

        if (!error && cache_)
        {
          cache_->Store(id_->GetCacheKey(), value);
        }
      }

      // Phase 2: Acquire budget WITHOUT holding a strong reference to "DataSourceAnswer".
      // If the "DataSourceAnswer" has been dropped, "answer_.lock()" below will return NULL
      // and we will immediately "Release()" to revert this.
      size_t size = 0;

      if (!error)
      {
        size = source_.GetSize(*value);
        budget_->Acquire(size);   // may block; "DataSourceAnswer" CAN be destroyed here
      }

      // Phase 3: Only now take a strong reference to "DataSourceAnswer".
      {
        boost::shared_ptr<DataSourceAnswer> lock = answer_.lock();

        if (!lock)
        {
          // Answer was abandoned while we were blocked in Acquire().
          // Undo the charge and exit cleanly.
          if (!error)
          {
            budget_->Release(size);
          }
        }
        else
        {
          if (error)
          {
            lock->EnqueueError(id_.release(), *error);
          }
          else
          {
            lock->EnqueueValue(id_.release(), value, budget_, size);
          }
        }
      }
    }
  };


  DataSourceReader::DataSourceReader(IExecutorService* executor /* takes ownership */,
                                     IDataSource* source /* takes ownership */) :
    executor_(executor),
    source_(source),
    budget_(new Internals::DataSourceMemoryBudget(0))
  {
    if (executor == NULL ||
        source == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
  }


  DataSourceReader::~DataSourceReader()
  {
    executor_.reset();
    source_.reset();

    assert(budget_->GetCurrentMemory() == 0);
  }


  void DataSourceReader::CreateCache(IObjectSizeProvider* provider /* takes ownership */,
                                     size_t capacity)
  {
    cache_ = boost::make_shared<SharedObjectCache>(provider, capacity);
  }


  void DataSourceReader::SetMaximumMemory(uint64_t maximumMemory)
  {
    budget_ = boost::make_shared<Internals::DataSourceMemoryBudget>(maximumMemory);
  }


  boost::shared_ptr<DataSourceAnswer> DataSourceReader::Submit(DataSourceRequest* request /* takes ownership */)
  {
    std::unique_ptr<DataSourceRequest> protection(request);

    boost::shared_ptr<DataSourceAnswer> answer(new DataSourceAnswer(protection->GetSize()));

    while (!protection->IsEmpty())
    {
      std::unique_ptr<IDataIdentifier> identifier(protection->Dequeue());
      executor_->Submit(new DataSourceRunnable(answer, *source_, identifier.release(), cache_, budget_));
    }

    return answer;
  }


  boost::shared_ptr<IDynamicObject> DataSourceReader::ReadSingle(IDataIdentifier* id)
  {
    std::unique_ptr<IDataIdentifier> protection(id);

    std::unique_ptr<DataSourceRequest> request(new DataSourceRequest);
    request->Enqueue(protection.release());

    boost::shared_ptr<DataSourceAnswer> answer(Submit(request.release()));

    std::unique_ptr<DataSourceAnswer::Item> item(answer->Dequeue());
    assert(answer->Dequeue() == NULL);

    return item->GetValue();
  }
}
