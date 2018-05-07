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


#include "PrecompiledHeadersUnitTests.h"
#include "gtest/gtest.h"

#include "../OrthancServer/Scheduler/ServerScheduler.h"
#include "../Core/OrthancException.h"
#include "../Core/SystemToolbox.h"
#include "../Core/Toolbox.h"
#include "../Core/MultiThreading/Locker.h"

using namespace Orthanc;

namespace
{
  class DynamicInteger : public IDynamicObject
  {
  private:
    int value_;
    std::set<int>& target_;

  public:
    DynamicInteger(int value, std::set<int>& target) : 
      value_(value), target_(target)
    {
    }

    int GetValue() const
    {
      return value_;
    }
  };
}


TEST(MultiThreading, SharedMessageQueueBasic)
{
  std::set<int> s;

  SharedMessageQueue q;
  ASSERT_TRUE(q.WaitEmpty(0));
  q.Enqueue(new DynamicInteger(10, s));
  ASSERT_FALSE(q.WaitEmpty(1));
  q.Enqueue(new DynamicInteger(20, s));
  q.Enqueue(new DynamicInteger(30, s));
  q.Enqueue(new DynamicInteger(40, s));

  std::auto_ptr<DynamicInteger> i;
  i.reset(dynamic_cast<DynamicInteger*>(q.Dequeue(1))); ASSERT_EQ(10, i->GetValue());
  i.reset(dynamic_cast<DynamicInteger*>(q.Dequeue(1))); ASSERT_EQ(20, i->GetValue());
  i.reset(dynamic_cast<DynamicInteger*>(q.Dequeue(1))); ASSERT_EQ(30, i->GetValue());
  ASSERT_FALSE(q.WaitEmpty(1));
  i.reset(dynamic_cast<DynamicInteger*>(q.Dequeue(1))); ASSERT_EQ(40, i->GetValue());
  ASSERT_TRUE(q.WaitEmpty(0));
  ASSERT_EQ(NULL, q.Dequeue(1));
}


TEST(MultiThreading, SharedMessageQueueClean)
{
  std::set<int> s;

  try
  {
    SharedMessageQueue q;
    q.Enqueue(new DynamicInteger(10, s));
    q.Enqueue(new DynamicInteger(20, s));  
    throw OrthancException(ErrorCode_InternalError);
  }
  catch (OrthancException&)
  {
  }
}




#include "../Core/DicomNetworking/ReusableDicomUserConnection.h"

TEST(ReusableDicomUserConnection, DISABLED_Basic)
{
  ReusableDicomUserConnection c;
  c.SetMillisecondsBeforeClose(200);
  printf("START\n"); fflush(stdout);

  {
    RemoteModalityParameters remote("STORESCP", "localhost", 2000, ModalityManufacturer_Generic);
    ReusableDicomUserConnection::Locker lock(c, "ORTHANC", remote);
    lock.GetConnection().StoreFile("/home/jodogne/DICOM/Cardiac/MR.X.1.2.276.0.7230010.3.1.4.2831157719.2256.1336386844.676281");
  }

  printf("**\n"); fflush(stdout);
  SystemToolbox::USleep(1000000);
  printf("**\n"); fflush(stdout);

  {
    RemoteModalityParameters remote("STORESCP", "localhost", 2000, ModalityManufacturer_Generic);
    ReusableDicomUserConnection::Locker lock(c, "ORTHANC", remote);
    lock.GetConnection().StoreFile("/home/jodogne/DICOM/Cardiac/MR.X.1.2.276.0.7230010.3.1.4.2831157719.2256.1336386844.676277");
  }

  SystemToolbox::ServerBarrier();
  printf("DONE\n"); fflush(stdout);
}



class Tutu : public IServerCommand
{
private:
  int factor_;

public:
  Tutu(int f) : factor_(f)
  {
  }

  virtual bool Apply(ListOfStrings& outputs,
                     const ListOfStrings& inputs)
  {
    for (ListOfStrings::const_iterator 
           it = inputs.begin(); it != inputs.end(); ++it)
    {
      int a = boost::lexical_cast<int>(*it);
      int b = factor_ * a;

      printf("%d * %d = %d\n", a, factor_, b);

      //if (a == 84) { printf("BREAK\n"); return false; }

      outputs.push_back(boost::lexical_cast<std::string>(b));
    }

    SystemToolbox::USleep(30000);

    return true;
  }
};


