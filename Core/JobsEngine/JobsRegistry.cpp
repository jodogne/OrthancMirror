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
#include "JobsRegistry.h"

#include "../Logging.h"
#include "../OrthancException.h"
#include "../Toolbox.h"

namespace Orthanc
{
  class JobsRegistry::JobHandler : public boost::noncopyable
  {   
  private:
    std::string                       id_;
    JobState                          state_;
    std::auto_ptr<IJob>               job_;
    int                               priority_;  // "+inf()" means highest priority
    boost::posix_time::ptime          creationTime_;
    boost::posix_time::ptime          lastStateChangeTime_;
    boost::posix_time::time_duration  runtime_;
    boost::posix_time::ptime          retryTime_;
    bool                              pauseScheduled_;
    JobStatus                         lastStatus_;

    void Touch()
    {
      const boost::posix_time::ptime now = boost::posix_time::microsec_clock::universal_time();

      if (state_ == JobState_Running)
      {
        runtime_ += (now - lastStateChangeTime_);
      }

      lastStateChangeTime_ = now;
    }

    void SetStateInternal(JobState state) 
    {
      state_ = state;
      pauseScheduled_ = false;
      Touch();
    }

  public:
    JobHandler(IJob* job,
               int priority) :
      id_(Toolbox::GenerateUuid()),
      state_(JobState_Pending),
      job_(job),
      priority_(priority),
      creationTime_(boost::posix_time::microsec_clock::universal_time()),
      lastStateChangeTime_(creationTime_),
      runtime_(boost::posix_time::milliseconds(0)),
      retryTime_(creationTime_),
      pauseScheduled_(false)
    {
      if (job == NULL)
      {
        throw OrthancException(ErrorCode_NullPointer);
      }

      lastStatus_ = JobStatus(ErrorCode_Success, *job);
      job->Start();
    }

    const std::string& GetId() const
    {
      return id_;
    }

    IJob& GetJob() const
    {
      assert(job_.get() != NULL);
      return *job_;
    }

    void SetPriority(int priority)
    {
      priority_ = priority;
    }

    int GetPriority() const
    {
      return priority_;
    }

    JobState GetState() const
    {
      return state_;
    }

    void SetState(JobState state) 
    {
      if (state == JobState_Retry)
      {
        // Use "SetRetryState()"
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }
      else
      {
        SetStateInternal(state);
      }
    }

    void SetRetryState(unsigned int timeout)
    {
      if (state_ == JobState_Running)
      {
        SetStateInternal(JobState_Retry);
        retryTime_ = (boost::posix_time::microsec_clock::universal_time() + 
                      boost::posix_time::milliseconds(timeout));
      }
      else
      {
        // Only valid for running jobs
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }
    }

    void SchedulePause()
    {
      if (state_ == JobState_Running)
      {
        pauseScheduled_ = true;
      }
      else
      {
        // Only valid for running jobs
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }
    }

    bool IsPauseScheduled()
    {
      return pauseScheduled_;
    }

    bool IsRetryReady(const boost::posix_time::ptime& now) const
    {
      if (state_ != JobState_Retry)
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }
      else
      {
        return retryTime_ <= now;
      }
    }

    const boost::posix_time::ptime& GetCreationTime() const
    {
      return creationTime_;
    }

    const boost::posix_time::ptime& GetLastStateChangeTime() const
    {
      return lastStateChangeTime_;
    }

    const boost::posix_time::time_duration& GetRuntime() const
    {
      return runtime_;
    }

    const JobStatus& GetLastStatus() const
    {
      return lastStatus_;
    }

    void SetLastStatus(const JobStatus& status)
    {
      lastStatus_ = status;
      Touch();
    }
  };


  bool JobsRegistry::PriorityComparator::operator() (JobHandler*& a,
                                                     JobHandler*& b) const
  {
    return a->GetPriority() < b->GetPriority();
  }                       


#if defined(NDEBUG)
  void JobsRegistry::CheckInvariants() const
  {
  }
  
