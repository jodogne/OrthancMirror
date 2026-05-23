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

#include "../MultiThreading/IExecutorService.h"
#include "DataSourceAnswer.h"
#include "DataSourceRequest.h"
#include "IDataSource.h"

#include <stdint.h>


namespace Orthanc
{
  class SharedObjectCache;

  namespace Internals
  {
    class DataSourceMemoryBudget;
  }

  class ORTHANC_PUBLIC DataSourceReader : public boost::noncopyable
  {
  private:
    class DataSourceRunnable;

    boost::shared_ptr<IExecutorService>                   executor_;
    std::unique_ptr<IDataSource>                          source_;
    boost::shared_ptr<SharedObjectCache>                  cache_;
    boost::shared_ptr<Internals::DataSourceMemoryBudget>  budget_;

  public:
    DataSourceReader(const boost::shared_ptr<IExecutorService>& executor,
                     IDataSource* source /* takes ownership */);

    ~DataSourceReader();

    void CreateCache(size_t capacity);

    /**
     * Apply backpressure by limiting memory for pending read
     * tasks. Further reads block until memory is released.
     **/
    void SetCapacity(uint64_t maximumMemory);

    /**
     * Request the data source to load a set of items. The values will
     * be read in parallel by a thread pool (cf. "executor_") and will
     * be answered in no specific order through a message queue
     * (cf. class "DataSourceAnswer"). The user data stored in
     * "IDataIdentifier" can be used to identify items. If order is
     * important, use the class "DataSourceSequentialReader" instead.
     **/
    boost::shared_ptr<DataSourceAnswer> Submit(DataSourceRequest* request /* takes ownership */);

    DataSourceAnswer::Item* ReadSingle(IDataIdentifier* id /* takes ownership */);

    void Stop()
    {
      executor_->Stop();
    }

    void GetStatistics(uint64_t& tasksMaximumMemory,
                       uint64_t& tasksCurrentMemory,
                       unsigned int& tasksReservations,
                       size_t& cacheCapacity,
                       size_t& cacheCurrentCount,
                       size_t& cacheCurrentSize);
  };
}
