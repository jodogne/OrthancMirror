/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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
#include "JobsEngine.h"

#include "../Logging.h"
#include "../OrthancException.h"
#include "../Toolbox.h"


namespace Orthanc
{
  bool JobsEngine::IsRunning()
  {
    boost::mutex::scoped_lock lock(stateMutex_);
    return (state_ == State_Running);
  }
  
  
  bool JobsEngine::ExecuteStep(JobsRegistry::RunningJob& running,
                               size_t workerIndex)
  {
    assert(running.IsValid());

    if (running.IsPauseScheduled())
    {
      running.GetJob().Stop(JobStopReason_Paused);
      running.MarkPause();
      return false;
    }

    if (running.IsCancelScheduled())
    {
      running.GetJob().Stop(JobStopReason_Canceled);
      running.MarkCanceled();
      return false;
    }

    JobStepResult result;

    try
    {
      result = running.GetJob().Step(running.GetId());
    }
    catch (OrthancException& e)
    {
      result = JobStepResult::Failure(e);
    }
    catch (boost::bad_lexical_cast&)
    {
      result = JobStepResult::Failure(ErrorCode_BadFileFormat, NULL);
    }
    catch (...)
    {
      result = JobStepResult::Failure(ErrorCode_InternalError, NULL);
    }

    switch (result.GetCode())
    {
      case JobStepCode_Success:
        running.GetJob().Stop(JobStopReason_Success);
        running.UpdateStatus(ErrorCode_Success, "");
        running.MarkSuccess();
        return false;

      case JobStepCode_Failure:
        running.GetJob().Stop(JobStopReason_Failure);
        running.UpdateStatus(result.GetFailureCode(), result.GetFailureDetails());
        running.MarkFailure();
        return false;

      case JobStepCode_Retry:
        running.GetJob().Stop(JobStopReason_Retry);
        running.UpdateStatus(ErrorCode_Success, "");
        running.MarkRetry(result.GetRetryTimeout());
        return false;

      case JobStepCode_Continue:
        running.UpdateStatus(ErrorCode_Success, "");
        return true;
            
      default:
        throw OrthancException(ErrorCode_InternalError);
    }
  }

    
  void JobsEngine::RetryHandler(JobsEngine* engine)
  {
    assert(engine != NULL);

    while (engine->IsRunning())
    {
      boost::this_thread::sleep(boost::posix_time::milliseconds(engine->threadSleep_));
      engine->GetRegistry().ScheduleRetries();
    }
  }

    
  void JobsEngine::Worker(JobsEngine* engine,
                          size_t workerIndex)
  {
    assert(engine != NULL);

    CLOG(INFO, JOBS) << "Worker thread " << workerIndex << " has started";

    while (engine->IsRunning())
    {
      JobsRegistry::RunningJob running(engine->GetRegistry(), engine->threadSleep_);

      if (running.IsValid())
      {
        CLOG(INFO, JOBS) << "Executing job with priority " << running.GetPriority()
                         << " in worker thread " << workerIndex << ": " << running.GetId();

        while (engine->IsRunning())
        {
          if (!engine->ExecuteStep(running, workerIndex))
          {
            break;
          }
        }
      }
    }      
  }


  JobsEngine::JobsEngine(size_t maxCompletedJobs) :
    state_(State_Setup),
    registry_(new JobsRegistry(maxCompletedJobs)),
    threadSleep_(200),
    workers_(1)
  {
  }

    
  JobsEngine::~JobsEngine()
  {
    if (state_ != State_Setup &&
        state_ != State_Done)
    {
      CLOG(ERROR, JOBS) << "INTERNAL ERROR: JobsEngine::Stop() should be invoked manually to avoid mess in the destruction order!";
      Stop();
    }
  }

 
  JobsRegistry& JobsEngine::GetRegistry()
  {
    if (registry_.get() == NULL)
    {
      throw OrthancException(ErrorCode_InternalError);
    }

    return *registry_;
  }
  
   
  void JobsEngine::LoadRegistryFromJson(IJobUnserializer& unserializer,
                                        const Json::Value& serialized)
  {
    boost::mutex::scoped_lock lock(stateMutex_);
      
    if (state_ != State_Setup)
    {
      // Can only be invoked before calling "Start()"
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    assert(registry_.get() != NULL);
    const size_t maxCompletedJobs = registry_->GetMaxCompletedJobs();
    registry_.reset(new JobsRegistry(unserializer, serialized, maxCompletedJobs));
  }


  void JobsEngine::LoadRegistryFromString(IJobUnserializer& unserializer,
                                          const std::string& serialized)
  {
    Json::Value value;
    if (Toolbox::ReadJson(value, serialized))
    {
      LoadRegistryFromJson(unserializer, value);
    }
    else
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }
  }


  void JobsEngine::SetWorkersCount(size_t count)
  {
    boost::mutex::scoped_lock lock(stateMutex_);
      
    if (state_ != State_Setup)
    {
      // Can only be invoked before calling "Start()"
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    workers_.resize(count);
  }


  void JobsEngine::SetThreadSleep(unsigned int sleep)
  {
    boost::mutex::scoped_lock lock(stateMutex_);
      
    if (state_ != State_Setup)
    {
      // Can only be invoked before calling "Start()"
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    threadSleep_ = sleep;
  }


  void JobsEngine::Start()
  {
    boost::mutex::scoped_lock lock(stateMutex_);

    if (state_ != State_Setup)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    retryHandler_ = boost::thread(RetryHandler, this);

    if (workers_.size() == 0)
    {
      // Use all the available CPUs
      size_t n = boost::thread::hardware_concurrency();
      
      if (n == 0)
      {
        n = 1;
      }

      workers_.resize(n);
    }      

    for (size_t i = 0; i < workers_.size(); i++)
    {
      assert(workers_[i] == NULL);
      workers_[i] = new boost::thread(Worker, this, i);
    }

    state_ = State_Running;

    CLOG(WARNING, JOBS) << "The jobs engine has started with " << workers_.size() << " threads";
  }


  void JobsEngine::Stop()
  {
    {
      boost::mutex::scoped_lock lock(stateMutex_);

      if (state_ != State_Running)
      {
        return;
      }
        
      state_ = State_Stopping;
    }

    CLOG(INFO, JOBS) << "Stopping the jobs engine";
      
    if (retryHandler_.joinable())
    {
      retryHandler_.join();
    }
      
    for (size_t i = 0; i < workers_.size(); i++)
    {
      assert(workers_[i] != NULL);

      if (workers_[i]->joinable())
      {
        workers_[i]->join();
      }

      delete workers_[i];
    }
      
    {
      boost::mutex::scoped_lock lock(stateMutex_);
      state_ = State_Done;
    }

    CLOG(WARNING, JOBS) << "The jobs engine has stopped";
  }
}
