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
#include "../Core/MultiThreading/Mutex.h"
#include "../Core/MultiThreading/ReaderWriterLock.h"

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


TEST(MultiThreading, Mutex)
{
  Mutex mutex;
  Locker locker(mutex);
}


TEST(MultiThreading, ReaderWriterLock)
{
  ReaderWriterLock lock;

  {
    Locker locker1(lock.ForReader());
    Locker locker2(lock.ForReader());
  }

  {
    Locker locker3(lock.ForWriter());
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
  
  enum JobStepStatus
  {
    JobStepStatus_Success,
    JobStepStatus_Failure,
    JobStepStatus_Continue,
    JobStepStatus_Retry
  };


  class JobStepResult
  {
  private:
    JobStepStatus status_;
    
  public:
    explicit JobStepResult(JobStepStatus status) :
    status_(status)
    {
    }

    virtual ~JobStepResult()
    {
    }

    JobStepStatus GetStatus() const
    {
      return status_;
    }
  };


  class RetryResult : public JobStepResult
  {
  private:
    unsigned int  timeout_;   // Retry after "timeout_" milliseconds

  public:
    RetryResult(unsigned int timeout) :
    JobStepResult(JobStepStatus_Retry),
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

    virtual void FormatStatus(Json::Value& value) = 0;
  };


  class JobHandler : public boost::noncopyable
  {
  private:
    std::string               id_;
    JobState                  state_;
    std::auto_ptr<IJob>       job_;
    int                       priority_;  // "+inf()" means highest priority
    boost::posix_time::ptime  creationTime_;
    boost::posix_time::ptime  lastUpdateTime_;
    boost::posix_time::ptime  retryTime_;
    uint64_t                  runtime_;  // In milliseconds
    bool                      pauseScheduled_;

    void SetStateInternal(JobState state) 
    {
      const boost::posix_time::ptime now = boost::posix_time::microsec_clock::universal_time();

      if (state_ == JobState_Running)
      {
        runtime_ += (now - lastUpdateTime_).total_milliseconds();
      }

      state_ = state;
      lastUpdateTime_ = now;
      pauseScheduled_ = false;
    }

  public:
    JobHandler(IJob* job,
               int priority) :
      id_(Toolbox::GenerateUuid()),
      state_(JobState_Pending),
      job_(job),
      priority_(priority),
      creationTime_(boost::posix_time::microsec_clock::universal_time()),
      lastUpdateTime_(creationTime_),
      runtime_(0),
      pauseScheduled_(false)
    {
      if (job == NULL)
      {
        throw OrthancException(ErrorCode_NullPointer);
      }
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
        return retryTime_ >= now;
      }
    }
  };


  class JobsMonitor : public boost::noncopyable
  {
  private:
    struct PriorityComparator
    {
      bool operator() (JobHandler*& a,
                       JobHandler*& b) const
      {
        return a->GetPriority() < b->GetPriority();
      }                       
    };

    typedef std::map<std::string, JobHandler*>              JobsIndex;
    typedef std::list<const JobHandler*>                    CompletedJobs;
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

    bool IsCompletedJob(const JobHandler& job) const
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


    void CheckInvariants()
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

      for (JobsIndex::iterator it = jobsIndex_.begin();
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
      boost::mutex::scoped_lock lock(mutex_);
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
      boost::mutex::scoped_lock lock(mutex_);
      CheckInvariants();

      assert(job.GetState() == JobState_Running &&
             retryJobs_.find(&job) == retryJobs_.end());

      retryJobs_.insert(&job);
      job.SetRetryState(timeout);

      CheckInvariants();
    }


    void MarkRunningAsPaused(JobHandler& job)
    {
      boost::mutex::scoped_lock lock(mutex_);
      CheckInvariants();
      assert(job.GetState() == JobState_Running);

      job.SetState(JobState_Paused);

      CheckInvariants();
    }


    JobHandler* WaitPendingJob(unsigned int timeout)
    {
      boost::mutex::scoped_lock lock(mutex_);

      while (pendingJobs_.empty())
      {
        if (timeout == 0)
        {
          pendingJobAvailable_.wait(lock);
        }
        else
        {
          bool success = pendingJobAvailable_.timed_wait
            (lock, boost::posix_time::milliseconds(timeout));
          if (!success)
          {
            return NULL;
          }
        }
      }

      JobHandler* job = pendingJobs_.top();
      pendingJobs_.pop();
      
      job->SetState(JobState_Running);
      return job;
    }


  public:
    JobsMonitor() :
      maxCompletedJobs_(10)
    {
    }


    ~JobsMonitor()
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


    void Submit(std::string& id,
                IJob* job,        // Takes ownership
                int priority)
    {
      std::auto_ptr<JobHandler>  handler(new JobHandler(job, priority));

      boost::mutex::scoped_lock lock(mutex_);
      CheckInvariants();
      
      id = handler->GetId();
      pendingJobs_.push(handler.get());
      jobsIndex_.insert(std::make_pair(id, handler.release()));

      pendingJobAvailable_.notify_one();

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
          (*it)->SetState(JobState_Pending);
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
      JobsMonitor&  that_;
      JobHandler*   handler_;
      JobState      targetState_;
      unsigned int  retryTimeout_;
      
    public:
      RunningJob(JobsMonitor& that,
                 unsigned int timeout) :
        that_(that),
        handler_(NULL),
        targetState_(JobState_Failure),
        retryTimeout_(0)
      {
        handler_ = that_.WaitPendingJob(timeout);
      }

      ~RunningJob()
      {
        if (IsValid())
        {
          switch (targetState_)
          {
            case JobState_Failure:
              that_.MarkRunningAsCompleted(*handler_, false);
              break;

            case JobState_Success:
              that_.MarkRunningAsCompleted(*handler_, true);
              break;

            case JobState_Paused:
              that_.MarkRunningAsPaused(*handler_);
              break;            

            case JobState_Retry:
              that_.MarkRunningAsRetry(*handler_, retryTimeout_);
              break;
            
            default:
              assert(0);
          }
        }
      }

      bool IsValid() const
      {
        return handler_ != NULL;
      }

      const std::string& GetId() const
      {
        if (IsValid())
        {
          return handler_->GetId();
        }
        else
        {
          throw OrthancException(ErrorCode_BadSequenceOfCalls);
        }      
      }

      int GetPriority() const
      {
        if (IsValid())
        {
          return handler_->GetPriority();
        }
        else
        {
          throw OrthancException(ErrorCode_BadSequenceOfCalls);
        }      
      }

      bool IsPauseScheduled()
      {
        if (!IsValid())
        {
          throw OrthancException(ErrorCode_BadSequenceOfCalls);
        }

        boost::mutex::scoped_lock lock(that_.mutex_);
        that_.CheckInvariants();
        assert(handler_->GetState() == JobState_Running);

        return handler_->IsPauseScheduled();
      }

      IJob& GetJob()
      {
        if (!IsValid())
        {
          throw OrthancException(ErrorCode_BadSequenceOfCalls);
        }

        boost::mutex::scoped_lock lock(that_.mutex_);
        that_.CheckInvariants();
        assert(handler_->GetState() == JobState_Running);

        return handler_->GetJob();
      }

      void MarkSuccess()
      {
        if (!IsValid())
        {
          throw OrthancException(ErrorCode_BadSequenceOfCalls);
        }

        targetState_ = JobState_Success;
      }

      void MarkFailure()
      {
        if (!IsValid())
        {
          throw OrthancException(ErrorCode_BadSequenceOfCalls);
        }

        targetState_ = JobState_Failure;
      }

      void MarkPause()
      {
        if (!IsValid())
        {
          throw OrthancException(ErrorCode_BadSequenceOfCalls);
        }

        targetState_ = JobState_Paused;
      }

      void MarkRetry(unsigned int timeout)
      {
        if (!IsValid())
        {
          throw OrthancException(ErrorCode_BadSequenceOfCalls);
        }

        targetState_ = JobState_Retry;
        retryTimeout_ = timeout;
      }
    };
  };
}



