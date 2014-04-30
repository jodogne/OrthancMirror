#include "gtest/gtest.h"
#include <glog/logging.h>

#include "../Core/OrthancException.h"
#include "../Core/Toolbox.h"
#include "../Core/MultiThreading/ArrayFilledByThreads.h"
#include "../Core/MultiThreading/Locker.h"
#include "../Core/MultiThreading/Mutex.h"
#include "../Core/MultiThreading/ReaderWriterLock.h"
#include "../Core/MultiThreading/ThreadedCommandProcessor.h"

using namespace Orthanc;

namespace
{
  class DynamicInteger : public ICommand
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

    virtual bool Execute()
    {
      static boost::mutex mutex;
      boost::mutex::scoped_lock lock(mutex);
      target_.insert(value_);
      return true;
    }
  };

  class MyFiller : public ArrayFilledByThreads::IFiller
  {
  private:
    int size_;
    unsigned int created_;
    std::set<int> set_;

  public:
    MyFiller(int size) : size_(size), created_(0)
    {
    }

    virtual size_t GetFillerSize()
    {
      return size_;
    }

    virtual IDynamicObject* GetFillerItem(size_t index)
    {
      static boost::mutex mutex;
      boost::mutex::scoped_lock lock(mutex);
      created_++;
      return new DynamicInteger(index * 2, set_);
    }

    unsigned int GetCreatedCount() const
    {
      return created_;
    }

    std::set<int> GetSet()
    {
      return set_;
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
    throw OrthancException("Nope");
  }
  catch (OrthancException&)
  {
  }
}


TEST(MultiThreading, ArrayFilledByThreadEmpty)
{
  MyFiller f(0);
  ArrayFilledByThreads a(f);
  a.SetThreadCount(1);
  ASSERT_EQ(0, a.GetSize());
}


TEST(MultiThreading, ArrayFilledByThread1)
{
  MyFiller f(100);
  ArrayFilledByThreads a(f);
  a.SetThreadCount(1);
  ASSERT_EQ(100, a.GetSize());
  for (size_t i = 0; i < a.GetSize(); i++)
  {
    ASSERT_EQ(2 * i, dynamic_cast<DynamicInteger&>(a.GetItem(i)).GetValue());
  }
}


TEST(MultiThreading, ArrayFilledByThread4)
{
  MyFiller f(100);
  ArrayFilledByThreads a(f);
  a.SetThreadCount(4);
  ASSERT_EQ(100, a.GetSize());
  for (size_t i = 0; i < a.GetSize(); i++)
  {
    ASSERT_EQ(2 * i, dynamic_cast<DynamicInteger&>(a.GetItem(i)).GetValue());
  }

  ASSERT_EQ(100u, f.GetCreatedCount());

  a.Invalidate();

  ASSERT_EQ(100, a.GetSize());
  ASSERT_EQ(200u, f.GetCreatedCount());
  ASSERT_EQ(4u, a.GetThreadCount());
  ASSERT_TRUE(f.GetSet().empty());

  for (size_t i = 0; i < a.GetSize(); i++)
  {
    ASSERT_EQ(2 * i, dynamic_cast<DynamicInteger&>(a.GetItem(i)).GetValue());
  }
}


