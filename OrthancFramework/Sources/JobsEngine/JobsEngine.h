/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
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

#include "JobsRegistry.h"

#include "../Compatibility.h"

#include <boost/thread.hpp>

namespace Orthanc
{
  class ORTHANC_PUBLIC JobsEngine : public boost::noncopyable
  {
  private:
    enum State
    {
      State_Setup,
      State_Running,
      State_Stopping,
      State_Done
    };

    boost::mutex                 stateMutex_;
    State                        state_;
    std::unique_ptr<JobsRegistry>  registry_;
    boost::thread                retryHandler_;
    unsigned int                 threadSleep_;
    std::vector<boost::thread*>  workers_;

    bool IsRunning();
    
    bool ExecuteStep(JobsRegistry::RunningJob& running,
                     size_t workerIndex);
    
    static void RetryHandler(JobsEngine* engine);

    static void Worker(JobsEngine* engine,
                       size_t workerIndex);

  public:
    explicit JobsEngine(size_t maxCompletedJobs);

    ~JobsEngine();

    JobsRegistry& GetRegistry();

    void LoadRegistryFromJson(IJobUnserializer& unserializer,
                              const Json::Value& serialized);

    void LoadRegistryFromString(IJobUnserializer& unserializer,
                                const std::string& serialized);

    void SetWorkersCount(size_t count);

    void SetThreadSleep(unsigned int sleep);

    void Start();

    void Stop();
  };
}
