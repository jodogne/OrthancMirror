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

#include "../Core/JobsEngine/JobsEngine.h"
#include "../Core/MultiThreading/SharedMessageQueue.h"
#include "../Core/OrthancException.h"
#include "../Core/SystemToolbox.h"
#include "../Core/Toolbox.h"

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




class DummyJob : public Orthanc::IJob
{
private:
  bool         fails_;
  unsigned int count_;
  unsigned int steps_;

public:
  DummyJob() :
    fails_(false),
    count_(0),
    steps_(4)
  {
  }

  explicit DummyJob(bool fails) :
  fails_(fails),
    count_(0),
    steps_(4)
  {
  }

  virtual void Start()
  {
  }

  virtual void SignalResubmit()
  {
  }
    
  virtual JobStepResult ExecuteStep()
  {
    if (fails_)
    {
      return JobStepResult::Failure(ErrorCode_ParameterOutOfRange);
    }
    else if (count_ == steps_ - 1)
    {
      return JobStepResult::Success();
    }
    else
    {
      count_++;
      return JobStepResult::Continue();
    }
  }

  virtual void ReleaseResources()
  {
  }

  virtual float GetProgress()
  {
    return static_cast<float>(count_) / static_cast<float>(steps_ - 1);
  }

  virtual void GetJobType(std::string& type)
  {
    type = "DummyJob";
  }

  virtual void GetInternalContent(Json::Value& value)
  {
  }

  virtual void GetPublicContent(Json::Value& value)
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


static bool CheckErrorCode(Orthanc::JobsRegistry& registry,
                           const std::string& id,
                           Orthanc::ErrorCode code)
{
  Orthanc::JobInfo s;
  if (registry.GetJobInfo(s, id))
  {
    return code == s.GetStatus().GetErrorCode();
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


TEST(JobsRegistry, Cancel)
{
  JobsRegistry registry;

  std::string id;
  registry.Submit(id, new DummyJob(), 10);

  ASSERT_FALSE(registry.Cancel("nope"));

  ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Pending));
  ASSERT_TRUE(CheckErrorCode(registry, id, ErrorCode_Success));
            
  ASSERT_TRUE(registry.Cancel(id));
  ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Failure));
  ASSERT_TRUE(CheckErrorCode(registry, id, ErrorCode_CanceledJob));
  
  ASSERT_TRUE(registry.Cancel(id));
  ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Failure));
  ASSERT_TRUE(CheckErrorCode(registry, id, ErrorCode_CanceledJob));
  
  ASSERT_TRUE(registry.Resubmit(id));
  ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Pending));
  ASSERT_TRUE(CheckErrorCode(registry, id, ErrorCode_CanceledJob));
  
  {
    JobsRegistry::RunningJob job(registry, 0);
    ASSERT_TRUE(job.IsValid());

    ASSERT_TRUE(CheckErrorCode(registry, id, ErrorCode_Success));

    job.MarkSuccess();
    ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Running));
  }

  ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Success));
  ASSERT_TRUE(CheckErrorCode(registry, id, ErrorCode_Success));

  ASSERT_TRUE(registry.Cancel(id));
  ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Success));
  ASSERT_TRUE(CheckErrorCode(registry, id, ErrorCode_Success));

  registry.Submit(id, new DummyJob(), 10);

  {
    JobsRegistry::RunningJob job(registry, 0);
    ASSERT_TRUE(job.IsValid());
    ASSERT_EQ(id, job.GetId());

    ASSERT_TRUE(CheckErrorCode(registry, id, ErrorCode_Success));
    ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Running));

    job.MarkCanceled();
  }

  ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Failure));
  ASSERT_TRUE(CheckErrorCode(registry, id, ErrorCode_CanceledJob));

  ASSERT_TRUE(registry.Resubmit(id));
  ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Pending));
  ASSERT_TRUE(CheckErrorCode(registry, id, ErrorCode_CanceledJob));

  ASSERT_TRUE(registry.Pause(id));
  ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Paused));
  ASSERT_TRUE(CheckErrorCode(registry, id, ErrorCode_CanceledJob));

  ASSERT_TRUE(registry.Cancel(id));
  ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Failure));
  ASSERT_TRUE(CheckErrorCode(registry, id, ErrorCode_CanceledJob));

  ASSERT_TRUE(registry.Resubmit(id));
  ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Pending));
  ASSERT_TRUE(CheckErrorCode(registry, id, ErrorCode_CanceledJob));

  {
    JobsRegistry::RunningJob job(registry, 0);
    ASSERT_TRUE(job.IsValid());
    ASSERT_EQ(id, job.GetId());

    ASSERT_TRUE(CheckErrorCode(registry, id, ErrorCode_Success));
    ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Running));

    job.MarkRetry(500);
  }

  ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Retry));
  ASSERT_TRUE(CheckErrorCode(registry, id, ErrorCode_Success));

  ASSERT_TRUE(registry.Cancel(id));
  ASSERT_TRUE(CheckState(registry, id, Orthanc::JobState_Failure));
  ASSERT_TRUE(CheckErrorCode(registry, id, ErrorCode_CanceledJob));
}