TEST(MultiThreading, CommandProcessor)
{
  ThreadedCommandProcessor p(4);

  std::set<int> s;

  for (size_t i = 0; i < 100; i++)
  {
    p.Post(new DynamicInteger(i * 2, s));
  }

  p.Join();

  for (size_t i = 0; i < 200; i++)
  {
    if (i % 2)
      ASSERT_TRUE(s.find(i) == s.end());
    else
      ASSERT_TRUE(s.find(i) != s.end());
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



#include "../OrthancServer/DicomProtocol/ReusableDicomUserConnection.h"

TEST(ReusableDicomUserConnection, DISABLED_Basic)
{
  ReusableDicomUserConnection c;
  c.SetMillisecondsBeforeClose(200);
  printf("START\n"); fflush(stdout);

  {
    ReusableDicomUserConnection::Locker lock(c, "STORESCP", "localhost", 2000, ModalityManufacturer_Generic);
    lock.GetConnection().StoreFile("/home/jodogne/DICOM/Cardiac/MR.X.1.2.276.0.7230010.3.1.4.2831157719.2256.1336386844.676281");
  }

  printf("**\n"); fflush(stdout);
  Toolbox::USleep(1000000);
  printf("**\n"); fflush(stdout);

  {
    ReusableDicomUserConnection::Locker lock(c, "STORESCP", "localhost", 2000, ModalityManufacturer_Generic);
    lock.GetConnection().StoreFile("/home/jodogne/DICOM/Cardiac/MR.X.1.2.276.0.7230010.3.1.4.2831157719.2256.1336386844.676277");
  }

  Toolbox::ServerBarrier();
  printf("DONE\n"); fflush(stdout);
}



#include "../Core/ICommand.h"
#include "../Core/Toolbox.h"
#include "../Core/Uuid.h"
#include "../Core/MultiThreading/SharedMessageQueue.h"
#include <boost/lexical_cast.hpp>


namespace Orthanc
{
  class IServerFilter
  {
  public:
    typedef std::list<std::string>  ListOfStrings;

    virtual ~IServerFilter()
    {
    }

    virtual bool Apply(ListOfStrings& outputs,
                       const ListOfStrings& inputs) = 0;

    virtual bool SendOutputsToSink() const = 0;
  };


  class Sink : public IServerFilter
  {
  private:
    ListOfStrings& target_;

  public:
    Sink(ListOfStrings& target) : target_(target)
    {
    }

    virtual bool SendOutputsToSink() const
    {
      return false;
    }

    virtual bool Apply(ListOfStrings& outputs,
                       const ListOfStrings& inputs)
    {
      for (ListOfStrings::const_iterator 
             it = inputs.begin(); it != inputs.end(); it++)
      {
        target_.push_back(*it);
      }

      return true;
    }    
  };



  class IServerFilterListener
  {
  public:
    virtual ~IServerFilterListener()
    {
    }

    virtual void SignalSuccess(const std::string& jobId) = 0;

    virtual void SignalFailure(const std::string& jobId) = 0;
  };


  class ServerFilterInstance : public IDynamicObject
  {
    friend class ServerScheduler;

  private:
    typedef IServerFilter::ListOfStrings  ListOfStrings;

    IServerFilter *filter_;
    std::string jobId_;
    ListOfStrings inputs_;
    std::list<ServerFilterInstance*> next_;

    bool Execute(IServerFilterListener& listener)
    {
      ListOfStrings outputs;
      if (!filter_->Apply(outputs, inputs_))
      {
        listener.SignalFailure(jobId_);
        return true;
      }

      for (std::list<ServerFilterInstance*>::iterator
             it = next_.begin(); it != next_.end(); it++)
      {
        for (ListOfStrings::const_iterator
               output = outputs.begin(); output != outputs.end(); output++)
        {
          (*it)->AddInput(*output);
        }
      }

      listener.SignalSuccess(jobId_);
      return true;
    }


  public:
    ServerFilterInstance(IServerFilter *filter,
                         const std::string& jobId) : 
      filter_(filter), 
      jobId_(jobId)
    {
      if (filter_ == NULL)
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
    }

    virtual ~ServerFilterInstance()
    {
      if (filter_ != NULL)
      {
        delete filter_;
      }
    }

    const std::string& GetJobId() const
    {
      return jobId_;
    }

    void AddInput(const std::string& input)
    {
      inputs_.push_back(input);
    }

    void ConnectNext(ServerFilterInstance& filter)
    {
      next_.push_back(&filter);
    }

    const std::list<ServerFilterInstance*>& GetNextFilters() const
    {
      return next_;
    }

    IServerFilter& GetFilter() const
    {
      return *filter_;
    }
  };


  class ServerJob
  {
    friend class ServerScheduler;

  private:
    std::list<ServerFilterInstance*> filters_;
    std::string jobId_;
    bool submitted_;
    std::string description_;

    
    void CheckOrdering()
    {
      std::map<ServerFilterInstance*, unsigned int> index;

      unsigned int count = 0;
      for (std::list<ServerFilterInstance*>::const_iterator
             it = filters_.begin(); it != filters_.end(); it++)
      {
        index[*it] = count++;
      }

      for (std::list<ServerFilterInstance*>::const_iterator
             it = filters_.begin(); it != filters_.end(); it++)
      {
        const std::list<ServerFilterInstance*>& nextFilters = (*it)->GetNextFilters();

        for (std::list<ServerFilterInstance*>::const_iterator
               next = nextFilters.begin(); next != nextFilters.end(); next++)
        {
          if (index.find(*next) == index.end() ||
              index[*next] <= index[*it])
          {
            // You must reorder your calls to "ServerJob::AddFilter"
            throw OrthancException("Bad ordering of filters in a job");
          }
        }
      }
    }


    size_t Submit(SharedMessageQueue& target,
                  IServerFilterListener& listener)
    {
      if (submitted_)
      {
        // This job has already been submitted
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }

      CheckOrdering();

      size_t size = filters_.size();

      for (std::list<ServerFilterInstance*>::iterator 
             it = filters_.begin(); it != filters_.end(); it++)
      {
        target.Enqueue(*it);
      }

      filters_.clear();
      submitted_ = true;

      return size;
    }

  public:
    ServerJob()
    {
      jobId_ = Toolbox::GenerateUuid();      
      submitted_ = false;
      description_ = "no description";
    }

    ~ServerJob()
    {
      for (std::list<ServerFilterInstance*>::iterator
             it = filters_.begin(); it != filters_.end(); it++)
      {
        delete *it;
      }
    }

    const std::string& GetId() const
    {
      return jobId_;
    }

    void SetDescription(const char* description)
    {
      description_ = description;
    }

    const std::string& GetDescription() const
    {
      return description_;
    }

    ServerFilterInstance& AddFilter(IServerFilter* filter)
    {
      if (submitted_)
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }

      filters_.push_back(new ServerFilterInstance(filter, jobId_));
      
      return *filters_.back();
    }
  };


  class ServerScheduler : public IServerFilterListener
  {
  private:
    struct JobInfo
    {
      bool watched_;
      bool cancel_;
      size_t size_;
      size_t success_;
      size_t failures_;
      std::string description_;
    };

    enum JobStatus
    {
      JobStatus_Running = 1,
      JobStatus_Success = 2,
      JobStatus_Failure = 3
    };

    typedef IServerFilter::ListOfStrings  ListOfStrings;
    typedef std::map<std::string, JobInfo> Jobs;

    boost::mutex mutex_;
    boost::condition_variable jobFinished_;
    Jobs jobs_;
    SharedMessageQueue queue_;
    bool finish_;
    boost::thread worker_;
    std::map<std::string, JobStatus> watchedJobStatus_;

    JobInfo& GetJobInfo(const std::string& jobId)
    {
      Jobs::iterator info = jobs_.find(jobId);

      if (info == jobs_.end())
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      return info->second;
    }

    virtual void SignalSuccess(const std::string& jobId)
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
          jobFinished_.notify_all();
        }

        LOG(INFO) << "Job successfully finished (" << info.description_ << ")";
        jobs_.erase(jobId);
      }
    }

    virtual void SignalFailure(const std::string& jobId)
    {
      boost::mutex::scoped_lock lock(mutex_);

      JobInfo& info = GetJobInfo(jobId);
      info.failures_++;

      if (info.success_ + info.failures_ >= info.size_)
      {
        if (info.watched_)
        {
          watchedJobStatus_[jobId] = JobStatus_Failure;
          jobFinished_.notify_all();
        }

        LOG(ERROR) << "Job has failed (" << info.description_ << ")";
        jobs_.erase(jobId);
      }
    }

    static void Worker(ServerScheduler* that)
    {
      static const int32_t TIMEOUT = 100;

      while (!that->finish_)
      {
        std::auto_ptr<IDynamicObject> object(that->queue_.Dequeue(TIMEOUT));
        if (object.get() != NULL)
        {
          ServerFilterInstance& filter = dynamic_cast<ServerFilterInstance&>(*object);

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

    void SubmitInternal(ServerJob& job,
                        bool watched)
    {
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

  public:
    ServerScheduler()
    {
      finish_ = false;
      worker_ = boost::thread(Worker, this);
    }

    ~ServerScheduler()
    {
      finish_ = true;
      worker_.join();
    }

    void Submit(ServerJob& job)
    {
      if (job.filters_.empty())
      {
        return;
      }

      SubmitInternal(job, false);
    }

    bool SubmitAndWait(ListOfStrings& outputs,
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
      ServerFilterInstance& sink = job.AddFilter(new Sink(outputs));

      for (std::list<ServerFilterInstance*>::iterator
             it = job.filters_.begin(); it != job.filters_.end(); it++)
      {
        if ((*it) != &sink &&
            (*it)->GetNextFilters().size() == 0 &&
            (*it)->GetFilter().SendOutputsToSink())
        {
          (*it)->ConnectNext(sink);
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
          jobFinished_.wait(lock);
        }

        status = watchedJobStatus_[jobId];
        watchedJobStatus_.erase(jobId);
      }

      return (status == JobStatus_Success);
    }


    bool IsRunning(const std::string& jobId)
    {
      boost::mutex::scoped_lock lock(mutex_);
      return jobs_.find(jobId) != jobs_.end();
    }


    void Cancel(const std::string& jobId)
    {
      boost::mutex::scoped_lock lock(mutex_);

      Jobs::iterator job = jobs_.find(jobId);

      if (job != jobs_.end())
      {
        job->second.cancel_ = true;
        LOG(WARNING) << "Canceling a job (" << job->second.description_ << ")";
      }
    }


    // Returns a number between 0 and 1
    float GetProgress(const std::string& jobId) 
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
        return job->second.success_;
      }

      return (static_cast<float>(job->second.success_) / 
              static_cast<float>(job->second.size_ - 1));
    }

    bool IsRunning(const ServerJob& job)
    {
      return IsRunning(job.GetId());
    }

    void Cancel(const ServerJob& job) 
    {
      Cancel(job.GetId());
    }

    float GetProgress(const ServerJob& job) 
    {
      return GetProgress(job);
    }

    void GetListOfJobs(ListOfStrings& jobs)
    {
      boost::mutex::scoped_lock lock(mutex_);

      jobs.clear();

      for (Jobs::const_iterator 
             it = jobs_.begin(); it != jobs_.end(); it++)
      {
        jobs.push_back(it->first);
      }
    }
  };

}



