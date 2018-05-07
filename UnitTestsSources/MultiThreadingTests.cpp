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

#include "../Core/JobsEngine/JobStepRetry.h"
#include "../Core/JobsEngine/JobsEngine.h"
#include "../Core/MultiThreading/Locker.h"
#include "../Core/OrthancException.h"
#include "../Core/SystemToolbox.h"
#include "../Core/Toolbox.h"
#include "../OrthancServer/Scheduler/ServerScheduler.h"

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

  virtual void Start()
  {
  }
    
  virtual JobStepResult* ExecuteStep()
  {
    boost::this_thread::sleep(boost::posix_time::milliseconds(10));

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



#include "../OrthancServer/ServerContext.h"

namespace Orthanc
{
  class InstancesIteratorJob : public IJob
  {
  private:
    bool                      started_;
    std::vector<std::string>  instances_;
    size_t                    position_;

  public:
    InstancesIteratorJob() :
      started_(false),
      position_(0)
    {
    }

    void Reserve(size_t size)
    {
      if (started_)
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }
      else
      {
        instances_.reserve(size);
      }
    }
    
    void AddInstance(const std::string& instance)
    {
      if (started_)
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }
      else
      {
        instances_.push_back(instance);
      }
    }
    
    virtual void Start()
    {
      started_ = true;
    }
    
    virtual float GetProgress()
    {
      if (instances_.size() == 0)
      {
        return 0;
      }
      else
      {
        return (static_cast<float>(position_) /
                static_cast<float>(instances_.size()));
      }
    }

    bool IsStarted() const
    {
      return started_;
    }

    bool IsDone() const
    {
      if (instances_.size() == 0)
      {
        return true;
      }
      else
      {
        return (position_ == instances_.size() - 1);
      }
    }

    void Next()
    {
      if (IsDone())
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }
      else
      {
        position_ += 1;
      }
    }

    const std::string& GetCurrentInstance() const
    {
      if (IsDone())
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }
      else
      {
        return instances_[position_];
      }      
    }
  };


  class StoreScuJob : public InstancesIteratorJob
  {
  private:
    ServerContext&                      context_;
    std::string                         localAet_;
    RemoteModalityParameters            remote_;
    bool                                permissive_;
    std::string                         moveOriginatorAet_;
    uint16_t                            moveOriginatorId_;
    std::auto_ptr<DicomUserConnection>  connection_;
    std::set<std::string>               failedInstances_;

    void Open()
    {
      if (connection_.get() == NULL)
      {
        connection_.reset(new DicomUserConnection);
        connection_->SetLocalApplicationEntityTitle(localAet_);
        connection_->SetRemoteModality(remote_);
        connection_->Open();
      }
    }
    
  public:
    StoreScuJob(ServerContext& context) :
      context_(context),
      localAet_("ORTHANC"),
      permissive_(false),
      moveOriginatorId_(0)  // By default, not a C-MOVE
    {
    }

    const std::string& GetLocalAet() const
    {
      return localAet_;
    }

    void SetLocalAet(const std::string& aet)
    {
      if (IsStarted())
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }
      else
      {
        localAet_ = aet;
      }
    }

    const RemoteModalityParameters& GetRemoteModality() const
    {
      return remote_;
    }

    void SetRemoteModality(const RemoteModalityParameters& remote)
    {
      if (IsStarted())
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }
      else
      {
        remote_ = remote;
      }
    }

    bool IsPermissive() const
    {
      return permissive_;
    }

    void SetPermissive(bool permissive)
    {
      if (IsStarted())
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }
      else
      {
        permissive_ = permissive;
      }
    }

    bool HasMoveOriginator() const
    {
      return moveOriginatorId_ != 0;
    }
    
    const std::string& GetMoveOriginatorAet() const
    {
      if (HasMoveOriginator())
      {
        return moveOriginatorAet_;
      }
      else
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }
    }
    
    uint16_t GetMoveOriginatorId() const
    {
      if (HasMoveOriginator())
      {
        return moveOriginatorId_;
      }
      else
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }
    }

    void SetMoveOriginator(const std::string& aet,
                           int id)
    {
      if (IsStarted())
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }
      else if (id < 0 || 
               id >= 65536)
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
      else
      {
        moveOriginatorId_ = static_cast<uint16_t>(id);
        moveOriginatorAet_ = aet;
      }
    }

    virtual JobStepResult* ExecuteStep()
    {
      if (IsDone())
      {
        return new JobStepResult(JobStepCode_Success);
      }

      Open();

      bool ok = false;
      
      try
      {
        std::string dicom;
        context_.ReadDicom(dicom, GetCurrentInstance());

        if (HasMoveOriginator())
        {
          connection_->Store(dicom, moveOriginatorAet_, moveOriginatorId_);
        }
        else
        {
          connection_->Store(dicom);
        }

        ok = true;
      }
      catch (OrthancException& e)
      {
      }

      if (!ok)
      {
        if (permissive_)
        {
          failedInstances_.insert(GetCurrentInstance());
        }
        else
        {
          return new JobStepResult(JobStepCode_Failure);
        }
      }

      Next();
      
      return new JobStepResult(IsDone() ? JobStepCode_Success : JobStepCode_Continue);
    }

    virtual void ReleaseResources()   // For pausing jobs
    {
      connection_.release();
    }

    virtual void GetDescription(Json::Value& value)
    {
      value["Type"] = "C-STORE";
      value["LocalAet"] = localAet_;
      
      Json::Value v;
      remote_.ToJson(v);
      value["Target"] = v;

      if (HasMoveOriginator())
      {
        value["MoveOriginatorAET"] = GetMoveOriginatorAet();
        value["MoveOriginatorID"] = GetMoveOriginatorId();
      }

      v = Json::arrayValue;
      for (std::set<std::string>::const_iterator it = failedInstances_.begin();
           it != failedInstances_.end(); ++it)
      {
        v.append(*it);
      }

      value["FailedInstances"] = v;
    }
  };
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

  if (1)
  {
    printf(">> %d\n", engine.GetRegistry().SubmitAndWait(new DummyJob(JobStepResult(Orthanc::JobStepCode_Failure)), rand() % 10));
  }

  boost::this_thread::sleep(boost::posix_time::milliseconds(100));

  
  engine.Stop();

  if (0)
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
