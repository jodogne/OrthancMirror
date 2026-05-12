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

    std::unique_ptr<IExecutorService>                     executor_;
    std::unique_ptr<IDataSource>                          source_;
    boost::shared_ptr<SharedObjectCache>                  cache_;
    boost::shared_ptr<Internals::DataSourceMemoryBudget>  budget_;

  public:
    DataSourceReader(IExecutorService* executor /* takes ownership */,
                     IDataSource* source /* takes ownership */);

    ~DataSourceReader();

    void CreateCache(IObjectSizeProvider* provider /* takes ownership */,
                     size_t capacity);

    void SetMaximumMemory(uint64_t maximumMemory);

    boost::shared_ptr<DataSourceAnswer> Submit(DataSourceRequest* request /* takes ownership */);

    void Stop()
    {
      executor_->Stop();
    }
  };
}