static void Tata(ServerScheduler* s, ServerJob* j, bool* done)
{
  typedef IServerCommand::ListOfStrings  ListOfStrings;

  while (!(*done))
  {
    ListOfStrings l;
    s->GetListOfJobs(l);
    for (ListOfStrings::iterator it = l.begin(); it != l.end(); ++it)
    {
      printf(">> %s: %0.1f\n", it->c_str(), 100.0f * s->GetProgress(*it));
    }
    SystemToolbox::USleep(3000);
  }
}


TEST(MultiThreading, ServerScheduler)
{
  ServerScheduler scheduler(10);

  ServerJob job;
  ServerCommandInstance& f2 = job.AddCommand(new Tutu(2));
  ServerCommandInstance& f3 = job.AddCommand(new Tutu(3));
  ServerCommandInstance& f4 = job.AddCommand(new Tutu(4));
  ServerCommandInstance& f5 = job.AddCommand(new Tutu(5));
  f2.AddInput(boost::lexical_cast<std::string>(42));
  //f3.AddInput(boost::lexical_cast<std::string>(42));
  //f4.AddInput(boost::lexical_cast<std::string>(42));
  f2.ConnectOutput(f3);
  f3.ConnectOutput(f4);
  f4.ConnectOutput(f5);

  f3.SetConnectedToSink(true);
  f5.SetConnectedToSink(true);

  job.SetDescription("tutu");

  bool done = false;
  boost::thread t(Tata, &scheduler, &job, &done);


  //scheduler.Submit(job);

  IServerCommand::ListOfStrings l;
  scheduler.SubmitAndWait(l, job);

  ASSERT_EQ(2u, l.size());
  ASSERT_EQ(42 * 2 * 3, boost::lexical_cast<int>(l.front()));
  ASSERT_EQ(42 * 2 * 3 * 4 * 5, boost::lexical_cast<int>(l.back()));

  for (IServerCommand::ListOfStrings::iterator i = l.begin(); i != l.end(); i++)
  {
    printf("** %s\n", i->c_str());
  }

  //SystemToolbox::ServerBarrier();
  //SystemToolbox::USleep(3000000);

  scheduler.Stop();

  done = true;
  if (t.joinable())
  {
    t.join();
  }
}





#if !defined(ORTHANC_SANDBOXED)
#  error The macro ORTHANC_SANDBOXED must be defined
#endif

#if ORTHANC_SANDBOXED == 1
#  error The job engine cannot be used in sandboxed environments
#endif

#include "../Core/Logging.h"

#include <boost/math/special_functions/round.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <queue>

namespace Orthanc
{
  enum JobState
  {
    JobState_Pending,
    JobState_Running,
    JobState_Success,
    JobState_Failure,
    JobState_Paused,
    JobState_Retry
  };

