/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2018 Osimis S.A., Belgium
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


#include "../PrecompiledHeaders.h"
#include "JobsEngine.h"

#include "JobStepRetry.h"

#include "../Logging.h"
#include "../OrthancException.h"

namespace Orthanc
{
  bool JobsEngine::ExecuteStep(JobsRegistry::RunningJob& running,
                               size_t workerIndex)
  {
    assert(running.IsValid());

    LOG(INFO) << "Executing job with priority " << running.GetPriority()
              << " in worker thread " << workerIndex << ": " << running.GetId();

    if (running.IsPauseScheduled())
    {
      running.GetJob().ReleaseResources();
      running.MarkPause();
      return false;
    }

    std::auto_ptr<JobStepResult> result;

    {
      try
      {
        result.reset(running.GetJob().ExecuteStep());

        if (result->GetCode() == JobStepCode_Failure)
        {
          running.UpdateStatus(ErrorCode_InternalError);
        }
        else
        {
          running.UpdateStatus(ErrorCode_Success);
        }
      }
      catch (OrthancException& e)
      {
        running.UpdateStatus(e.GetErrorCode());
      }
      catch (boost::bad_lexical_cast&)
      {
        running.UpdateStatus(ErrorCode_BadFileFormat);
      }
      catch (...)
      {
        running.UpdateStatus(ErrorCode_InternalError);
      }

      if (result.get() == NULL)
      {
        result.reset(new JobStepResult(JobStepCode_Failure));
      }
    }

    switch (result->GetCode())
    {
      case JobStepCode_Success:
        running.MarkSuccess();
        return false;

      case JobStepCode_Failure:
        running.MarkFailure();
        return false;

      case JobStepCode_Retry:
        running.MarkRetry(dynamic_cast<JobStepRetry&>(*result).GetTimeout());
        return false;

      case JobStepCode_Continue:
        return true;
            
      default:
        throw OrthancException(ErrorCode_InternalError);
    }
  }

    
  void JobsEngine::RetryHandler(JobsEngine* engine)
  {
    assert(engine != NULL);

    for (;;)
    {
      boost::this_thread::sleep(boost::posix_time::milliseconds(200));

      {
        boost::mutex::scoped_lock lock(engine->stateMutex_);

        if (engine->state_ != State_Running)
        {
          return;
        }
      }

      engine->GetRegistry().ScheduleRetries();
    }
  }

    
  void JobsEngine::Worker(JobsEngine* engine,
                          size_t workerIndex)
  {
    assert(engine != NULL);

    LOG(INFO) << "Worker thread " << workerIndex << " has started";

    for (;;)
    {
      {
        boost::mutex::scoped_lock lock(engine->stateMutex_);

        if (engine->state_ != State_Running)
        {
          return;
        }
      }

      JobsRegistry::RunningJob running(engine->GetRegistry(), 100);

      if (running.IsValid())
      {
        for (;;)
        {
          if (!engine->ExecuteStep(running, workerIndex))
          {
            break;
          }
        }
      }
    }      
  }


  JobsEngine::JobsEngine() :
    state_(State_Setup),
    workers_(1)
  {
  }

    
  JobsEngine::~JobsEngine()
  {
    if (state_ != State_Setup &&
        state_ != State_Done)
    {
      LOG(ERROR) << "INTERNAL ERROR: JobsEngine::Stop() should be invoked manually to avoid mess in the destruction order!";
      Stop();
    }
  }

    
  void JobsEngine::SetWorkersCount(size_t count)
  {
    if (count == 0)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
      
    boost::mutex::scoped_lock lock(stateMutex_);
      
    if (state_ != State_Setup)
    {
      // Can only be invoked before calling "Start()"
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    workers_.resize(count);
  }
    

  void JobsEngine::Start()
  {
    boost::mutex::scoped_lock lock(stateMutex_);

    if (state_ != State_Setup)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    retryHandler_ = boost::thread(RetryHandler, this);

    for (size_t i = 0; i < workers_.size(); i++)
    {
      workers_[i] = boost::thread(Worker, this, i);
    }

    state_ = State_Running;

    LOG(WARNING) << "The jobs engine has started";
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

    LOG(INFO) << "Stopping the jobs engine";
      
    if (retryHandler_.joinable())
    {
      retryHandler_.join();
    }
      
    for (size_t i = 0; i < workers_.size(); i++)
    {
      if (workers_[i].joinable())
      {
        workers_[i].join();
      }
    }
      
    {
      boost::mutex::scoped_lock lock(stateMutex_);
      state_ = State_Done;
    }

    LOG(WARNING) << "The jobs engine has stopped";
  }
}
