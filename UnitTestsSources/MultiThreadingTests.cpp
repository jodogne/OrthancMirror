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

#include <boost/date_time/posix_time/posix_time.hpp>

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
    JobStepStatus_Error,
    JobStepStatus_Continue,
    JobStepStatus_Retry
  };


  class IJobStepResult : public boost::noncopyable
  {
  private:
    JobStepStatus status_;
    
  public:
    explicit IJobStepResult(JobStepStatus status) :
      status_(status)
    {
    }

    virtual ~IJobStepResult()
    {
    }

    JobStepStatus GetStatus() const
    {
      return status_;
    }
  };


  class RetryResult : public IJobStepResult
  {
  private:
    unsigned int  timeout_;   // Retry after "timeout_" milliseconds

  public:
    RetryResult(unsigned int timeout) :
      IJobStepResult(JobStepStatus_Retry),
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

    virtual IJobStepResult* ExecuteStep() = 0;

    virtual void ReleaseResources() = 0;   // For pausing jobs

    virtual float GetProgress() = 0;

    virtual void FormatStatus(Json::Value& value) = 0;
  };


  class JobsMonitor : public boost::noncopyable
  {
  private:
    class JobHandler : public boost::noncopyable
    {
    private:
      std::string               id_;
      JobState                  state_;
      std::auto_ptr<IJob>       job_;
      int                       priority_;  // "+inf()" means highest priority
      boost::posix_time::ptime  creationTime_;
      boost::posix_time::ptime  lastUpdateTime_;
      uint64_t                  runtime_;  // In milliseconds

    public:
      JobHandler(IJob* job,
                 int priority) :
        id_(Toolbox::GenerateUuid()),
        state_(JobState_Pending),
        job_(job),
        priority_(priority),
        creationTime_(boost::posix_time::microsec_clock::universal_time()),
        lastUpdateTime_(creationTime_),
        runtime_(0)
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
    };

  public:
    void Submit(std::string& id,
                IJob* job,
                int priority)
    {
      std::auto_ptr<JobHandler>  handler(new JobHandler(job, priority));
      id = handler->GetId();
    }

    void SetPriority(const std::string& id,
                     int priority)
    {
      // TODO
    }

    void Pause(const std::string& id)
    {
      // TODO
    }

    void Resume(const std::string& id)
    {
      // TODO
    }

    void Resubmit(const std::string& id)
    {
      // TODO
    }

    class JobToRun : public boost::noncopyable
    {
    private:
      JobHandler*  handler_;
      
    public:
      JobToRun(JobsMonitor& that,
               unsigned int timeout) :
        handler_(NULL)
      {
      }

      bool IsValid() const
      {
        return handler_ != NULL;
      }

      
    };
  };
}