#else
  bool JobsRegistry::IsPendingJob(const JobHandler& job) const
  {
    PendingJobs copy = pendingJobs_;
    while (!copy.empty())
    {
      if (copy.top() == &job)
      {
        return true;
      }

      copy.pop();
    }

    return false;
  }

  bool JobsRegistry::IsCompletedJob(JobHandler& job) const
  {
    for (CompletedJobs::const_iterator it = completedJobs_.begin();
         it != completedJobs_.end(); ++it)
    {
      if (*it == &job)
      {
        return true;
      }
    }

    return false;
  }

  bool JobsRegistry::IsRetryJob(JobHandler& job) const
  {
    return retryJobs_.find(&job) != retryJobs_.end();
  }

  void JobsRegistry::CheckInvariants() const
  {
    {
      PendingJobs copy = pendingJobs_;
      while (!copy.empty())
      {
        assert(copy.top()->GetState() == JobState_Pending);
        copy.pop();
      }
    }

    assert(completedJobs_.size() <= maxCompletedJobs_);

    for (CompletedJobs::const_iterator it = completedJobs_.begin();
         it != completedJobs_.end(); ++it)
    {
      assert((*it)->GetState() == JobState_Success ||
             (*it)->GetState() == JobState_Failure);
    }

    for (RetryJobs::const_iterator it = retryJobs_.begin();
         it != retryJobs_.end(); ++it)
    {
      assert((*it)->GetState() == JobState_Retry);
    }

    for (JobsIndex::const_iterator it = jobsIndex_.begin();
         it != jobsIndex_.end(); ++it)
    {
      JobHandler& job = *it->second;

      assert(job.GetId() == it->first);

      switch (job.GetState())
      {
        case JobState_Pending:
          assert(!IsRetryJob(job) && IsPendingJob(job) && !IsCompletedJob(job));
          break;
            
        case JobState_Success:
        case JobState_Failure:
          assert(!IsRetryJob(job) && !IsPendingJob(job) && IsCompletedJob(job));
          break;
            
        case JobState_Retry:
          assert(IsRetryJob(job) && !IsPendingJob(job) && !IsCompletedJob(job));
          break;
            
        case JobState_Running:
        case JobState_Paused:
          assert(!IsRetryJob(job) && !IsPendingJob(job) && !IsCompletedJob(job));
          break;

        default:
          throw OrthancException(ErrorCode_InternalError);
      }
    }
  }
