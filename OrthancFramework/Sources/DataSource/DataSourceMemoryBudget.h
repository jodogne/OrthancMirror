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

#include "../Compatibility.h"

#include <boost/noncopyable.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>
#include <stdint.h>


namespace Orthanc
{
  namespace Internals
  {
    // This is basically a semaphore with the uint64_t data type
    class DataSourceMemoryBudget : public boost::noncopyable
    {
    private:
      boost::mutex               mutex_;
      boost::condition_variable  cond_;
      const uint64_t             maximumMemory_;
      uint64_t                   currentMemory_;

    public:
      DataSourceMemoryBudget(uint64_t maximumMemory);

      void Acquire(size_t memory) ORTHANC_NOEXCEPT;

      void Release(size_t memory) ORTHANC_NOEXCEPT;

      uint64_t GetCurrentMemory() ORTHANC_NOEXCEPT;
    };
  }
}