TEST(JobsEngine, SubmitAndWait)
{
  JobsEngine engine;
  engine.SetWorkersCount(3);
  engine.Start();

  ASSERT_TRUE(engine.GetRegistry().SubmitAndWait(new DummyJob(), rand() % 10));
  ASSERT_FALSE(engine.GetRegistry().SubmitAndWait(new DummyJob(true), rand() % 10));

  engine.Stop();
}





#include "../OrthancServer/ServerJobs/LuaJobManager.h"
#include "../Core/JobsEngine/Operations/StringOperationValue.h"
#include "../Core/JobsEngine/Operations/LogJobOperation.h"


TEST(JobsEngine, DISABLED_SequenceOfOperationsJob)
{
  JobsEngine engine;
  engine.SetWorkersCount(3);
  engine.Start();

  std::string id;
  SequenceOfOperationsJob* job = NULL;

  {
    std::auto_ptr<SequenceOfOperationsJob> a(new SequenceOfOperationsJob);
    job = a.get();
    engine.GetRegistry().Submit(id, a.release(), 0);
  }

  boost::this_thread::sleep(boost::posix_time::milliseconds(500));

  {
    SequenceOfOperationsJob::Lock lock(*job);
    size_t i = lock.AddOperation(new LogJobOperation);
    size_t j = lock.AddOperation(new LogJobOperation);
    size_t k = lock.AddOperation(new LogJobOperation);
    lock.AddInput(i, StringOperationValue("Hello"));
    lock.AddInput(i, StringOperationValue("World"));
    lock.Connect(i, j);
    lock.Connect(j, k);
  }

  boost::this_thread::sleep(boost::posix_time::milliseconds(2000));

  engine.Stop();

}


TEST(JobsEngine, DISABLED_Lua)
{
  JobsEngine engine;
  engine.SetWorkersCount(2);
  engine.Start();

  LuaJobManager lua;
  lua.SetMaxOperationsPerJob(5);
  lua.SetTrailingOperationTimeout(200);

  for (size_t i = 0; i < 30; i++)
  {
    boost::this_thread::sleep(boost::posix_time::milliseconds(150));

    LuaJobManager::Lock lock(lua, engine);
    size_t a = lock.AddLogOperation();
    size_t b = lock.AddLogOperation();
    size_t c = lock.AddSystemCallOperation("echo");
    lock.AddStringInput(a, boost::lexical_cast<std::string>(i));
    lock.AddNullInput(a);
    lock.Connect(a, b);
    lock.Connect(a, c);
  }

  boost::this_thread::sleep(boost::posix_time::milliseconds(2000));

  engine.Stop();

}
