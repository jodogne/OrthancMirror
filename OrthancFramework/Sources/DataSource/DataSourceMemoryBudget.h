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


#pragma once

#if defined(__EMSCRIPTEN__)
#  error This file is currently not available if targeting WebAssembly
#endif

#include "../Compatibility.h"
#include "../OrthancFramework.h"

#include <boost/noncopyable.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>
#include <stdint.h>


namespace Orthanc
{
  class MetricsRegistry;

  namespace Internals
  {
    // This is basically a semaphore with the uint64_t data type (with metrics !)
    class ORTHANC_PUBLIC DataSourceMemoryBudget : public boost::noncopyable
    {
    public:
      class MetricsConfiguration
      {
      private:
        boost::shared_ptr<MetricsRegistry>  metrics_;
        std::string                         capacityMaxSizeMegabytesName_;
        std::string                         capacityCurrentSizeMegabytesName_;
        std::string                         capacityCountName_;
        std::string                         capacityMaxUsageSinceStartMegabytesName_;
        uint64_t                            maxMemoryUsageSinceStart_;

      public:
        MetricsConfiguration()
        {
        }

        MetricsConfiguration(const boost::shared_ptr<MetricsRegistry>& metrics,
                             const std::string& capacityMaxSizeMegabytesName,
                             const std::string& capacityCurrentSizeMegabytesName,
                             const std::string& capacityCountName,
                             const std::string& capacityMaxUsageSinceStartMegabytesName);

        void Update(uint64_t maximumMemorySize, uint64_t currentMemorySize, unsigned int currentReservationCount);
      };


    private:
      boost::mutex               mutex_;
      boost::condition_variable  cond_;
      const uint64_t             maximumMemory_;
      uint64_t                   currentMemory_;
      unsigned int               reservations_;
      MetricsConfiguration       metricsConfiguration_;

    public:
      explicit DataSourceMemoryBudget(uint64_t maximumMemory); //, const MetricsConfiguration& metricsConfiguration);

      void Acquire(size_t memory) ORTHANC_NOEXCEPT;

      void Release(size_t memory) ORTHANC_NOEXCEPT;

      void SetMetricsConfiguration(const MetricsConfiguration& configuration);

      uint64_t GetCurrentMemory() ORTHANC_NOEXCEPT;

      class ORTHANC_PUBLIC Lock : public boost::noncopyable
      {
      private:
        DataSourceMemoryBudget&  that_;
        size_t                   memory_;

      public:
        explicit Lock(DataSourceMemoryBudget& that,
                      size_t memory) :
          that_(that),
          memory_(memory)
        {
          that_.Acquire(memory_);
        }

        ~Lock()
        {
          that_.Release(memory_);
        }
      };

      void GetStatistics(uint64_t& maximumMemory,
                         uint64_t& currentMemory,
                         unsigned int& countReservations);
    };
  }
}
