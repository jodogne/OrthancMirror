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
#include "../Constants.h"
#include "../MetricsRegistry.h"
#include "../OrthancException.h"
#include "DataSourceMemoryBudget.h"

#include <boost/make_shared.hpp>
#include <boost/weak_ptr.hpp>
#include <cassert>


namespace Orthanc
{
  DataSourceReader::MetricsConfiguration::MetricsConfiguration(const boost::shared_ptr<MetricsRegistry>& metrics,
                                                               const std::string& cacheSizeMegabytesName,
                                                               const std::string& cacheCountName,
                                                               const std::string& cacheHitCountName,
                                                               const std::string& cacheMissCountName,
                                                               const std::string& capacityMaxSizeMegabytesName,
                                                               const std::string& capacityCurrentSizeMegabytesName,
                                                               const std::string& capacityCountName,
                                                               const std::string& capacityMaxUsageSinceStartMegabytesName)
  {
    if (metrics.get() == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
    else if (cacheSizeMegabytesName.empty() ||
             cacheCountName.empty() ||
             cacheHitCountName.empty() ||
             cacheMissCountName.empty() || 
             capacityMaxSizeMegabytesName.empty() ||
             capacityCurrentSizeMegabytesName.empty() ||
             capacityCountName.empty() ||
             capacityMaxUsageSinceStartMegabytesName.empty())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
    else
    {
      metrics_ = metrics;
      cacheSizeMegabytesName_ = cacheSizeMegabytesName;
      cacheCountName_ = cacheCountName;
      cacheHitCountName_ = cacheHitCountName;
      cacheMissCountName_ = cacheMissCountName;
      capacityMaxSizeMegabytesName_ = capacityMaxSizeMegabytesName;
      capacityCurrentSizeMegabytesName_ = capacityCurrentSizeMegabytesName;
      capacityCountName_ = capacityCountName;
      capacityMaxUsageSinceStartMegabytesName_ = capacityMaxUsageSinceStartMegabytesName;

      metrics_->SetFloatValue(cacheSizeMegabytesName_, 0);
      metrics_->SetIntegerValue(cacheCountName_, 0);
      metrics_->SetIntegerValue(cacheHitCountName_, 0);
      metrics_->SetIntegerValue(cacheMissCountName_, 0);
      metrics_->SetFloatValue(capacityMaxSizeMegabytesName_, 0); // will be updated when we set the capacity
      metrics_->SetFloatValue(capacityCurrentSizeMegabytesName_, 0);
      metrics_->SetIntegerValue(capacityCountName_, 0);
      metrics_->SetFloatValue(capacityMaxUsageSinceStartMegabytesName_, 0);
    }
  }


  void DataSourceReader::MetricsConfiguration::SetCacheStatistics(SharedObjectCache& cache)
  {
    if (metrics_)
    {
      size_t count, size;
      cache.GetStatistics(count, size);

      metrics_->SetFloatValue(cacheSizeMegabytesName_, BytesToFloatMegabytes(size));
      metrics_->SetIntegerValue(cacheCountName_, count);
    }
  }


  void DataSourceReader::MetricsConfiguration::IncrementCacheHitCount()
  {
    if (metrics_)
    {
      metrics_->IncrementIntegerValue(cacheHitCountName_, 1);
    }
  }


  void DataSourceReader::MetricsConfiguration::IncrementCacheMissCount()
  {
    if (metrics_)
    {
      metrics_->IncrementIntegerValue(cacheMissCountName_, 1);
    }
  }


  class DataSourceReader::DataSourceRunnable : public IRunnable
  {
  private:
    boost::weak_ptr<DataSourceAnswer>                     answer_;
    IDataSource&                                          source_;
    std::unique_ptr<IDataIdentifier>                      id_;
    boost::shared_ptr<SharedObjectCache>                  cache_;
    boost::shared_ptr<Internals::DataSourceMemoryBudget>  budget_;
    MetricsConfiguration                                  metricsConfiguration_;

  public:
    DataSourceRunnable(const boost::shared_ptr<DataSourceAnswer>& answer,
                       IDataSource& source,
                       IDataIdentifier* id,
                       boost::shared_ptr<SharedObjectCache>& cache,
                       boost::shared_ptr<Internals::DataSourceMemoryBudget>& budget,
                       const MetricsConfiguration& metrics) :
      answer_(answer),
      source_(source),
      id_(id),
      cache_(cache),
      budget_(budget),
      metricsConfiguration_(metrics)
    {
      if (id == NULL ||
          answer.get() == NULL)
      {
        throw OrthancException(ErrorCode_NullPointer);
      }
    }

    virtual void Run() ORTHANC_OVERRIDE
    {
      // Phase 1: Make sure the target answer is still alive before doing unnecessary work.
      {
        boost::shared_ptr<DataSourceAnswer> lock = answer_.lock();
        if (!lock)
        {
          // The answer was abandonned, give up the request
          return;
        }
      }

      // Phase 2: Do all the work WITHOUT holding a strong reference to "DataSourceAnswer".
      boost::shared_ptr<IDynamicObject> value;
      std::unique_ptr<OrthancException> error;

      size_t size = 0;

      try
      {
        std::string cacheKey;
        bool hasCacheKey = id_->GetCacheKey(cacheKey);

        if (cache_ && hasCacheKey)
        {
          value = cache_->GetCachedValue(cacheKey);

          if (value)
          {
            metricsConfiguration_.IncrementCacheHitCount();
          }
          else
          {
            metricsConfiguration_.IncrementCacheMissCount();
          }
        }

        if (!value)
        {
          std::unique_ptr<Internals::DataSourceMemoryBudget::Lock> preReservation;

          size_t estimatedSize = 0;
          if (id_->EstimateValueSize(estimatedSize))
          {
            preReservation.reset(new Internals::DataSourceMemoryBudget::Lock(*budget_, estimatedSize));
          }

          value.reset(source_.Load(*id_, cache_));

          if (!value)
          {
            error.reset(new OrthancException(ErrorCode_NullPointer));
          }

          if (!error && cache_ && hasCacheKey)
          {
            cache_->Store(cacheKey, value, source_.GetValueSize(*value));
            metricsConfiguration_.SetCacheStatistics(*cache_);
          }
        }

        if (!error)
        {
          size = source_.GetValueSize(*value);
        }
      }
      catch (OrthancException& e)
      {
        error.reset(new OrthancException(e));
      }
      catch (...)
      {
        error.reset(new OrthancException(ErrorCode_InternalError, "Unknown exception in Datasource::Run"));
      }

      // Phase 3: Acquire budget WITHOUT holding a strong reference to "DataSourceAnswer".
      // If the "DataSourceAnswer" has been dropped, "answer_.lock()" below will return NULL
      // and we will immediately "Release()" to revert this.
      if (!error)
      {
        budget_->Acquire(size);   // may block; "DataSourceAnswer" CAN be destroyed here
      }

      // Phase 4: Only now take a strong reference to "DataSourceAnswer".
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


  DataSourceReader::DataSourceReader(const boost::shared_ptr<IExecutorService>& executor /* takes ownership */,
                                     IDataSource* source /* takes ownership */) :
    executor_(executor),
    source_(source),
    budget_(new Internals::DataSourceMemoryBudget(0))
  {
    if (executor.get() == NULL ||
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


  void DataSourceReader::CreateCache(size_t capacity)
  {
    cache_ = boost::make_shared<SharedObjectCache>(capacity);
  }

  void DataSourceReader::SetMetricsConfiguration(const MetricsConfiguration& configuration)
  {
    metricsConfiguration_ = configuration;
    if (budget_.get())
    {
      budget_->SetMetricsConfiguration(Internals::DataSourceMemoryBudget::MetricsConfiguration(configuration.metrics_,
                                                                                               configuration.capacityMaxSizeMegabytesName_,
                                                                                               configuration.capacityCurrentSizeMegabytesName_,
                                                                                               configuration.capacityCountName_,
                                                                                               configuration.capacityMaxUsageSinceStartMegabytesName_));
    }
  }


  void DataSourceReader::SetCapacity(uint64_t maximumMemory)
  {
    budget_ = boost::make_shared<Internals::DataSourceMemoryBudget>(maximumMemory);
    budget_->SetMetricsConfiguration(Internals::DataSourceMemoryBudget::MetricsConfiguration(metricsConfiguration_.metrics_,
                                                                                             metricsConfiguration_.capacityMaxSizeMegabytesName_,
                                                                                             metricsConfiguration_.capacityCurrentSizeMegabytesName_,
                                                                                             metricsConfiguration_.capacityCountName_,
                                                                                             metricsConfiguration_.capacityMaxUsageSinceStartMegabytesName_));
  }

  uint64_t DataSourceReader::GetCapacity() const
  {
    uint64_t maximumMemory, currentMemory;
    unsigned int currentReservations;

    GetStatistics(maximumMemory, currentMemory, currentReservations);
    
    return maximumMemory;
  }


  boost::shared_ptr<DataSourceAnswer> DataSourceReader::Submit(DataSourceRequest* request /* takes ownership */)
  {
    std::unique_ptr<DataSourceRequest> protection(request);

    boost::shared_ptr<DataSourceAnswer> answer(new DataSourceAnswer(protection->GetSize()));

    while (!protection->IsEmpty())
    {
      std::unique_ptr<IDataIdentifier> identifier(protection->Dequeue());
      executor_->Submit(new DataSourceRunnable(answer, *source_, identifier.release(), cache_, budget_, metricsConfiguration_));
    }

    return answer;
  }


  DataSourceAnswer::Item* DataSourceReader::ReadSingle(IDataIdentifier* id /* takes ownership */)
  {
    std::unique_ptr<IDataIdentifier> protection(id);

    std::unique_ptr<DataSourceRequest> request(new DataSourceRequest);
    request->Enqueue(protection.release());

    boost::shared_ptr<DataSourceAnswer> answer(Submit(request.release()));

    std::unique_ptr<DataSourceAnswer::Item> item(answer->Dequeue());
    assert(item != NULL);
    assert(answer->Dequeue() == NULL);

    return item.release();
  }


  void DataSourceReader::GetStatistics(uint64_t& tasksMaximumMemory,
                                       uint64_t& tasksCurrentMemory,
                                       unsigned int& tasksReservations) const
  {
    { // TODO-Streaming - Question AM: why this copy ?
      boost::shared_ptr<Internals::DataSourceMemoryBudget> budgetCopy(budget_);
      budgetCopy->GetStatistics(tasksMaximumMemory, tasksCurrentMemory, tasksReservations);
    }
  }


  size_t DataSourceReader::GetCacheCapacity() const
  {
    boost::shared_ptr<SharedObjectCache> lock(cache_);

    if (lock)
    {
      return lock->GetCapacity();
    }
    else
    {
      return 0;
    }
  }


  void DataSourceReader::StoreIntoCache(const std::string& key,
                                        IDynamicObject* value /* takes ownership */)
  {
    boost::shared_ptr<IDynamicObject> protection(value);

    if (value == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }

    if (cache_)
    {
      size_t size = source_->GetValueSize(*value);
      cache_->Store(key, protection, size);
      metricsConfiguration_.SetCacheStatistics(*cache_);
    }
  }
}