class Tutu : public IServerFilter
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
           it = inputs.begin(); it != inputs.end(); it++)
    {
      int a = boost::lexical_cast<int>(*it);
      int b = factor_ * a;

      printf("%d * %d = %d\n", a, factor_, b);

      //if (a == 84) { printf("BREAK\n"); return false; }

      outputs.push_back(boost::lexical_cast<std::string>(b));
    }

    Toolbox::USleep(1000000);

    return true;
  }

  virtual bool SendOutputsToSink() const
  {
    return true;
  }
};


static void Tata(ServerScheduler* s, ServerJob* j, bool* done)
{
  typedef IServerFilter::ListOfStrings  ListOfStrings;

#if 1
  while (!(*done))
  {
    ListOfStrings l;
    s->GetListOfJobs(l);
    for (ListOfStrings::iterator i = l.begin(); i != l.end(); i++)
      printf(">> %s: %0.1f\n", i->c_str(), 100.0f * s->GetProgress(*i));
    Toolbox::USleep(100000);
  }
#else
  ListOfStrings l;
  s->GetListOfJobs(l);
  for (ListOfStrings::iterator i = l.begin(); i != l.end(); i++)
    printf(">> %s\n", i->c_str());
  Toolbox::USleep(1500000);
  s->Cancel(*j);
  Toolbox::USleep(1000000);
  s->GetListOfJobs(l);
  for (ListOfStrings::iterator i = l.begin(); i != l.end(); i++)
    printf(">> %s\n", i->c_str());
#endif
}


TEST(Toto, Toto)
{
  ServerScheduler scheduler;

  ServerJob job;
  ServerFilterInstance& f2 = job.AddFilter(new Tutu(2));
  ServerFilterInstance& f3 = job.AddFilter(new Tutu(3));
  ServerFilterInstance& f4 = job.AddFilter(new Tutu(4));
  ServerFilterInstance& f5 = job.AddFilter(new Tutu(5));
  f2.AddInput(boost::lexical_cast<std::string>(42));
  //f3.AddInput(boost::lexical_cast<std::string>(42));
  //f4.AddInput(boost::lexical_cast<std::string>(42));
  f2.ConnectNext(f3);
  f3.ConnectNext(f4);
  f4.ConnectNext(f5);

  job.SetDescription("tutu");

  bool done = false;
  boost::thread t(Tata, &scheduler, &job, &done);


  //scheduler.Submit(job);

  IServerFilter::ListOfStrings l;
  scheduler.SubmitAndWait(l, job);

  for (IServerFilter::ListOfStrings::iterator i = l.begin(); i != l.end(); i++)
  {
    printf("** %s\n", i->c_str());
  }

  //Toolbox::ServerBarrier();
  //Toolbox::USleep(3000000);

  done = true;
  t.join();
}