class DummyJob : public Orthanc::IJob
{
private:
  JobStepResult  result_;

public:
  DummyJob() :
    result_(Orthanc::JobStepStatus_Success)
  {
  }

  explicit DummyJob(JobStepResult result) :
  result_(result)
  {
  }

  virtual JobStepResult* ExecuteStep()
  {
    return new JobStepResult(result_);
  }

  virtual void ReleaseResources()
  {
  }

  virtual float GetProgress()
  {
    return 0;
  }

  virtual void FormatStatus(Json::Value& value)
  {
  }
};


static bool CheckState(Orthanc::JobsMonitor& monitor,
                       const std::string& id,
                       Orthanc::JobState state)
{
  Orthanc::JobState s;
  if (monitor.GetState(s, id))
  {
    return state == s;
  }
  else
  {
    return false;
  }
}


TEST(JobsMonitor, Priority)
{
  JobsMonitor monitor;

  std::string i1, i2, i3, i4;
  monitor.Submit(i1, new DummyJob(), 10);
  monitor.Submit(i2, new DummyJob(), 30);
  monitor.Submit(i3, new DummyJob(), 20);
  monitor.Submit(i4, new DummyJob(), 5);  

  monitor.SetMaxCompletedJobs(2);

  std::set<std::string> id;
  monitor.ListJobs(id);

  ASSERT_EQ(4u, id.size());
  ASSERT_TRUE(id.find(i1) != id.end());
  ASSERT_TRUE(id.find(i2) != id.end());
  ASSERT_TRUE(id.find(i3) != id.end());
  ASSERT_TRUE(id.find(i4) != id.end());

  ASSERT_TRUE(CheckState(monitor, i2, Orthanc::JobState_Pending));

  {
    JobsMonitor::RunningJob job(monitor, 0);
    ASSERT_TRUE(job.IsValid());
    ASSERT_EQ(30, job.GetPriority());
    ASSERT_EQ(i2, job.GetId());

    ASSERT_TRUE(CheckState(monitor, i2, Orthanc::JobState_Running));
  }

  ASSERT_TRUE(CheckState(monitor, i2, Orthanc::JobState_Failure));
  ASSERT_TRUE(CheckState(monitor, i3, Orthanc::JobState_Pending));

  {
    JobsMonitor::RunningJob job(monitor, 0);
    ASSERT_TRUE(job.IsValid());
    ASSERT_EQ(20, job.GetPriority());
    ASSERT_EQ(i3, job.GetId());

    job.MarkSuccess();

    ASSERT_TRUE(CheckState(monitor, i3, Orthanc::JobState_Running));
  }

  ASSERT_TRUE(CheckState(monitor, i3, Orthanc::JobState_Success));

  {
    JobsMonitor::RunningJob job(monitor, 0);
    ASSERT_TRUE(job.IsValid());
    ASSERT_EQ(10, job.GetPriority());
    ASSERT_EQ(i1, job.GetId());
  }

  {
    JobsMonitor::RunningJob job(monitor, 0);
    ASSERT_TRUE(job.IsValid());
    ASSERT_EQ(5, job.GetPriority());
    ASSERT_EQ(i4, job.GetId());
  }

  {
    JobsMonitor::RunningJob job(monitor, 1);
    ASSERT_FALSE(job.IsValid());
  }

  Orthanc::JobState s;
  ASSERT_TRUE(monitor.GetState(s, i1));
  ASSERT_FALSE(monitor.GetState(s, i2));  // Removed because oldest
  ASSERT_FALSE(monitor.GetState(s, i3));  // Removed because second oldest
  ASSERT_TRUE(monitor.GetState(s, i4));

  monitor.SetMaxCompletedJobs(1);  // (*)
  ASSERT_FALSE(monitor.GetState(s, i1));  // Just discarded by (*)
  ASSERT_TRUE(monitor.GetState(s, i4));
}


