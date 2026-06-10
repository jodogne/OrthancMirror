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
#include "DataSourceMemoryBudget.h"

#include "../Constants.h"
#include "../MetricsRegistry.h"
#include "../OrthancException.h"

#include <math.h>

namespace Orthanc
{
  namespace Internals
  {
    DataSourceMemoryBudget::MetricsConfiguration::MetricsConfiguration(const boost::shared_ptr<MetricsRegistry>& metrics,
                                                                       const std::string& capacityMaxSizeMegabytesName,
                                                                       const std::string& capacityCurrentSizeMegabytesName,
                                                                       const std::string& capacityCountName,
                                                                       const std::string& capacityMaxUsageSinceStartMegabytesName) :
      maxMemoryUsageSinceStart_(0)
    {
      if (metrics.get() == NULL)
      {
        throw OrthancException(ErrorCode_NullPointer);
      }
      else if (capacityMaxSizeMegabytesName.empty() ||
               capacityCurrentSizeMegabytesName.empty() ||
               capacityCountName.empty() ||
               capacityMaxUsageSinceStartMegabytesName.empty())
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
      else
      {
        metrics_ = metrics;
        capacityMaxSizeMegabytesName_ = capacityMaxSizeMegabytesName;
        capacityCurrentSizeMegabytesName_ = capacityCurrentSizeMegabytesName;
        capacityCountName_ = capacityCountName;
        capacityMaxUsageSinceStartMegabytesName_ = capacityMaxUsageSinceStartMegabytesName;

        Update(0, 0, 0);
      }
    }

    void DataSourceMemoryBudget::MetricsConfiguration::Update(uint64_t maximumMemorySize, uint64_t currentMemorySize, unsigned int currentReservationCount)
    {
      maxMemoryUsageSinceStart_ = std::max(currentMemorySize, maxMemoryUsageSinceStart_);

      metrics_->SetFloatValue(capacityMaxSizeMegabytesName_, static_cast<float>(maximumMemorySize) / static_cast<float>(MEGABYTE)); // will be updated when we set the capacity
      metrics_->SetFloatValue(capacityCurrentSizeMegabytesName_, static_cast<float>(currentMemorySize) / static_cast<float>(MEGABYTE));
      metrics_->SetIntegerValue(capacityCountName_, currentReservationCount);
      metrics_->SetFloatValue(capacityMaxUsageSinceStartMegabytesName_, static_cast<float>(maxMemoryUsageSinceStart_) / static_cast<float>(MEGABYTE));
    }


    void DataSourceMemoryBudget::SetMetricsConfiguration(const MetricsConfiguration& configuration)
    {
      metricsConfiguration_ = configuration;
      
      boost::mutex::scoped_lock lock(mutex_);
      metricsConfiguration_.Update(maximumMemory_, currentMemory_, reservations_);
    }

    DataSourceMemoryBudget::DataSourceMemoryBudget(uint64_t maximumMemory) :
      maximumMemory_(maximumMemory),
      currentMemory_(0),
      reservations_(0)
    {
    }


    void DataSourceMemoryBudget::Acquire(size_t memory) ORTHANC_NOEXCEPT
    {
      boost::mutex::scoped_lock lock(mutex_);

      while (reservations_ != 0 &&  // Make sure that 1 thread can always proceed
             currentMemory_ + memory > maximumMemory_)
      {
        cond_.wait(lock);
      }

      currentMemory_ += memory;
      reservations_++;
      
      metricsConfiguration_.Update(maximumMemory_, currentMemory_, reservations_);
    }


    void DataSourceMemoryBudget::Release(size_t memory) ORTHANC_NOEXCEPT
    {
      boost::mutex::scoped_lock lock(mutex_);

      assert(currentMemory_ >= memory);
      currentMemory_ -= memory;

      assert(reservations_ > 0);
      reservations_--;
      
      metricsConfiguration_.Update(maximumMemory_, currentMemory_, reservations_);
      
      cond_.notify_all();
    }


    uint64_t DataSourceMemoryBudget::GetCurrentMemory() ORTHANC_NOEXCEPT
    {
      boost::mutex::scoped_lock lock(mutex_);
      return currentMemory_;
    }


    void DataSourceMemoryBudget::GetStatistics(uint64_t& maximumMemory,
                                               uint64_t& currentMemory,
                                               unsigned int& countReservations)

    {
      boost::mutex::scoped_lock lock(mutex_);
      maximumMemory = maximumMemory_;
      currentMemory = currentMemory_;
      countReservations = reservations_;
    }
  }
}