  static const char* EnumerationToString(JobState state)
  {
    switch (state)
    {
      case JobState_Pending:
        return "Pending";
        
      case JobState_Running:
        return "Running";
        
      case JobState_Success:
        return "Success";
        
      case JobState_Failure:
        return "Failure";
        
      case JobState_Paused:
        return "Paused";
        
      case JobState_Retry:
        return "Retry";
        
      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }
  
  enum JobStepCode
  {
    JobStepCode_Success,
    JobStepCode_Failure,
    JobStepCode_Continue,
    JobStepCode_Retry
  };    
    
  class JobStepResult
  {
  private:
    JobStepCode code_;
    
  public:
    explicit JobStepResult(JobStepCode code) :
      code_(code)
    {
    }

    virtual ~JobStepResult()
    {
    }

    JobStepCode GetCode() const
    {
      return code_;
    }
  };


  class JobStepRetry : public JobStepResult
  {
  private:
    unsigned int  timeout_;   // Retry after "timeout_" milliseconds

  public:
    JobStepRetry(unsigned int timeout) :
      JobStepResult(JobStepCode_Retry),
      timeout_(timeout)
    {
    }

    unsigned int  GetTimeout() const
    {
      return timeout_;
    }
  };

  
  class IJob : public boost::noncopyable
  {
  public:
    virtual ~IJob()
    {
    }

    virtual JobStepResult* ExecuteStep() = 0;

    virtual void ReleaseResources() = 0;   // For pausing jobs

    virtual float GetProgress() = 0;

    virtual void GetDescription(Json::Value& value) = 0;
  };


  class JobStatus
  {
  private:
    ErrorCode      errorCode_;
    float          progress_;
    Json::Value    description_;

  public:
    JobStatus() :
      errorCode_(ErrorCode_InternalError),
      progress_(0),
      description_(Json::objectValue)
    {
    }

    JobStatus(ErrorCode code,
              IJob& job) :
      errorCode_(code),
      progress_(job.GetProgress())
    {
      if (progress_ < 0)
      {
        progress_ = 0;
      }
      
      if (progress_ > 1)
      {
        progress_ = 1;
      }

      job.GetDescription(description_);
    }

    ErrorCode GetErrorCode() const
    {
      return errorCode_;
    }

    float GetProgress() const
    {
      return progress_;
    }

    const Json::Value& GetDescription() const
    {
      return description_;
    }
  };


  class JobInfo
  {
  private:
    std::string                       id_;
    int                               priority_;
    JobState                          state_;
    boost::posix_time::ptime          timestamp_;
    boost::posix_time::ptime          creationTime_;
    boost::posix_time::ptime          lastStateChangeTime_;
    boost::posix_time::time_duration  runtime_;
    bool                              hasEta_;
    boost::posix_time::ptime          eta_;
    JobStatus                         status_;

  public:
    JobInfo(const std::string& id,
            int priority,
            JobState state,
            const JobStatus& status,
            const boost::posix_time::ptime& creationTime,
            const boost::posix_time::ptime& lastStateChangeTime,
            const boost::posix_time::time_duration& runtime) :
      id_(id),
      priority_(priority),
      state_(state),
      timestamp_(boost::posix_time::microsec_clock::universal_time()),
      creationTime_(creationTime),
      lastStateChangeTime_(lastStateChangeTime),
      runtime_(runtime),
      hasEta_(false),
      status_(status)
    {
      if (state_ == JobState_Running)
      {
        float ms = static_cast<float>(runtime_.total_milliseconds());

        if (status_.GetProgress() > 0.01f &&
            ms > 0.01f)
        {
          float remaining = boost::math::llround(1.0f - status_.GetProgress()) * ms;
          eta_ = timestamp_ + boost::posix_time::milliseconds(remaining);
          hasEta_ = true;
        }
      }
    }

    JobInfo() :
      priority_(0),
      state_(JobState_Failure),
      timestamp_(boost::posix_time::microsec_clock::universal_time()),
      creationTime_(timestamp_),
      lastStateChangeTime_(timestamp_),
      runtime_(boost::posix_time::milliseconds(0)),
      hasEta_(false)
    {
    }

    const std::string& GetIdentifier() const
    {
      return id_;
    }

    int GetPriority() const
    {
      return priority_;
    }

    JobState GetState() const
    {
      return state_;
    }

    const boost::posix_time::ptime& GetInfoTime() const
    {
      return timestamp_;
    }

    const boost::posix_time::ptime& GetCreationTime() const
    {
      return creationTime_;
    }

    const boost::posix_time::time_duration& GetRuntime() const
    {
      return runtime_;
    }

    bool HasEstimatedTimeOfArrival() const
    {
      return hasEta_;
    }

    bool HasCompletionTime() const
    {
      return (state_ == JobState_Success ||
              state_ == JobState_Failure);
    }

    const boost::posix_time::ptime& GetEstimatedTimeOfArrival() const
    {
      if (hasEta_)
      {
        return eta_;
      }
      else
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }
    }

    const boost::posix_time::ptime& GetCompletionTime() const
    {
      if (HasCompletionTime())
      {
        return lastStateChangeTime_;
      }
      else
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }
    }

    const JobStatus& GetStatus() const
    {
      return status_;
    }

    JobStatus& GetStatus()
    {
      return status_;
    }

