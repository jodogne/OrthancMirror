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


#include "../PrecompiledHeadersServer.h"
#include "ServerScheduler.h"

#include "../../Core/OrthancException.h"
#include "../../Core/Logging.h"

namespace Orthanc
{
  namespace
  {
    // Anonymous namespace to avoid clashes between compilation modules
    class Sink : public IServerCommand
    {
    private:
      ListOfStrings& target_;

    public:
      explicit Sink(ListOfStrings& target) : target_(target)
      {
      }

      virtual bool Apply(ListOfStrings& outputs,
                         const ListOfStrings& inputs)
      {
        for (ListOfStrings::const_iterator 
               it = inputs.begin(); it != inputs.end(); ++it)
        {
          target_.push_back(*it);
        }

        return true;
      }    
    };
  }


  ServerScheduler::JobInfo& ServerScheduler::GetJobInfo(const std::string& jobId)
  {
    Jobs::iterator info = jobs_.find(jobId);

    if (info == jobs_.end())
    {
      throw OrthancException(ErrorCode_InternalError);
    }

    return info->second;
  }


  void ServerScheduler::SignalSuccess(const std::string& jobId)
  {
    boost::mutex::scoped_lock lock(mutex_);

    JobInfo& info = GetJobInfo(jobId);
    info.success_++;

    assert(info.failures_ == 0);

    if (info.success_ >= info.size_)
    {
      if (info.watched_)
      {
        watchedJobStatus_[jobId] = JobStatus_Success;
        watchedJobFinished_.notify_all();
      }

      LOG(INFO) << "Job successfully finished (" << info.description_ << ")";
      jobs_.erase(jobId);

      availableJob_.Release();
    }
  }


  void ServerScheduler::SignalFailure(const std::string& jobId)
  {
    boost::mutex::scoped_lock lock(mutex_);

    JobInfo& info = GetJobInfo(jobId);
    info.failures_++;

    if (info.success_ + info.failures_ >= info.size_)
    {
      if (info.watched_)
      {
        watchedJobStatus_[jobId] = JobStatus_Failure;
        watchedJobFinished_.notify_all();
      }

      LOG(ERROR) << "Job has failed (" << info.description_ << ")";
      jobs_.erase(jobId);

      availableJob_.Release();
    }
  }


  void ServerScheduler::Worker(ServerScheduler* that)
  {
    static const int32_t TIMEOUT = 100;

    LOG(WARNING) << "The server scheduler has started";

    while (!that->finish_)
    {
      std::auto_ptr<IDynamicObject> object(that->queue_.Dequeue(TIMEOUT));
      if (object.get() != NULL)
      {
        ServerCommandInstance& filter = dynamic_cast<ServerCommandInstance&>(*object);

        // Skip the execution of this filter if its parent job has
        // previously failed.
        bool jobHasFailed;
        {
          boost::mutex::scoped_lock lock(that->mutex_);
          JobInfo& info = that->GetJobInfo(filter.GetJobId());
          jobHasFailed = (info.failures_ > 0 || info.cancel_); 
        }

        if (jobHasFailed)
        {
          that->SignalFailure(filter.GetJobId());
        }
        else
        {
          filter.Execute(*that);
        }
      }
    }
  }


  void ServerScheduler::SubmitInternal(ServerJob& job,
                                       bool watched)
  {
    availableJob_.Acquire();

    boost::mutex::scoped_lock lock(mutex_);

    JobInfo info;
    info.size_ = job.Submit(queue_, *this);
    info.cancel_ = false;
    info.success_ = 0;
    info.failures_ = 0;
    info.description_ = job.GetDescription();
    info.watched_ = watched;

    assert(info.size_ > 0);

    if (watched)
    {
      watchedJobStatus_[job.GetId()] = JobStatus_Running;
    }

    jobs_[job.GetId()] = info;

    LOG(INFO) << "New job submitted (" << job.description_ << ")";
  }


  ServerScheduler::ServerScheduler(unsigned int maxJobs) : availableJob_(maxJobs)
  {
    if (maxJobs == 0)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    finish_ = false;
    worker_ = boost::thread(Worker, this);
  }


  ServerScheduler::~ServerScheduler()
  {
    if (!finish_)
    {
      LOG(ERROR) << "INTERNAL ERROR: ServerScheduler::Finalize() should be invoked manually to avoid mess in the destruction order!";
      Stop();
    }
  }


  void ServerScheduler::Stop()
  {
    if (!finish_)
    {
      finish_ = true;

      if (worker_.joinable())
      {
        worker_.join();
      }
    }
  }


  void ServerScheduler::Submit(ServerJob& job)
  {
    if (job.filters_.empty())
    {
      return;
    }

    SubmitInternal(job, false);
  }


  bool ServerScheduler::SubmitAndWait(ListOfStrings& outputs,
                                      ServerJob& job)
  {
    std::string jobId = job.GetId();

    outputs.clear();

    if (job.filters_.empty())
    {
      return true;
    }

    // Add a sink filter to collect all the results of the filters
    // that have no next filter.
    ServerCommandInstance& sink = job.AddCommand(new Sink(outputs));

    for (std::list<ServerCommandInstance*>::iterator
           it = job.filters_.begin(); it != job.filters_.end(); ++it)
    {
      if ((*it) != &sink &&
          (*it)->IsConnectedToSink())
      {
        (*it)->ConnectOutput(sink);
      }
    }

    // Submit the job
    SubmitInternal(job, true);

    // Wait for the job to complete (either success or failure)
    JobStatus status;

    {
      boost::mutex::scoped_lock lock(mutex_);

      assert(watchedJobStatus_.find(jobId) != watchedJobStatus_.end());
        
      while (watchedJobStatus_[jobId] == JobStatus_Running)
      {
        watchedJobFinished_.wait(lock);
      }

      status = watchedJobStatus_[jobId];
      watchedJobStatus_.erase(jobId);
    }

    return (status == JobStatus_Success);
  }


  bool ServerScheduler::SubmitAndWait(ServerJob& job)
  {
    ListOfStrings ignoredSink;
    return SubmitAndWait(ignoredSink, job);
  }


  bool ServerScheduler::IsRunning(const std::string& jobId)
  {
    boost::mutex::scoped_lock lock(mutex_);
    return jobs_.find(jobId) != jobs_.end();
  }


  void ServerScheduler::Cancel(const std::string& jobId)
  {
    boost::mutex::scoped_lock lock(mutex_);

    Jobs::iterator job = jobs_.find(jobId);

    if (job != jobs_.end())
    {
      job->second.cancel_ = true;
      LOG(WARNING) << "Canceling a job (" << job->second.description_ << ")";
    }
  }


  float ServerScheduler::GetProgress(const std::string& jobId) 
  {
    boost::mutex::scoped_lock lock(mutex_);

    Jobs::iterator job = jobs_.find(jobId);

    if (job == jobs_.end() || 
        job->second.size_ == 0  /* should never happen */)
    {
      // This job is not running
      return 1;
    }

    if (job->second.failures_ != 0)
    {
      return 1;
    }

    if (job->second.size_ == 1)
    {
      return static_cast<float>(job->second.success_);
    }

    return (static_cast<float>(job->second.success_) / 
            static_cast<float>(job->second.size_ - 1));
  }


  void ServerScheduler::GetListOfJobs(ListOfStrings& jobs)
  {
    boost::mutex::scoped_lock lock(mutex_);

    jobs.clear();

    for (Jobs::const_iterator 
           it = jobs_.begin(); it != jobs_.end(); ++it)
    {
      jobs.push_back(it->first);
    }
  }
}