#endif


  void JobsRegistry::ForgetOldCompletedJobs()
  {
    if (maxCompletedJobs_ != 0)
    {
      while (completedJobs_.size() > maxCompletedJobs_)
      {
        assert(completedJobs_.front() != NULL);

        std::string id = completedJobs_.front()->GetId();
        assert(jobsIndex_.find(id) != jobsIndex_.end());

        jobsIndex_.erase(id);
        delete(completedJobs_.front());
        completedJobs_.pop_front();
      }
    }
  }


  void JobsRegistry::MarkRunningAsCompleted(JobHandler& job,
                                            bool success)
  {
    LOG(INFO) << "Job has completed with " << (success ? "success" : "failure")
              << ": " << job.GetId();

    CheckInvariants();
    assert(job.GetState() == JobState_Running);

    job.SetState(success ? JobState_Success : JobState_Failure);

    completedJobs_.push_back(&job);
    ForgetOldCompletedJobs();

    someJobComplete_.notify_all();

    CheckInvariants();
  }


  void JobsRegistry::MarkRunningAsRetry(JobHandler& job,
                                        unsigned int timeout)
  {
    LOG(INFO) << "Job scheduled for retry in " << timeout << "ms: " << job.GetId();

    CheckInvariants();

    assert(job.GetState() == JobState_Running &&
           retryJobs_.find(&job) == retryJobs_.end());

    retryJobs_.insert(&job);
    job.SetRetryState(timeout);

    CheckInvariants();
  }


  void JobsRegistry::MarkRunningAsPaused(JobHandler& job)
  {
    LOG(INFO) << "Job paused: " << job.GetId();

    CheckInvariants();
    assert(job.GetState() == JobState_Running);

    job.SetState(JobState_Paused);

    CheckInvariants();
  }


  bool JobsRegistry::GetStateInternal(JobState& state,
                                      const std::string& id)
  {
    CheckInvariants();

    JobsIndex::const_iterator it = jobsIndex_.find(id);
    if (it == jobsIndex_.end())
    {
      return false;
    }
    else
    {
      state = it->second->GetState();
      return true;
    }
  }

  
  JobsRegistry::~JobsRegistry()
  {
    for (JobsIndex::iterator it = jobsIndex_.begin(); it != jobsIndex_.end(); ++it)
    {
      assert(it->second != NULL);
      delete it->second;
    }
  }


  void JobsRegistry::SetMaxCompletedJobs(size_t i)
  {
    boost::mutex::scoped_lock lock(mutex_);
    CheckInvariants();

    maxCompletedJobs_ = i;
    ForgetOldCompletedJobs();

    CheckInvariants();
  }


  void JobsRegistry::ListJobs(std::set<std::string>& target)
  {
    boost::mutex::scoped_lock lock(mutex_);
    CheckInvariants();

    for (JobsIndex::const_iterator it = jobsIndex_.begin();
         it != jobsIndex_.end(); ++it)
    {
      target.insert(it->first);
    }
  }


  bool JobsRegistry::GetJobInfo(JobInfo& target,
                                const std::string& id)
  {
    boost::mutex::scoped_lock lock(mutex_);
    CheckInvariants();

    JobsIndex::const_iterator found = jobsIndex_.find(id);

    if (found == jobsIndex_.end())
    {
      return false;
    }
    else
    {
      const JobHandler& handler = *found->second;
      target = JobInfo(handler.GetId(),
                       handler.GetPriority(),
                       handler.GetState(),
                       handler.GetLastStatus(),
                       handler.GetCreationTime(),
                       handler.GetLastStateChangeTime(),
                       handler.GetRuntime());
      return true;
    }
  }


  void JobsRegistry::Submit(std::string& id,
                            IJob* job,        // Takes ownership
                            int priority)
  {
    std::auto_ptr<JobHandler>  handler(new JobHandler(job, priority));

    boost::mutex::scoped_lock lock(mutex_);
    CheckInvariants();
      
    id = handler->GetId();

    pendingJobs_.push(handler.get());
    pendingJobAvailable_.notify_one();

    jobsIndex_.insert(std::make_pair(id, handler.release()));

    LOG(INFO) << "New job submitted with priority " << priority << ": " << id;

    CheckInvariants();
  }


  void JobsRegistry::Submit(IJob* job,        // Takes ownership
                            int priority)
  {
    std::string id;
    Submit(id, job, priority);
  }


  bool JobsRegistry::SubmitAndWait(IJob* job,        // Takes ownership
                                   int priority)
  {
    std::string id;
    Submit(id, job, priority);

    printf(">> %s\n", id.c_str()); fflush(stdout);

    JobState state;

    {
      boost::mutex::scoped_lock lock(mutex_);

      while (GetStateInternal(state, id) &&
             state != JobState_Success &&
             state != JobState_Failure)
      {
        someJobComplete_.wait(lock);
      }
    }

    return (state == JobState_Success);
  }


  void JobsRegistry::SetPriority(const std::string& id,
                                 int priority)
  {
    LOG(INFO) << "Changing priority to " << priority << " for job: " << id;

    boost::mutex::scoped_lock lock(mutex_);
    CheckInvariants();

    JobsIndex::iterator found = jobsIndex_.find(id);

    if (found == jobsIndex_.end())
    {
      LOG(WARNING) << "Unknown job: " << id;
    }
    else
    {
      found->second->SetPriority(priority);

      if (found->second->GetState() == JobState_Pending)
      {
        // If the job is pending, we need to reconstruct the
        // priority queue, as the heap condition has changed

        PendingJobs copy;
        std::swap(copy, pendingJobs_);

        assert(pendingJobs_.empty());
        while (!copy.empty())
        {
          pendingJobs_.push(copy.top());
          copy.pop();
        }
      }
    }

    CheckInvariants();
  }


  void JobsRegistry::Pause(const std::string& id)
  {
    LOG(INFO) << "Pausing job: " << id;

    boost::mutex::scoped_lock lock(mutex_);
    CheckInvariants();

    JobsIndex::iterator found = jobsIndex_.find(id);

    if (found == jobsIndex_.end())
    {
      LOG(WARNING) << "Unknown job: " << id;
    }
    else
    {
      switch (found->second->GetState())
      {
        case JobState_Pending:
        {
          // If the job is pending, we need to reconstruct the
          // priority queue to remove it
          PendingJobs copy;
          std::swap(copy, pendingJobs_);

          assert(pendingJobs_.empty());
          while (!copy.empty())
          {
            if (copy.top()->GetId() != id)
            {
              pendingJobs_.push(copy.top());
            }

            copy.pop();
          }

          found->second->SetState(JobState_Paused);

          break;
        }

        case JobState_Retry:
        {
          RetryJobs::iterator item = retryJobs_.find(found->second);
          assert(item != retryJobs_.end());            
          retryJobs_.erase(item);

          found->second->SetState(JobState_Paused);

          break;
        }

        case JobState_Paused:
        case JobState_Success:
        case JobState_Failure:
          // Nothing to be done
          break;

        case JobState_Running:
          found->second->SchedulePause();
          break;

        default:
          throw OrthancException(ErrorCode_InternalError);
      }
    }

    CheckInvariants();
  }


  void JobsRegistry::Resume(const std::string& id)
  {
    LOG(INFO) << "Resuming job: " << id;

    boost::mutex::scoped_lock lock(mutex_);
    CheckInvariants();

    JobsIndex::iterator found = jobsIndex_.find(id);

    if (found == jobsIndex_.end())
    {
      LOG(WARNING) << "Unknown job: " << id;
    }
    else if (found->second->GetState() != JobState_Paused)
    {
      LOG(WARNING) << "Cannot resume a job that is not paused: " << id;
    }
    else
    {
      found->second->SetState(JobState_Pending);
      pendingJobs_.push(found->second);
      pendingJobAvailable_.notify_one();
    }

    CheckInvariants();
  }


  void JobsRegistry::Resubmit(const std::string& id)
  {
    LOG(INFO) << "Resubmitting failed job: " << id;

    boost::mutex::scoped_lock lock(mutex_);
    CheckInvariants();

    JobsIndex::iterator found = jobsIndex_.find(id);

    if (found == jobsIndex_.end())
    {
      LOG(WARNING) << "Unknown job: " << id;
    }
    else if (found->second->GetState() != JobState_Failure)
    {
      LOG(WARNING) << "Cannot resubmit a job that has not failed: " << id;
    }
    else
    {
      bool ok = false;
      for (CompletedJobs::iterator it = completedJobs_.begin(); 
           it != completedJobs_.end(); ++it)
      {
        if (*it == found->second)
        {
          ok = true;
          completedJobs_.erase(it);
          break;
        }
      }

      assert(ok);

      found->second->SetState(JobState_Pending);
      pendingJobs_.push(found->second);
      pendingJobAvailable_.notify_one();
    }

    CheckInvariants();
  }


  void JobsRegistry::ScheduleRetries()
  {
    boost::mutex::scoped_lock lock(mutex_);
    CheckInvariants();

    RetryJobs copy;
    std::swap(copy, retryJobs_);

    const boost::posix_time::ptime now = boost::posix_time::microsec_clock::universal_time();

    assert(retryJobs_.empty());
    for (RetryJobs::iterator it = copy.begin(); it != copy.end(); ++it)
    {
      if ((*it)->IsRetryReady(now))
      {
        LOG(INFO) << "Retrying job: " << (*it)->GetId();
        (*it)->SetState(JobState_Pending);
        pendingJobs_.push(*it);
        pendingJobAvailable_.notify_one();
      }
      else
      {
        retryJobs_.insert(*it);
      }
    }

    CheckInvariants();
  }


  bool JobsRegistry::GetState(JobState& state,
                              const std::string& id)
  {
    boost::mutex::scoped_lock lock(mutex_);
    return GetStateInternal(state, id);
  }

  
  JobsRegistry::RunningJob::RunningJob(JobsRegistry& registry,
                                       unsigned int timeout) :
    registry_(registry),
    handler_(NULL),
    targetState_(JobState_Failure),
    targetRetryTimeout_(0)
  {
    {
      boost::mutex::scoped_lock lock(registry_.mutex_);

      while (registry_.pendingJobs_.empty())
      {
        if (timeout == 0)
        {
          registry_.pendingJobAvailable_.wait(lock);
        }
        else
        {
          bool success = registry_.pendingJobAvailable_.timed_wait
            (lock, boost::posix_time::milliseconds(timeout));
          if (!success)
          {
            // No pending job
            return;
          }
        }
      }

      handler_ = registry_.pendingJobs_.top();
      registry_.pendingJobs_.pop();

      assert(handler_->GetState() == JobState_Pending);
      handler_->SetState(JobState_Running);

      job_ = &handler_->GetJob();
      id_ = handler_->GetId();
      priority_ = handler_->GetPriority();
    }
  }

      
  JobsRegistry::RunningJob::~RunningJob()
  {
    if (IsValid())
    {
      boost::mutex::scoped_lock lock(registry_.mutex_);

      switch (targetState_)
      {
        case JobState_Failure:
          registry_.MarkRunningAsCompleted(*handler_, false);
          break;

        case JobState_Success:
          registry_.MarkRunningAsCompleted(*handler_, true);
          break;

        case JobState_Paused:
          registry_.MarkRunningAsPaused(*handler_);
          break;            

        case JobState_Retry:
          registry_.MarkRunningAsRetry(*handler_, targetRetryTimeout_);
          break;
            
        default:
          assert(0);
      }
    }
  }

      
  bool JobsRegistry::RunningJob::IsValid() const
  {
    return (handler_ != NULL &&
            job_ != NULL);
  }

      
  const std::string& JobsRegistry::RunningJob::GetId() const
  {
    if (!IsValid())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      return id_;
    }
  }

      
  int JobsRegistry::RunningJob::GetPriority() const
  {
    if (!IsValid())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      return priority_;
    }
  }
      

  IJob& JobsRegistry::RunningJob::GetJob()
  {
    if (!IsValid())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      return *job_;
    }
  }

      
  bool JobsRegistry::RunningJob::IsPauseScheduled()
  {
    if (!IsValid())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      boost::mutex::scoped_lock lock(registry_.mutex_);
      registry_.CheckInvariants();
      assert(handler_->GetState() == JobState_Running);
        
      return handler_->IsPauseScheduled();
    }
  }

      
  void JobsRegistry::RunningJob::MarkSuccess()
  {
    if (!IsValid())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      targetState_ = JobState_Success;
    }
  }

      
  void JobsRegistry::RunningJob::MarkFailure()
  {
    if (!IsValid())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      targetState_ = JobState_Failure;
    }
  }

      
  void JobsRegistry::RunningJob::MarkPause()
  {
    if (!IsValid())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      targetState_ = JobState_Paused;
    }
  }

      
  void JobsRegistry::RunningJob::MarkRetry(unsigned int timeout)
  {
    if (!IsValid())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      targetState_ = JobState_Retry;
      targetRetryTimeout_ = timeout;
    }
  }
      

  void JobsRegistry::RunningJob::UpdateStatus(ErrorCode code)
  {
    if (!IsValid())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      JobStatus status(code, *job_);
          
      boost::mutex::scoped_lock lock(registry_.mutex_);
      registry_.CheckInvariants();
      assert(handler_->GetState() == JobState_Running);
        
      handler_->SetLastStatus(status);
    }
  }
}