    void Format(Json::Value& target) const
    {
      target = Json::objectValue;
      target["ID"] = id_;
      target["Priority"] = priority_;
      target["ErrorCode"] = static_cast<int>(status_.GetErrorCode());
      target["ErrorDescription"] = EnumerationToString(status_.GetErrorCode());
      target["State"] = EnumerationToString(state_);
      target["Timestamp"] = boost::posix_time::to_iso_string(timestamp_);
      target["CreationTime"] = boost::posix_time::to_iso_string(creationTime_);
      target["Runtime"] = static_cast<uint32_t>(runtime_.total_milliseconds());      
      target["Progress"] = boost::math::iround(status_.GetProgress() * 100.0f);
      target["Description"] = status_.GetDescription();

      if (HasEstimatedTimeOfArrival())
      {
        target["EstimatedTimeOfArrival"] = boost::posix_time::to_iso_string(GetEstimatedTimeOfArrival());
      }

      if (HasCompletionTime())
      {
        target["CompletionTime"] = boost::posix_time::to_iso_string(GetCompletionTime());
      }
    }
  };




  class JobsRegistry : public boost::noncopyable
  {
  private:
    class JobHandler : public boost::noncopyable
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

    struct PriorityComparator
    {
      bool operator() (JobHandler*& a,
                       JobHandler*& b) const
      {
        return a->GetPriority() < b->GetPriority();
      }                       
    };

    typedef std::map<std::string, JobHandler*>              JobsIndex;
    typedef std::list<JobHandler*>                          CompletedJobs;
    typedef std::set<JobHandler*>                           RetryJobs;
    typedef std::priority_queue<JobHandler*, 
                                std::vector<JobHandler*>,   // Could be a "std::deque"
                                PriorityComparator>         PendingJobs;

    boost::mutex               mutex_;
    JobsIndex                  jobsIndex_;
    PendingJobs                pendingJobs_;
    CompletedJobs              completedJobs_;
    RetryJobs                  retryJobs_;

    boost::condition_variable  pendingJobAvailable_;
    size_t                     maxCompletedJobs_;


#ifndef NDEBUG
    bool IsPendingJob(const JobHandler& job) const
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

    bool IsCompletedJob(JobHandler& job) const
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

    bool IsRetryJob(JobHandler& job) const
    {
      return retryJobs_.find(&job) != retryJobs_.end();
    }
#endif


    void CheckInvariants() const
    {
#ifndef NDEBUG
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
#endif
    }


    void ForgetOldCompletedJobs()
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


    void MarkRunningAsCompleted(JobHandler& job,
                                bool success)
    {
      LOG(INFO) << "Job has completed with " << (success ? "success" : "failure")
                << ": " << job.GetId();

      CheckInvariants();
      assert(job.GetState() == JobState_Running);

      job.SetState(success ? JobState_Success : JobState_Failure);

      completedJobs_.push_back(&job);
      ForgetOldCompletedJobs();

      CheckInvariants();
    }


    void MarkRunningAsRetry(JobHandler& job,
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


    void MarkRunningAsPaused(JobHandler& job)
    {
      LOG(INFO) << "Job paused: " << job.GetId();

      CheckInvariants();
      assert(job.GetState() == JobState_Running);

      job.SetState(JobState_Paused);

      CheckInvariants();
    }


  public:
    JobsRegistry() :
      maxCompletedJobs_(10)
    {
    }


    ~JobsRegistry()
    {
      for (JobsIndex::iterator it = jobsIndex_.begin(); it != jobsIndex_.end(); ++it)
      {
        assert(it->second != NULL);
        delete it->second;
      }
    }


    void SetMaxCompletedJobs(size_t i)
    {
      boost::mutex::scoped_lock lock(mutex_);
      CheckInvariants();

      maxCompletedJobs_ = i;
      ForgetOldCompletedJobs();

      CheckInvariants();
    }


    void ListJobs(std::set<std::string>& target)
    {
      boost::mutex::scoped_lock lock(mutex_);
      CheckInvariants();

      for (JobsIndex::const_iterator it = jobsIndex_.begin();
           it != jobsIndex_.end(); ++it)
      {
        target.insert(it->first);
      }
    }


    bool GetJobInfo(JobInfo& target,
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


    void Submit(std::string& id,
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


    void Submit(IJob* job,        // Takes ownership
                int priority)
    {
      std::string id;
      Submit(id, job, priority);
    }


    void SetPriority(const std::string& id,
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


    void Pause(const std::string& id)
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


    void Resume(const std::string& id)
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


    void Resubmit(const std::string& id)
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


    void ScheduleRetries()
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


    bool GetState(JobState& state,
                  const std::string& id)
    {
      boost::mutex::scoped_lock lock(mutex_);
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


    class RunningJob : public boost::noncopyable
    {
    private:
      JobsRegistry&  registry_;
      JobHandler*    handler_;  // Can only be accessed if the registry
      // mutex is locked!
      IJob*          job_;  // Will by design be in mutual exclusion,
      // because only one RunningJob can be
      // executed at a time on a JobHandler

      std::string    id_;
      int            priority_;
      JobState       targetState_;
      unsigned int   targetRetryTimeout_;
      
    public:
      RunningJob(JobsRegistry& registry,
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

      ~RunningJob()
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

      bool IsValid() const
      {
        return (handler_ != NULL &&
                job_ != NULL);
      }

      const std::string& GetId() const
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

      int GetPriority() const
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

      IJob& GetJob()
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

      bool IsPauseScheduled()
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

      void MarkSuccess()
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

      void MarkFailure()
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

      void MarkPause()
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

      void MarkRetry(unsigned int timeout)
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

      void UpdateStatus(ErrorCode code)
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
    };
  };



  class JobsEngine
  {
  private:
    enum State
    {
      State_Setup,
      State_Running,
      State_Stopping,
      State_Done
    };

    boost::mutex                stateMutex_;
    State                       state_;
    JobsRegistry                registry_;
    boost::thread               retryHandler_;
    std::vector<boost::thread>  workers_;

    bool ExecuteStep(JobsRegistry::RunningJob& running,
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

    static void RetryHandler(JobsEngine* engine)
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

    static void Worker(JobsEngine* engine,
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

  public:
    JobsEngine() :
      state_(State_Setup),
      workers_(1)
    {
    }

    ~JobsEngine()
    {
      if (state_ != State_Setup &&
          state_ != State_Done)
      {
        LOG(ERROR) << "INTERNAL ERROR: JobsEngine::Stop() should be invoked manually to avoid mess in the destruction order!";
        Stop();
      }
    }

    void SetWorkersCount(size_t count)
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
    
    JobsRegistry& GetRegistry()
    {
      return registry_;
    }

    void Start()
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


    void Stop()
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
  };
}



class DummyJob : public Orthanc::IJob
{
private:
  JobStepResult  result_;
  unsigned int count_;
  unsigned int steps_;

public:
  DummyJob() :
    result_(Orthanc::JobStepCode_Success),
    count_(0),
    steps_(4)
  {
  }

  explicit DummyJob(JobStepResult result) :
    result_(result),
    count_(0),
    steps_(4)
  {
  }

  virtual JobStepResult* ExecuteStep()
  {
    boost::this_thread::sleep(boost::posix_time::milliseconds(50));

    if (count_ == steps_ - 1)
    {
      return new JobStepResult(result_);
    }
    else
    {
      count_++;
      return new JobStepResult(JobStepCode_Continue);
    }
  }

  virtual void ReleaseResources()
  {
  }

  virtual float GetProgress()
  {
    return static_cast<float>(count_) / static_cast<float>(steps_ - 1);
  }

  virtual void GetDescription(Json::Value& value)
  {
    value["hello"] = "world";
  }
};


static bool CheckState(Orthanc::JobsRegistry& registry,
                       const std::string& id,
                       Orthanc::JobState state)
{
  Orthanc::JobState s;
  if (registry.GetState(s, id))
  {
    return state == s;
  }
  else
  {
    return false;
  }
}


TEST(JobsRegistry, Priority)
{
  JobsRegistry registry;

  std::string i1, i2, i3, i4;
  registry.Submit(i1, new DummyJob(), 10);
  registry.Submit(i2, new DummyJob(), 30);
  registry.Submit(i3, new DummyJob(), 20);
  registry.Submit(i4, new DummyJob(), 5);  

  registry.SetMaxCompletedJobs(2);

  std::set<std::string> id;
  registry.ListJobs(id);

  ASSERT_EQ(4u, id.size());
  ASSERT_TRUE(id.find(i1) != id.end());
  ASSERT_TRUE(id.find(i2) != id.end());
  ASSERT_TRUE(id.find(i3) != id.end());
  ASSERT_TRUE(id.find(i4) != id.end());

  ASSERT_TRUE(CheckState(registry, i2, Orthanc::JobState_Pending));

  {
    JobsRegistry::RunningJob job(registry, 0);
    ASSERT_TRUE(job.IsValid());
    ASSERT_EQ(30, job.GetPriority());
    ASSERT_EQ(i2, job.GetId());

    ASSERT_TRUE(CheckState(registry, i2, Orthanc::JobState_Running));
  }

  ASSERT_TRUE(CheckState(registry, i2, Orthanc::JobState_Failure));
  ASSERT_TRUE(CheckState(registry, i3, Orthanc::JobState_Pending));

  {
    JobsRegistry::RunningJob job(registry, 0);
    ASSERT_TRUE(job.IsValid());
    ASSERT_EQ(20, job.GetPriority());
    ASSERT_EQ(i3, job.GetId());

    job.MarkSuccess();

    ASSERT_TRUE(CheckState(registry, i3, Orthanc::JobState_Running));
  }

  ASSERT_TRUE(CheckState(registry, i3, Orthanc::JobState_Success));

  {
    JobsRegistry::RunningJob job(registry, 0);
    ASSERT_TRUE(job.IsValid());
    ASSERT_EQ(10, job.GetPriority());
    ASSERT_EQ(i1, job.GetId());
  }

  {
    JobsRegistry::RunningJob job(registry, 0);
    ASSERT_TRUE(job.IsValid());
    ASSERT_EQ(5, job.GetPriority());
    ASSERT_EQ(i4, job.GetId());
  }

  {
    JobsRegistry::RunningJob job(registry, 1);
    ASSERT_FALSE(job.IsValid());
  }

  Orthanc::JobState s;
  ASSERT_TRUE(registry.GetState(s, i1));
  ASSERT_FALSE(registry.GetState(s, i2));  // Removed because oldest
  ASSERT_FALSE(registry.GetState(s, i3));  // Removed because second oldest
  ASSERT_TRUE(registry.GetState(s, i4));

  registry.SetMaxCompletedJobs(1);  // (*)
  ASSERT_FALSE(registry.GetState(s, i1));  // Just discarded by (*)
  ASSERT_TRUE(registry.GetState(s, i4));
}


TEST(JobsRegistry, Simultaneous)
{
  JobsRegistry registry;

  std::string i1, i2;
  registry.Submit(i1, new DummyJob(), 20);
  registry.Submit(i2, new DummyJob(), 10);

  ASSERT_TRUE(CheckState(registry, i1, Orthanc::JobState_Pending));
  ASSERT_TRUE(CheckState(registry, i2, Orthanc::JobState_Pending));

  {
    JobsRegistry::RunningJob job1(registry, 0);
    JobsRegistry::RunningJob job2(registry, 0);

    ASSERT_TRUE(job1.IsValid());
    ASSERT_TRUE(job2.IsValid());

    job1.MarkFailure();
    job2.MarkSuccess();

    ASSERT_TRUE(CheckState(registry, i1, Orthanc::JobState_Running));
    ASSERT_TRUE(CheckState(registry, i2, Orthanc::JobState_Running));
  }

  ASSERT_TRUE(CheckState(registry, i1, Orthanc::JobState_Failure));
  ASSERT_TRUE(CheckState(registry, i2, Orthanc::JobState_Success));
}


TEST(JobsRegistry, Resubmit)
{
  JobsRegistry registry;

  std::string id;
  registry.Submit(id, new DummyJob(), 10);

  ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Pending));

  registry.Resubmit(id);
  ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Pending));

  {
    JobsRegistry::RunningJob job(registry, 0);
    ASSERT_TRUE(job.IsValid());
    job.MarkFailure();

    ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Running));

    registry.Resubmit(id);
    ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Running));
  }

  ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Failure));

  registry.Resubmit(id);
  ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Pending));

  {
    JobsRegistry::RunningJob job(registry, 0);
    ASSERT_TRUE(job.IsValid());
    ASSERT_EQ(id, job.GetId());

    job.MarkSuccess();
    ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Running));
  }

  ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Success));

  registry.Resubmit(id);
  ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Success));
}


