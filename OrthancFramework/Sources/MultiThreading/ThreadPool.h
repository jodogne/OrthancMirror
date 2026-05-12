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
#include "IExecutorService.h"
#include "SharedMessageQueue.h"

#include <boost/thread.hpp>

namespace Orthanc
{
  class ORTHANC_PUBLIC ThreadPool : public IExecutorService
  {
  private:
    enum State
    {
      State_Initialization,
      State_Running,
      State_Finalization
    };

    class ITask;
    class CallableTask;
    class RunnableTask;

    SharedMessageQueue                    queue_;
    std::unique_ptr<boost::thread_group>  workers_;
    boost::mutex                          mutex_;
    std::string                           loggingThreadName_;
    unsigned int                          countThreads_;
    State                                 state_;
    unsigned int                          dequeueTimeoutMilliseconds_;

    void StopInternal(bool throws);

    void WorkerLoop(std::string threadName);

  public:
    ThreadPool();

    ~ThreadPool()
    {
      StopInternal(false /* don't throw in destructor */);
    }

    void SetLoggingThreadName(const std::string& name);

    void SetCountThreads(unsigned int count);

    unsigned int GetCountThreads();

    void SetDequeueTimeout(unsigned int milliseconds);

    unsigned int GetDequeueTimeout();

    void Start();

    virtual Future* Submit(ICallable* callable /* takes ownership */) ORTHANC_OVERRIDE;

    virtual void Submit(IRunnable* runnable /* takes ownership */) ORTHANC_OVERRIDE;

    virtual void Stop() ORTHANC_OVERRIDE
    {
      StopInternal(true);
    }
  };
}
