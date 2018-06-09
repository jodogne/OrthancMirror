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
#include "../SerializationToolbox.h"

namespace Orthanc
{
  static const char* STATE = "State";
  static const char* TYPE = "Type";
  static const char* PRIORITY = "Priority";
  static const char* JOB = "Job";
  static const char* JOBS = "Jobs";
  static const char* JOBS_REGISTRY = "JobsRegistry";
  static const char* MAX_COMPLETED_JOBS = "MaxCompletedJobs";
  static const char* CREATION_TIME = "CreationTime";
  static const char* LAST_CHANGE_TIME = "LastChangeTime";
  static const char* RUNTIME = "Runtime";
  

  class JobsRegistry::JobHandler : public boost::noncopyable
  {   
  private:
    std::string                       id_;
    JobState                          state_;
    std::string                       jobType_;
    std::auto_ptr<IJob>               job_;
    int                               priority_;  // "+inf()" means highest priority
    boost::posix_time::ptime          creationTime_;
    boost::posix_time::ptime          lastStateChangeTime_;
    boost::posix_time::time_duration  runtime_;
    boost::posix_time::ptime          retryTime_;
    bool                              pauseScheduled_;
    bool                              cancelScheduled_;
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
      cancelScheduled_ = false;
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
      pauseScheduled_(false),
      cancelScheduled_(false)
    {
      if (job == NULL)
      {
        throw OrthancException(ErrorCode_NullPointer);
      }

      job->GetJobType(jobType_);
      job->Start();

      lastStatus_ = JobStatus(ErrorCode_Success, *job_);
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

    void ScheduleCancel()
    {
      if (state_ == JobState_Running)
      {
        cancelScheduled_ = true;
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

    bool IsCancelScheduled()
    {
      return cancelScheduled_;
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

    void SetLastStateChangeTime(const boost::posix_time::ptime& time)
    {
      lastStateChangeTime_ = time;
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

    void SetLastErrorCode(ErrorCode code)
    {
      lastStatus_.SetErrorCode(code);
    }

    bool Serialize(Json::Value& target) const
    {
      target = Json::objectValue;

      bool ok;

      if (state_ == JobState_Running)
      {
        // WARNING: Cannot directly access the "job_" member, as long
        // as a "RunningJob" instance is running. We do not use a
        // mutex at the "JobHandler" level, as serialization would be
        // blocked while a step in the job is running. Instead, we
        // save a snapshot of the serialized job.

        if (lastStatus_.HasSerialized())
        {
          target[JOB] = lastStatus_.GetSerialized();
          ok = true;
        }
        else
        {
          ok = false;
        }
      }
      else 
      {
        ok = job_->Serialize(target[JOB]);
      }

      if (ok)
      {
        target[STATE] = EnumerationToString(state_);
        target[PRIORITY] = priority_;
        target[CREATION_TIME] = boost::posix_time::to_iso_string(creationTime_);
        target[LAST_CHANGE_TIME] = boost::posix_time::to_iso_string(lastStateChangeTime_);
        target[RUNTIME] = static_cast<unsigned int>(runtime_.total_milliseconds());
        return true;
      }
      else
      {
        LOG(WARNING) << "Job backup is not supported for job of type: " << jobType_;
        return false;
      }
    }

    JobHandler(IJobUnserializer& unserializer,
               const Json::Value& serialized,
               const std::string& id) :
      id_(id),
      pauseScheduled_(false),
      cancelScheduled_(false)
    {
      state_ = StringToJobState(SerializationToolbox::ReadString(serialized, STATE));
      priority_ = SerializationToolbox::ReadInteger(serialized, PRIORITY);
      creationTime_ = boost::posix_time::from_iso_string
        (SerializationToolbox::ReadString(serialized, CREATION_TIME));
      lastStateChangeTime_ = boost::posix_time::from_iso_string
        (SerializationToolbox::ReadString(serialized, LAST_CHANGE_TIME));
      runtime_ = boost::posix_time::milliseconds
        (SerializationToolbox::ReadInteger(serialized, RUNTIME));

      retryTime_ = creationTime_;

      job_.reset(unserializer.UnserializeJob(serialized[JOB]));
      job_->GetJobType(jobType_);
      job_->Start();

      lastStatus_ = JobStatus(ErrorCode_Success, *job_);
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


  void JobsRegistry::SetCompletedJob(JobHandler& job,
                                     bool success)
  {
    job.SetState(success ? JobState_Success : JobState_Failure);

    completedJobs_.push_back(&job);
    ForgetOldCompletedJobs();

    someJobComplete_.notify_all();
  }


  void JobsRegistry::MarkRunningAsCompleted(JobHandler& job,
                                            bool success)
  {
    LOG(INFO) << "Job has completed with " << (success ? "success" : "failure")
              << ": " << job.GetId();

    CheckInvariants();

    assert(job.GetState() == JobState_Running);
    SetCompletedJob(job, success);

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


  void JobsRegistry::SetMaxCompletedJobs(size_t n)
  {
    boost::mutex::scoped_lock lock(mutex_);
    CheckInvariants();

    LOG(INFO) << "The size of the history of the jobs engine is set to: " << n << " job(s)";

    maxCompletedJobs_ = n;
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


  void JobsRegistry::SubmitInternal(std::string& id,
                                    JobHandler* handlerRaw,
                                    bool keepLastChangeTime)
  {
    if (handlerRaw == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
    
    std::auto_ptr<JobHandler>  handler(handlerRaw);

    boost::posix_time::ptime lastChangeTime = handler->GetLastStateChangeTime();

    boost::mutex::scoped_lock lock(mutex_);
    CheckInvariants();
      
    id = handler->GetId();
    int priority = handler->GetPriority();

    switch (handler->GetState())
    {
      case JobState_Pending:
      case JobState_Retry:
      case JobState_Running:
        handler->SetState(JobState_Pending);
        pendingJobs_.push(handler.get());
        pendingJobAvailable_.notify_one();
        break;
 
      case JobState_Success:
        SetCompletedJob(*handler, true);
        break;
        
      case JobState_Failure:
        SetCompletedJob(*handler, false);
        break;

      case JobState_Paused:
        break;
        
      default:
        LOG(ERROR) << "A job should not be loaded from state: "
                   << EnumerationToString(handler->GetState());
        throw OrthancException(ErrorCode_InternalError);
    }

    if (keepLastChangeTime)
    {
      handler->SetLastStateChangeTime(lastChangeTime);
    }
    
    jobsIndex_.insert(std::make_pair(id, handler.release()));

    LOG(INFO) << "New job submitted with priority " << priority << ": " << id;

    CheckInvariants();
  }


  void JobsRegistry::Submit(std::string& id,
                            IJob* job,        // Takes ownership
                            int priority)
  {
    SubmitInternal(id, new JobHandler(job, priority), false);
  }


  void JobsRegistry::Submit(IJob* job,        // Takes ownership
                            int priority)
  {
    std::string id;
    SubmitInternal(id, new JobHandler(job, priority), false);
  }


  bool JobsRegistry::SubmitAndWait(IJob* job,        // Takes ownership
                                   int priority)
  {
    std::string id;
    Submit(id, job, priority);

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


  bool JobsRegistry::SetPriority(const std::string& id,
                                 int priority)
  {
    LOG(INFO) << "Changing priority to " << priority << " for job: " << id;

    boost::mutex::scoped_lock lock(mutex_);
    CheckInvariants();

    JobsIndex::iterator found = jobsIndex_.find(id);

    if (found == jobsIndex_.end())
    {
      LOG(WARNING) << "Unknown job: " << id;
      return false;
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

      CheckInvariants();
      return true;
    }
  }


  void JobsRegistry::RemovePendingJob(const std::string& id)
  {
    // If the job is pending, we need to reconstruct the priority
    // queue to remove it
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
  }


  void JobsRegistry::RemoveRetryJob(JobHandler* handler)
  {
    RetryJobs::iterator item = retryJobs_.find(handler);
    assert(item != retryJobs_.end());            
    retryJobs_.erase(item);
  }


  bool JobsRegistry::Pause(const std::string& id)
  {
    LOG(INFO) << "Pausing job: " << id;

    boost::mutex::scoped_lock lock(mutex_);
    CheckInvariants();

    JobsIndex::iterator found = jobsIndex_.find(id);

    if (found == jobsIndex_.end())
    {
      LOG(WARNING) << "Unknown job: " << id;
      return false;
    }
    else
    {
      switch (found->second->GetState())
      {
        case JobState_Pending:
          RemovePendingJob(id);
          found->second->SetState(JobState_Paused);
          break;

        case JobState_Retry:
          RemoveRetryJob(found->second);
          found->second->SetState(JobState_Paused);
          break;

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

      CheckInvariants();
      return true;
    }
  }


  bool JobsRegistry::Cancel(const std::string& id)
  {
    LOG(INFO) << "Canceling job: " << id;

    boost::mutex::scoped_lock lock(mutex_);
    CheckInvariants();

    JobsIndex::iterator found = jobsIndex_.find(id);

    if (found == jobsIndex_.end())
    {
      LOG(WARNING) << "Unknown job: " << id;
      return false;
    }
    else
    {
      switch (found->second->GetState())
      {
        case JobState_Pending:
          RemovePendingJob(id);
          SetCompletedJob(*found->second, false);
          found->second->SetLastErrorCode(ErrorCode_CanceledJob);
          break;

        case JobState_Retry:
          RemoveRetryJob(found->second);
          SetCompletedJob(*found->second, false);
          found->second->SetLastErrorCode(ErrorCode_CanceledJob);
          break;

        case JobState_Paused:
          SetCompletedJob(*found->second, false);
          found->second->SetLastErrorCode(ErrorCode_CanceledJob);
          break;
        
        case JobState_Success:
        case JobState_Failure:
          // Nothing to be done
          break;

        case JobState_Running:
          found->second->ScheduleCancel();
          break;

        default:
          throw OrthancException(ErrorCode_InternalError);
      }

      CheckInvariants();
      return true;
    }
  }


  bool JobsRegistry::Resume(const std::string& id)
  {
    LOG(INFO) << "Resuming job: " << id;

    boost::mutex::scoped_lock lock(mutex_);
    CheckInvariants();

    JobsIndex::iterator found = jobsIndex_.find(id);

    if (found == jobsIndex_.end())
    {
      LOG(WARNING) << "Unknown job: " << id;
      return false;
    }
    else if (found->second->GetState() != JobState_Paused)
    {
      LOG(WARNING) << "Cannot resume a job that is not paused: " << id;
      return false;
    }
    else
    {
      found->second->SetState(JobState_Pending);
      pendingJobs_.push(found->second);
      pendingJobAvailable_.notify_one();
      CheckInvariants();
      return true;      
    }
  }


  bool JobsRegistry::Resubmit(const std::string& id)
  {
    LOG(INFO) << "Resubmitting failed job: " << id;

    boost::mutex::scoped_lock lock(mutex_);
    CheckInvariants();

    JobsIndex::iterator found = jobsIndex_.find(id);

    if (found == jobsIndex_.end())
    {
      LOG(WARNING) << "Unknown job: " << id;
      return false;
    }
    else if (found->second->GetState() != JobState_Failure)
    {
      LOG(WARNING) << "Cannot resubmit a job that has not failed: " << id;
      return false;
    }
    else
    {
      found->second->GetJob().SignalResubmit();
      
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

      CheckInvariants();
      return true;
    }
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
    targetRetryTimeout_(0),
    canceled_(false)
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
      handler_->SetLastErrorCode(ErrorCode_Success);

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

          if (canceled_)
          {
            handler_->SetLastErrorCode(ErrorCode_CanceledJob);
          }
          
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

      
  bool JobsRegistry::RunningJob::IsCancelScheduled()
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
        
      return handler_->IsCancelScheduled();
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

      
  void JobsRegistry::RunningJob::MarkCanceled()
  {
    if (!IsValid())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      targetState_ = JobState_Failure;
      canceled_ = true;
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



  void JobsRegistry::Serialize(Json::Value& target)
  {
    boost::mutex::scoped_lock lock(mutex_);
    CheckInvariants();

    target = Json::objectValue;
    target[TYPE] = JOBS_REGISTRY;
    target[MAX_COMPLETED_JOBS] = static_cast<unsigned int>(maxCompletedJobs_);
    target[JOBS] = Json::objectValue;
    
    for (JobsIndex::const_iterator it = jobsIndex_.begin(); 
         it != jobsIndex_.end(); ++it)
    {
      Json::Value v;
      if (it->second->Serialize(v))
      {
        target[JOBS][it->first] = v;
      }
    }
  }


  JobsRegistry::JobsRegistry(IJobUnserializer& unserializer,
                             const Json::Value& s)
  {
    if (SerializationToolbox::ReadString(s, TYPE) != JOBS_REGISTRY ||
        !s.isMember(JOBS) ||
        s[JOBS].type() != Json::objectValue)
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    maxCompletedJobs_ = SerializationToolbox::ReadUnsignedInteger(s, MAX_COMPLETED_JOBS);

    Json::Value::Members members = s[JOBS].getMemberNames();

    for (Json::Value::Members::const_iterator it = members.begin();
         it != members.end(); ++it)
    {
      std::auto_ptr<JobHandler> job(new JobHandler(unserializer, s[JOBS][*it], *it));
      
      std::string id;
      SubmitInternal(id, job.release(), true);
    }
  }
}