TEST(JobsRegistry, Retry)
{
  JobsRegistry registry;

  std::string id;
  registry.Submit(id, new DummyJob(), 10);

  ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Pending));

  {
    JobsRegistry::RunningJob job(registry, 0);
    ASSERT_TRUE(job.IsValid());
    job.MarkRetry(0);

    ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Running));
  }

  ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Retry));

  registry.Resubmit(id);
  ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Retry));
  
  registry.ScheduleRetries();
  ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Pending));

  {
    JobsRegistry::RunningJob job(registry, 0);
    ASSERT_TRUE(job.IsValid());
    job.MarkSuccess();

    ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Running));
  }

  ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Success));
}


TEST(JobsRegistry, PausePending)
{
  JobsRegistry registry;

  std::string id;
  registry.Submit(id, new DummyJob(), 10);

  ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Pending));

  registry.Pause(id);
  ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Paused));

  registry.Pause(id);
  ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Paused));

  registry.Resubmit(id);
  ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Paused));

  registry.Resume(id);
  ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Pending));
}


TEST(JobsRegistry, PauseRunning)
{
  JobsRegistry registry;

  std::string id;
  registry.Submit(id, new DummyJob(), 10);

  ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Pending));

  {
    JobsRegistry::RunningJob job(registry, 0);
    ASSERT_TRUE(job.IsValid());

    registry.Resubmit(id);
    job.MarkPause();
    ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Running));
  }

  ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Paused));

  registry.Resubmit(id);
  ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Paused));

  registry.Resume(id);
  ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Pending));

  {
    JobsRegistry::RunningJob job(registry, 0);
    ASSERT_TRUE(job.IsValid());

    job.MarkSuccess();
    ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Running));
  }

  ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Success));
}


