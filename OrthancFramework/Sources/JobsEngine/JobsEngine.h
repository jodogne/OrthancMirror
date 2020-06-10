/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * In addition, as a special exception, the copyright holders of this
 * program give permission to link the code of its release with the
 * OpenSSL project's "OpenSSL" library (or with modified versions of it
 * that use the same license as the "OpenSSL" library), and distribute
 * the linked executables. You must obey the GNU General Public License
 * in all respects for all of the code used other than "OpenSSL". If you
 * modify file(s) with this exception, you may extend this exception to
 * your version of the file(s), but you are not obligated to do so. If
 * you do not wish to do so, delete this exception statement from your
 * version. If you delete this exception statement from all source files
 * in the program, then also delete it here.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#pragma once

#include "JobsRegistry.h"

#include "../Compatibility.h"

#include <boost/thread.hpp>

namespace Orthanc
{
  class JobsEngine : public boost::noncopyable
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
    JobsEngine(size_t maxCompletedJobs);

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