TEST(JobsMonitor, Resubmit)
{
  JobsMonitor monitor;

  std::string id;
  monitor.Submit(id, new DummyJob(), 10);

  ASSERT_TRUE(CheckState(monitor, id, Orthanc::JobState_Pending));

  monitor.Resubmit(id);
  ASSERT_TRUE(CheckState(monitor, id, Orthanc::JobState_Pending));

  {
    JobsMonitor::RunningJob job(monitor, 0);
    ASSERT_TRUE(job.IsValid());
    job.MarkFailure();

    ASSERT_TRUE(CheckState(monitor, id, Orthanc::JobState_Running));

    monitor.Resubmit(id);
    ASSERT_TRUE(CheckState(monitor, id, Orthanc::JobState_Running));
  }

  ASSERT_TRUE(CheckState(monitor, id, Orthanc::JobState_Failure));

  monitor.Resubmit(id);
  ASSERT_TRUE(CheckState(monitor, id, Orthanc::JobState_Pending));

  {
    JobsMonitor::RunningJob job(monitor, 0);
    ASSERT_TRUE(job.IsValid());
    ASSERT_EQ(id, job.GetId());

    job.MarkSuccess();
    ASSERT_TRUE(CheckState(monitor, id, Orthanc::JobState_Running));
  }

  ASSERT_TRUE(CheckState(monitor, id, Orthanc::JobState_Success));
}