TEST(JobsRegistry, PauseRetry)
{
  JobsRegistry registry;

  std::string id;
  registry.Submit(id, new DummyJob(), 10);

  ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Pending));

  {
    JobsRegistry::RunningJob job(registry, 0);
    ASSERT_TRUE(job.IsValid());

    job.MarkRetry(0);
    ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Running));
  }

  ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Retry));

  registry.Pause(id);
  ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Paused));

  registry.Resume(id);
  ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Pending));

  {
    JobsRegistry::RunningJob job(registry, 0);
    ASSERT_TRUE(job.IsValid());

    job.MarkSuccess();
    ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Running));
  }

  ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Success));
}


TEST(JobsEngine, Basic)
{
  JobsEngine engine;

  std::string s;

  for (size_t i = 0; i < 20; i++)
    engine.GetRegistry().Submit(s, new DummyJob(), rand() % 10);

  engine.SetWorkersCount(3);
  engine.Start();

  boost::this_thread::sleep(boost::posix_time::milliseconds(100));

  {
    typedef std::set<std::string> Jobs;

    Jobs jobs;
    engine.GetRegistry().ListJobs(jobs);

    Json::Value v = Json::arrayValue;
    for (Jobs::const_iterator it = jobs.begin(); it != jobs.end(); ++it)
    {
      JobInfo info;

      if (engine.GetRegistry().GetJobInfo(info, *it))
      {
        Json::Value vv;
        info.Format(vv);
        v.append(vv);
      }
    }

    std::cout << v << std::endl;
  }
  std::cout << "====================================================" << std::endl;

  boost::this_thread::sleep(boost::posix_time::milliseconds(100));
  
  engine.Stop();


  {
    typedef std::set<std::string> Jobs;

    Jobs jobs;
    engine.GetRegistry().ListJobs(jobs);

    Json::Value v = Json::arrayValue;
    for (Jobs::const_iterator it = jobs.begin(); it != jobs.end(); ++it)
    {
      JobInfo info;

      if (engine.GetRegistry().GetJobInfo(info, *it))
      {
        Json::Value vv;
        info.Format(vv);
        v.append(vv);
      }
    }

    std::cout << v << std::endl;
  }
}
