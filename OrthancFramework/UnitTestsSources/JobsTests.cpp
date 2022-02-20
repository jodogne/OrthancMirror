/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 **/


#if ORTHANC_UNIT_TESTS_LINK_FRAMEWORK == 1
// Must be the first to be sure to use the Orthanc framework shared library
#  include <OrthancFramework.h>
#endif

#include <gtest/gtest.h>

#include "../../OrthancFramework/Sources/Compatibility.h"
#include "../../OrthancFramework/Sources/DicomNetworking/DicomAssociationParameters.h"
#include "../../OrthancFramework/Sources/DicomNetworking/RemoteModalityParameters.h"
#include "../../OrthancFramework/Sources/DicomParsing/DicomModification.h"
#include "../../OrthancFramework/Sources/DicomParsing/ParsedDicomFile.h"
#include "../../OrthancFramework/Sources/JobsEngine/GenericJobUnserializer.h"
#include "../../OrthancFramework/Sources/JobsEngine/JobsEngine.h"
#include "../../OrthancFramework/Sources/JobsEngine/Operations/JobOperationValues.h"
#include "../../OrthancFramework/Sources/JobsEngine/Operations/LogJobOperation.h"
#include "../../OrthancFramework/Sources/JobsEngine/Operations/NullOperationValue.h"
#include "../../OrthancFramework/Sources/JobsEngine/Operations/SequenceOfOperationsJob.h"
#include "../../OrthancFramework/Sources/JobsEngine/Operations/StringOperationValue.h"
#include "../../OrthancFramework/Sources/JobsEngine/SetOfInstancesJob.h"
#include "../../OrthancFramework/Sources/Logging.h"
#include "../../OrthancFramework/Sources/MultiThreading/SharedMessageQueue.h"
#include "../../OrthancFramework/Sources/OrthancException.h"
#include "../../OrthancFramework/Sources/SerializationToolbox.h"


using namespace Orthanc;

namespace
{
  class DummyJob : public IJob
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

    virtual void Start() ORTHANC_OVERRIDE
    {
    }

    virtual void Reset() ORTHANC_OVERRIDE
    {
    }
    
    virtual JobStepResult Step(const std::string& jobId) ORTHANC_OVERRIDE
    {
      if (fails_)
      {
        return JobStepResult::Failure(ErrorCode_ParameterOutOfRange, NULL);
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

    virtual void Stop(JobStopReason reason) ORTHANC_OVERRIDE
    {
    }

    virtual float GetProgress() ORTHANC_OVERRIDE
    {
      return static_cast<float>(count_) / static_cast<float>(steps_ - 1);
    }

    virtual void GetJobType(std::string& type) ORTHANC_OVERRIDE
    {
      type = "DummyJob";
    }

    virtual bool Serialize(Json::Value& value) ORTHANC_OVERRIDE
    {
      value = Json::objectValue;
      value["Type"] = "DummyJob";
      return true;
    }

    virtual void GetPublicContent(Json::Value& value) ORTHANC_OVERRIDE
    {
      value["hello"] = "world";
    }

    virtual bool GetOutput(std::string& output,
                           MimeType& mime,
                           std::string& filename,
                           const std::string& key) ORTHANC_OVERRIDE
    {
      return false;
    }
  };


  class DummyInstancesJob : public SetOfInstancesJob
  {
  private:
    bool   trailingStepDone_;
    
  protected:
    virtual bool HandleInstance(const std::string& instance) ORTHANC_OVERRIDE
    {
      return (instance != "nope");
    }

    virtual bool HandleTrailingStep() ORTHANC_OVERRIDE
    {
      if (HasTrailingStep())
      {
        if (trailingStepDone_)
        {
          throw OrthancException(ErrorCode_InternalError);
        }
        else
        {
          trailingStepDone_ = true;
          return true;
        }
      }
      else
      {
        throw OrthancException(ErrorCode_InternalError);
      }
    }

  public:
    DummyInstancesJob() :
      trailingStepDone_(false)
    {
    }
    
    explicit DummyInstancesJob(const Json::Value& value) :
      SetOfInstancesJob(value)
    {
      if (HasTrailingStep())
      {
        trailingStepDone_ = (GetPosition() == GetCommandsCount());
      }
      else
      {
        trailingStepDone_ = false;
      }
    }

    bool IsTrailingStepDone() const
    {
      return trailingStepDone_;
    }
    
    virtual void Stop(JobStopReason reason) ORTHANC_OVERRIDE
    {
    }

    virtual void GetJobType(std::string& s) ORTHANC_OVERRIDE
    {
      s = "DummyInstancesJob";
    }
  };


  class DummyUnserializer : public GenericJobUnserializer
  {
  public:
    virtual IJob* UnserializeJob(const Json::Value& value) ORTHANC_OVERRIDE
    {
      if (SerializationToolbox::ReadString(value, "Type") == "DummyInstancesJob")
      {
        return new DummyInstancesJob(value);
      }
      else if (SerializationToolbox::ReadString(value, "Type") == "DummyJob")
      {
        return new DummyJob;
      }
      else
      {
        return GenericJobUnserializer::UnserializeJob(value);
      }
    }
  };

    
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

  std::unique_ptr<DynamicInteger> i;
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




static bool CheckState(JobsRegistry& registry,
                       const std::string& id,
                       JobState state)
{
  JobState s;
  if (registry.GetState(s, id))
  {
    return state == s;
  }
  else
  {
    return false;
  }
}


static bool CheckErrorCode(JobsRegistry& registry,
                           const std::string& id,
                           ErrorCode code)
{
  JobInfo s;
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
  JobsRegistry registry(10);

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

  ASSERT_TRUE(CheckState(registry, i2, JobState_Pending));

  {
    JobsRegistry::RunningJob job(registry, 0);
    ASSERT_TRUE(job.IsValid());
    ASSERT_EQ(30, job.GetPriority());
    ASSERT_EQ(i2, job.GetId());

    ASSERT_TRUE(CheckState(registry, i2, JobState_Running));
  }

  ASSERT_TRUE(CheckState(registry, i2, JobState_Failure));
  ASSERT_TRUE(CheckState(registry, i3, JobState_Pending));

  {
    JobsRegistry::RunningJob job(registry, 0);
    ASSERT_TRUE(job.IsValid());
    ASSERT_EQ(20, job.GetPriority());
    ASSERT_EQ(i3, job.GetId());

    job.MarkSuccess();

    ASSERT_TRUE(CheckState(registry, i3, JobState_Running));
  }

  ASSERT_TRUE(CheckState(registry, i3, JobState_Success));

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

  JobState s;
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
  JobsRegistry registry(10);

  std::string i1, i2;
  registry.Submit(i1, new DummyJob(), 20);
  registry.Submit(i2, new DummyJob(), 10);

  ASSERT_TRUE(CheckState(registry, i1, JobState_Pending));
  ASSERT_TRUE(CheckState(registry, i2, JobState_Pending));

  {
    JobsRegistry::RunningJob job1(registry, 0);
    JobsRegistry::RunningJob job2(registry, 0);

    ASSERT_TRUE(job1.IsValid());
    ASSERT_TRUE(job2.IsValid());

    job1.MarkFailure();
    job2.MarkSuccess();

    ASSERT_TRUE(CheckState(registry, i1, JobState_Running));
    ASSERT_TRUE(CheckState(registry, i2, JobState_Running));
  }

  ASSERT_TRUE(CheckState(registry, i1, JobState_Failure));
  ASSERT_TRUE(CheckState(registry, i2, JobState_Success));
}


TEST(JobsRegistry, Resubmit)
{
  JobsRegistry registry(10);

  std::string id;
  registry.Submit(id, new DummyJob(), 10);

  ASSERT_TRUE(CheckState(registry, id, JobState_Pending));

  registry.Resubmit(id);
  ASSERT_TRUE(CheckState(registry, id, JobState_Pending));

  {
    JobsRegistry::RunningJob job(registry, 0);
    ASSERT_TRUE(job.IsValid());
    job.MarkFailure();

    ASSERT_TRUE(CheckState(registry, id, JobState_Running));

    registry.Resubmit(id);
    ASSERT_TRUE(CheckState(registry, id, JobState_Running));
  }

  ASSERT_TRUE(CheckState(registry, id, JobState_Failure));

  registry.Resubmit(id);
  ASSERT_TRUE(CheckState(registry, id, JobState_Pending));

  {
    JobsRegistry::RunningJob job(registry, 0);
    ASSERT_TRUE(job.IsValid());
    ASSERT_EQ(id, job.GetId());

    job.MarkSuccess();
    ASSERT_TRUE(CheckState(registry, id, JobState_Running));
  }

  ASSERT_TRUE(CheckState(registry, id, JobState_Success));

  registry.Resubmit(id);
  ASSERT_TRUE(CheckState(registry, id, JobState_Success));
}


TEST(JobsRegistry, Retry)
{
  JobsRegistry registry(10);

  std::string id;
  registry.Submit(id, new DummyJob(), 10);

  ASSERT_TRUE(CheckState(registry, id, JobState_Pending));

  {
    JobsRegistry::RunningJob job(registry, 0);
    ASSERT_TRUE(job.IsValid());
    job.MarkRetry(0);

    ASSERT_TRUE(CheckState(registry, id, JobState_Running));
  }

  ASSERT_TRUE(CheckState(registry, id, JobState_Retry));

  registry.Resubmit(id);
  ASSERT_TRUE(CheckState(registry, id, JobState_Retry));
  
  registry.ScheduleRetries();
  ASSERT_TRUE(CheckState(registry, id, JobState_Pending));

  {
    JobsRegistry::RunningJob job(registry, 0);
    ASSERT_TRUE(job.IsValid());
    job.MarkSuccess();

    ASSERT_TRUE(CheckState(registry, id, JobState_Running));
  }

  ASSERT_TRUE(CheckState(registry, id, JobState_Success));
}


TEST(JobsRegistry, PausePending)
{
  JobsRegistry registry(10);

  std::string id;
  registry.Submit(id, new DummyJob(), 10);

  ASSERT_TRUE(CheckState(registry, id, JobState_Pending));

  registry.Pause(id);
  ASSERT_TRUE(CheckState(registry, id, JobState_Paused));

  registry.Pause(id);
  ASSERT_TRUE(CheckState(registry, id, JobState_Paused));

  registry.Resubmit(id);
  ASSERT_TRUE(CheckState(registry, id, JobState_Paused));

  registry.Resume(id);
  ASSERT_TRUE(CheckState(registry, id, JobState_Pending));
}


TEST(JobsRegistry, PauseRunning)
{
  JobsRegistry registry(10);

  std::string id;
  registry.Submit(id, new DummyJob(), 10);

  ASSERT_TRUE(CheckState(registry, id, JobState_Pending));

  {
    JobsRegistry::RunningJob job(registry, 0);
    ASSERT_TRUE(job.IsValid());

    registry.Resubmit(id);
    job.MarkPause();
    ASSERT_TRUE(CheckState(registry, id, JobState_Running));
  }

  ASSERT_TRUE(CheckState(registry, id, JobState_Paused));

  registry.Resubmit(id);
  ASSERT_TRUE(CheckState(registry, id, JobState_Paused));

  registry.Resume(id);
  ASSERT_TRUE(CheckState(registry, id, JobState_Pending));

  {
    JobsRegistry::RunningJob job(registry, 0);
    ASSERT_TRUE(job.IsValid());

    job.MarkSuccess();
    ASSERT_TRUE(CheckState(registry, id, JobState_Running));
  }

  ASSERT_TRUE(CheckState(registry, id, JobState_Success));
}


TEST(JobsRegistry, PauseRetry)
{
  JobsRegistry registry(10);

  std::string id;
  registry.Submit(id, new DummyJob(), 10);

  ASSERT_TRUE(CheckState(registry, id, JobState_Pending));

  {
    JobsRegistry::RunningJob job(registry, 0);
    ASSERT_TRUE(job.IsValid());

    job.MarkRetry(0);
    ASSERT_TRUE(CheckState(registry, id, JobState_Running));
  }

  ASSERT_TRUE(CheckState(registry, id, JobState_Retry));

  registry.Pause(id);
  ASSERT_TRUE(CheckState(registry, id, JobState_Paused));

  registry.Resume(id);
  ASSERT_TRUE(CheckState(registry, id, JobState_Pending));

  {
    JobsRegistry::RunningJob job(registry, 0);
    ASSERT_TRUE(job.IsValid());

    job.MarkSuccess();
    ASSERT_TRUE(CheckState(registry, id, JobState_Running));
  }

  ASSERT_TRUE(CheckState(registry, id, JobState_Success));
}


TEST(JobsRegistry, Cancel)
{
  JobsRegistry registry(10);

  std::string id;
  registry.Submit(id, new DummyJob(), 10);

  ASSERT_FALSE(registry.Cancel("nope"));

  ASSERT_TRUE(CheckState(registry, id, JobState_Pending));
  ASSERT_TRUE(CheckErrorCode(registry, id, ErrorCode_Success));
            
  ASSERT_TRUE(registry.Cancel(id));
  ASSERT_TRUE(CheckState(registry, id, JobState_Failure));
  ASSERT_TRUE(CheckErrorCode(registry, id, ErrorCode_CanceledJob));
  
  ASSERT_TRUE(registry.Cancel(id));
  ASSERT_TRUE(CheckState(registry, id, JobState_Failure));
  ASSERT_TRUE(CheckErrorCode(registry, id, ErrorCode_CanceledJob));
  
  ASSERT_TRUE(registry.Resubmit(id));
  ASSERT_TRUE(CheckState(registry, id, JobState_Pending));
  ASSERT_TRUE(CheckErrorCode(registry, id, ErrorCode_CanceledJob));
  
  {
    JobsRegistry::RunningJob job(registry, 0);
    ASSERT_TRUE(job.IsValid());

    ASSERT_TRUE(CheckErrorCode(registry, id, ErrorCode_Success));

    job.MarkSuccess();
    ASSERT_TRUE(CheckState(registry, id, JobState_Running));
  }

  ASSERT_TRUE(CheckState(registry, id, JobState_Success));
  ASSERT_TRUE(CheckErrorCode(registry, id, ErrorCode_Success));

  ASSERT_TRUE(registry.Cancel(id));
  ASSERT_TRUE(CheckState(registry, id, JobState_Success));
  ASSERT_TRUE(CheckErrorCode(registry, id, ErrorCode_Success));

  registry.Submit(id, new DummyJob(), 10);

  {
    JobsRegistry::RunningJob job(registry, 0);
    ASSERT_TRUE(job.IsValid());
    ASSERT_EQ(id, job.GetId());

    ASSERT_TRUE(CheckErrorCode(registry, id, ErrorCode_Success));
    ASSERT_TRUE(CheckState(registry, id, JobState_Running));

    job.MarkCanceled();
  }

  ASSERT_TRUE(CheckState(registry, id, JobState_Failure));
  ASSERT_TRUE(CheckErrorCode(registry, id, ErrorCode_CanceledJob));

  ASSERT_TRUE(registry.Resubmit(id));
  ASSERT_TRUE(CheckState(registry, id, JobState_Pending));
  ASSERT_TRUE(CheckErrorCode(registry, id, ErrorCode_CanceledJob));

  ASSERT_TRUE(registry.Pause(id));
  ASSERT_TRUE(CheckState(registry, id, JobState_Paused));
  ASSERT_TRUE(CheckErrorCode(registry, id, ErrorCode_CanceledJob));

  ASSERT_TRUE(registry.Cancel(id));
  ASSERT_TRUE(CheckState(registry, id, JobState_Failure));
  ASSERT_TRUE(CheckErrorCode(registry, id, ErrorCode_CanceledJob));

  ASSERT_TRUE(registry.Resubmit(id));
  ASSERT_TRUE(CheckState(registry, id, JobState_Pending));
  ASSERT_TRUE(CheckErrorCode(registry, id, ErrorCode_CanceledJob));

  {
    JobsRegistry::RunningJob job(registry, 0);
    ASSERT_TRUE(job.IsValid());
    ASSERT_EQ(id, job.GetId());

    ASSERT_TRUE(CheckErrorCode(registry, id, ErrorCode_Success));
    ASSERT_TRUE(CheckState(registry, id, JobState_Running));

    job.MarkRetry(500);
  }

  ASSERT_TRUE(CheckState(registry, id, JobState_Retry));
  ASSERT_TRUE(CheckErrorCode(registry, id, ErrorCode_Success));

  ASSERT_TRUE(registry.Cancel(id));
  ASSERT_TRUE(CheckState(registry, id, JobState_Failure));
  ASSERT_TRUE(CheckErrorCode(registry, id, ErrorCode_CanceledJob));
}



TEST(JobsEngine, SubmitAndWait)
{
  JobsEngine engine(10);
  engine.SetThreadSleep(10);
  engine.SetWorkersCount(3);
  engine.Start();

  Json::Value content = Json::nullValue;
  engine.GetRegistry().SubmitAndWait(content, new DummyJob(), rand() % 10);
  ASSERT_EQ(Json::objectValue, content.type());
  ASSERT_EQ("world", content["hello"].asString());

  content = Json::nullValue;
  ASSERT_THROW(engine.GetRegistry().SubmitAndWait(content, new DummyJob(true), rand() % 10), OrthancException);
  ASSERT_EQ(Json::nullValue, content.type());

  engine.Stop();
}


TEST(JobsEngine, DISABLED_SequenceOfOperationsJob)
{
  JobsEngine engine(10);
  engine.SetThreadSleep(10);
  engine.SetWorkersCount(3);
  engine.Start();

  std::string id;
  SequenceOfOperationsJob* job = NULL;

  {
    std::unique_ptr<SequenceOfOperationsJob> a(new SequenceOfOperationsJob);
    job = a.get();
    engine.GetRegistry().Submit(id, a.release(), 0);
  }

  boost::this_thread::sleep(boost::posix_time::milliseconds(500));

  {
    SequenceOfOperationsJob::Lock lock(*job);
    size_t i = lock.AddOperation(new LogJobOperation);
    size_t j = lock.AddOperation(new LogJobOperation);
    size_t k = lock.AddOperation(new LogJobOperation);

    StringOperationValue a("Hello");
    StringOperationValue b("World");
    lock.AddInput(i, a);
    lock.AddInput(i, b);
    
    lock.Connect(i, j);
    lock.Connect(j, k);
  }

  boost::this_thread::sleep(boost::posix_time::milliseconds(2000));

  engine.Stop();

}


static bool CheckSameJson(const Json::Value& a,
                          const Json::Value& b)
{
  std::string s = a.toStyledString();
  std::string t = b.toStyledString();

  if (s == t)
  {
    return true;
  }
  else
  {
    LOG(ERROR) << "Expected serialization: " << s;
    LOG(ERROR) << "Actual serialization: " << t;
    return false;
  }
}


static bool CheckIdempotentSerialization(IJobUnserializer& unserializer,
                                         IJob& job)
{
  Json::Value a = 42;
  
  if (!job.Serialize(a))
  {
    return false;
  }
  else
  {
    std::unique_ptr<IJob> unserialized(unserializer.UnserializeJob(a));
  
    Json::Value b = 43;
    if (unserialized->Serialize(b))
    {
      return (CheckSameJson(a, b));
    }
    else
    {
      return false;
    }
  }
}


static bool CheckIdempotentSetOfInstances(IJobUnserializer& unserializer,
                                          SetOfInstancesJob& job)
{
  Json::Value a = 42;
  
  if (!job.Serialize(a))
  {
    return false;
  }
  else
  {
    std::unique_ptr<SetOfInstancesJob> unserialized
      (dynamic_cast<SetOfInstancesJob*>(unserializer.UnserializeJob(a)));
  
    Json::Value b = 43;
    if (unserialized->Serialize(b))
    {    
      return (CheckSameJson(a, b) &&
              job.HasTrailingStep() == unserialized->HasTrailingStep() &&
              job.GetPosition() == unserialized->GetPosition() &&
              job.GetInstancesCount() == unserialized->GetInstancesCount() &&
              job.GetCommandsCount() == unserialized->GetCommandsCount());
    }
    else
    {
      return false;
    }
  }
}


static bool CheckIdempotentSerialization(IJobUnserializer& unserializer,
                                         const IJobOperationValue& value)
{
  Json::Value a = 42;
  value.Serialize(a);
  
  std::unique_ptr<IJobOperationValue> unserialized(unserializer.UnserializeValue(a));
  
  Json::Value b = 43;
  unserialized->Serialize(b);

  return CheckSameJson(a, b);
}


TEST(JobsSerialization, BadFileFormat)
{
  GenericJobUnserializer unserializer;

  Json::Value s;

  s = Json::objectValue;
  ASSERT_THROW(unserializer.UnserializeValue(s), OrthancException);
  ASSERT_THROW(unserializer.UnserializeJob(s), OrthancException);
  ASSERT_THROW(unserializer.UnserializeOperation(s), OrthancException);

  s = Json::arrayValue;
  ASSERT_THROW(unserializer.UnserializeValue(s), OrthancException);
  ASSERT_THROW(unserializer.UnserializeJob(s), OrthancException);
  ASSERT_THROW(unserializer.UnserializeOperation(s), OrthancException);

  s = "hello";
  ASSERT_THROW(unserializer.UnserializeValue(s), OrthancException);
  ASSERT_THROW(unserializer.UnserializeJob(s), OrthancException);
  ASSERT_THROW(unserializer.UnserializeOperation(s), OrthancException);

  s = 42;
  ASSERT_THROW(unserializer.UnserializeValue(s), OrthancException);
  ASSERT_THROW(unserializer.UnserializeJob(s), OrthancException);
  ASSERT_THROW(unserializer.UnserializeOperation(s), OrthancException);
}


TEST(JobsSerialization, JobOperationValues)
{
  Json::Value s;

  {
    JobOperationValues values;
    values.Append(new NullOperationValue);
    values.Append(new StringOperationValue("hello"));
    values.Append(new StringOperationValue("world"));

    s = 42;
    values.Serialize(s);
  }

  {
    GenericJobUnserializer unserializer;
    std::unique_ptr<JobOperationValues> values(JobOperationValues::Unserialize(unserializer, s));
    ASSERT_EQ(3u, values->GetSize());
    ASSERT_EQ(IJobOperationValue::Type_Null, values->GetValue(0).GetType());
    ASSERT_EQ(IJobOperationValue::Type_String, values->GetValue(1).GetType());
    ASSERT_EQ(IJobOperationValue::Type_String, values->GetValue(2).GetType());

    ASSERT_EQ("hello", dynamic_cast<const StringOperationValue&>(values->GetValue(1)).GetContent());
    ASSERT_EQ("world", dynamic_cast<const StringOperationValue&>(values->GetValue(2)).GetContent());
  }
}


TEST(JobsSerialization, GenericValues)
{
  GenericJobUnserializer unserializer;
  Json::Value s;

  {
    NullOperationValue null;

    ASSERT_TRUE(CheckIdempotentSerialization(unserializer, null));
    null.Serialize(s);
  }

  ASSERT_THROW(unserializer.UnserializeJob(s), OrthancException);
  ASSERT_THROW(unserializer.UnserializeOperation(s), OrthancException);

  std::unique_ptr<IJobOperationValue> value;
  value.reset(unserializer.UnserializeValue(s));
  
  ASSERT_EQ(IJobOperationValue::Type_Null, value->GetType());

  {
    StringOperationValue str("Hello");

    ASSERT_TRUE(CheckIdempotentSerialization(unserializer, str));
    str.Serialize(s);
  }

  ASSERT_THROW(unserializer.UnserializeJob(s), OrthancException);
  ASSERT_THROW(unserializer.UnserializeOperation(s), OrthancException);
  value.reset(unserializer.UnserializeValue(s));

  ASSERT_EQ(IJobOperationValue::Type_String, value->GetType());
  ASSERT_EQ("Hello", dynamic_cast<StringOperationValue&>(*value).GetContent());
}


TEST(JobsSerialization, GenericJobs)
{   
  Json::Value s;

  // This tests SetOfInstancesJob
  
  {
    DummyInstancesJob job;
    job.SetDescription("description");
    job.AddInstance("hello");
    job.AddInstance("nope");
    job.AddInstance("world");
    job.SetPermissive(true);
    ASSERT_THROW(job.Step("jobId"), OrthancException);  // Not started yet
    ASSERT_FALSE(job.HasTrailingStep());
    ASSERT_FALSE(job.IsTrailingStepDone());
    job.Start();
    ASSERT_EQ(JobStepCode_Continue, job.Step("jobId").GetCode());
    ASSERT_EQ(JobStepCode_Continue, job.Step("jobId").GetCode());

    {
      DummyUnserializer unserializer;
      ASSERT_TRUE(CheckIdempotentSetOfInstances(unserializer, job));
    }
    
    ASSERT_TRUE(job.Serialize(s));
  }

  {
    DummyUnserializer unserializer;
    ASSERT_THROW(unserializer.UnserializeValue(s), OrthancException);
    ASSERT_THROW(unserializer.UnserializeOperation(s), OrthancException);

    std::unique_ptr<IJob> job;
    job.reset(unserializer.UnserializeJob(s));

    const DummyInstancesJob& tmp = dynamic_cast<const DummyInstancesJob&>(*job);
    ASSERT_FALSE(tmp.IsStarted());
    ASSERT_TRUE(tmp.IsPermissive());
    ASSERT_EQ("description", tmp.GetDescription());
    ASSERT_EQ(3u, tmp.GetInstancesCount());
    ASSERT_EQ(2u, tmp.GetPosition());
    ASSERT_EQ(1u, tmp.GetFailedInstances().size());
    ASSERT_EQ("hello", tmp.GetInstance(0));
    ASSERT_EQ("nope", tmp.GetInstance(1));
    ASSERT_EQ("world", tmp.GetInstance(2));
    ASSERT_TRUE(tmp.IsFailedInstance("nope"));
  }

  // SequenceOfOperationsJob

  {
    SequenceOfOperationsJob job;
    job.SetDescription("hello");

    {
      SequenceOfOperationsJob::Lock lock(job);
      size_t a = lock.AddOperation(new LogJobOperation);
      size_t b = lock.AddOperation(new LogJobOperation);
      lock.Connect(a, b);

      StringOperationValue s1("hello");
      StringOperationValue s2("world");
      lock.AddInput(a, s1);
      lock.AddInput(a, s2);
      lock.SetTrailingOperationTimeout(300);
    }

    ASSERT_EQ(JobStepCode_Continue, job.Step("jobId").GetCode());

    {
      GenericJobUnserializer unserializer;
      ASSERT_TRUE(CheckIdempotentSerialization(unserializer, job));
    }
    
    ASSERT_TRUE(job.Serialize(s));
  }

  {
    GenericJobUnserializer unserializer;
    ASSERT_THROW(unserializer.UnserializeValue(s), OrthancException);
    ASSERT_THROW(unserializer.UnserializeOperation(s), OrthancException);

    std::unique_ptr<IJob> job;
    job.reset(unserializer.UnserializeJob(s));

    std::string tmp;
    dynamic_cast<SequenceOfOperationsJob&>(*job).GetDescription(tmp);
    ASSERT_EQ("hello", tmp);
  }  
}


static bool IsSameTagValue(const ParsedDicomFile& dicom1,
                           const ParsedDicomFile& dicom2,
                           DicomTag tag)
{
  std::string a, b;
  return (dicom1.GetTagValue(a, tag) &&
          dicom2.GetTagValue(b, tag) &&
          (a == b));
}
                       


TEST(JobsSerialization, DicomModification)
{   
  Json::Value s;

  ParsedDicomFile source(true);
  source.Insert(DICOM_TAG_STUDY_DESCRIPTION, "Test 1", false, "");
  source.Insert(DICOM_TAG_SERIES_DESCRIPTION, "Test 2", false, "");
  source.Insert(DICOM_TAG_PATIENT_NAME, "Test 3", false, "");

  std::unique_ptr<ParsedDicomFile> modified(source.Clone(true));

  {
    DicomModification modification;
    modification.SetLevel(ResourceType_Series);
    modification.Clear(DICOM_TAG_STUDY_DESCRIPTION);
    modification.Remove(DICOM_TAG_SERIES_DESCRIPTION);
    modification.Replace(DICOM_TAG_PATIENT_NAME, "Test 4", true);

    modification.Apply(*modified);

    s = 42;
    modification.Serialize(s);
  }

  {
    DicomModification modification(s);
    ASSERT_EQ(ResourceType_Series, modification.GetLevel());
    
    std::unique_ptr<ParsedDicomFile> second(source.Clone(true));
    modification.Apply(*second);

    std::string t;
    ASSERT_TRUE(second->GetTagValue(t, DICOM_TAG_STUDY_DESCRIPTION));
    ASSERT_TRUE(t.empty());
    ASSERT_FALSE(second->GetTagValue(t, DICOM_TAG_SERIES_DESCRIPTION));
    ASSERT_TRUE(second->GetTagValue(t, DICOM_TAG_PATIENT_NAME));
    ASSERT_EQ("Test 4", t);

    ASSERT_TRUE(IsSameTagValue(source, *modified, DICOM_TAG_STUDY_INSTANCE_UID));
    ASSERT_TRUE(IsSameTagValue(source, *second, DICOM_TAG_STUDY_INSTANCE_UID));

    ASSERT_FALSE(IsSameTagValue(source, *second, DICOM_TAG_SERIES_INSTANCE_UID));
    ASSERT_TRUE(IsSameTagValue(*modified, *second, DICOM_TAG_SERIES_INSTANCE_UID));
  }
}


TEST(JobsSerialization, DicomModification2)
{   
  Json::Value s;

  {
    DicomModification modification;
    modification.SetupAnonymization(DicomVersion_2017c);
    modification.Remove(DicomPath(DICOM_TAG_REFERENCED_IMAGE_SEQUENCE, 1, DICOM_TAG_SOP_INSTANCE_UID));
    modification.Replace(DicomPath(DICOM_TAG_REFERENCED_IMAGE_SEQUENCE, 1, DICOM_TAG_SOP_CLASS_UID), "Hello", true);
    modification.Keep(DicomPath(DICOM_TAG_REFERENCED_IMAGE_SEQUENCE, 1, DICOM_TAG_PATIENT_NAME));

    s = 42;
    modification.Serialize(s);
  }

  {
    DicomModification modification(s);

    // Check idempotent serialization
    Json::Value ss;
    modification.Serialize(ss);
    ASSERT_EQ(s.toStyledString(), ss.toStyledString());
  }
}


TEST(JobsSerialization, Registry)
{   
  Json::Value s;
  std::string i1, i2;

  {
    JobsRegistry registry(10);
    registry.Submit(i1, new DummyJob(), 10);
    registry.Submit(i2, new SequenceOfOperationsJob(), 30);
    registry.Serialize(s);
  }

  {
    DummyUnserializer unserializer;
    JobsRegistry registry(unserializer, s, 10);

    Json::Value t;
    registry.Serialize(t);
    ASSERT_TRUE(CheckSameJson(s, t));
  }
}


TEST(JobsSerialization, TrailingStep)
{
  {
    Json::Value s;
    
    DummyInstancesJob job;
    ASSERT_EQ(0u, job.GetCommandsCount());
    ASSERT_EQ(0u, job.GetInstancesCount());

    job.Start();
    ASSERT_EQ(0u, job.GetPosition());
    ASSERT_FALSE(job.HasTrailingStep());
    ASSERT_FALSE(job.IsTrailingStepDone());

    {
      DummyUnserializer unserializer;
      ASSERT_TRUE(CheckIdempotentSetOfInstances(unserializer, job));
    }
    
    ASSERT_EQ(JobStepCode_Success, job.Step("jobId").GetCode());
    ASSERT_EQ(1u, job.GetPosition());
    ASSERT_FALSE(job.IsTrailingStepDone());
    
    {
      DummyUnserializer unserializer;
      ASSERT_TRUE(CheckIdempotentSetOfInstances(unserializer, job));
    }

    ASSERT_THROW(job.Step("jobId"), OrthancException);
  }

  {
    Json::Value s;
    
    DummyInstancesJob job;
    job.AddInstance("hello");
    job.AddInstance("world");
    ASSERT_EQ(2u, job.GetCommandsCount());
    ASSERT_EQ(2u, job.GetInstancesCount());

    job.Start();
    ASSERT_EQ(0u, job.GetPosition());
    ASSERT_FALSE(job.HasTrailingStep());
    ASSERT_FALSE(job.IsTrailingStepDone());

    {
      DummyUnserializer unserializer;
      ASSERT_TRUE(CheckIdempotentSetOfInstances(unserializer, job));
    }
    
    ASSERT_EQ(JobStepCode_Continue, job.Step("jobId").GetCode());
    ASSERT_EQ(1u, job.GetPosition());
    ASSERT_FALSE(job.IsTrailingStepDone());
    
    {
      DummyUnserializer unserializer;
      ASSERT_TRUE(CheckIdempotentSetOfInstances(unserializer, job));
    }

    ASSERT_EQ(JobStepCode_Success, job.Step("jobId").GetCode());
    ASSERT_EQ(2u, job.GetPosition());
    ASSERT_FALSE(job.IsTrailingStepDone());
    
    {
      DummyUnserializer unserializer;
      ASSERT_TRUE(CheckIdempotentSetOfInstances(unserializer, job));
    }

    ASSERT_THROW(job.Step("jobId"), OrthancException);
  }

  {
    Json::Value s;
    
    DummyInstancesJob job;
    ASSERT_EQ(0u, job.GetInstancesCount());
    ASSERT_EQ(0u, job.GetCommandsCount());
    job.AddTrailingStep();
    ASSERT_EQ(0u, job.GetInstancesCount());
    ASSERT_EQ(1u, job.GetCommandsCount());

    job.Start(); // This adds the trailing step
    ASSERT_EQ(0u, job.GetPosition());
    ASSERT_TRUE(job.HasTrailingStep());
    ASSERT_FALSE(job.IsTrailingStepDone());

    {
      DummyUnserializer unserializer;
      ASSERT_TRUE(CheckIdempotentSetOfInstances(unserializer, job));
    }
    
    ASSERT_EQ(JobStepCode_Success, job.Step("jobId").GetCode());
    ASSERT_EQ(1u, job.GetPosition());
    ASSERT_TRUE(job.IsTrailingStepDone());
    
    {
      DummyUnserializer unserializer;
      ASSERT_TRUE(CheckIdempotentSetOfInstances(unserializer, job));
    }

    ASSERT_THROW(job.Step("jobId"), OrthancException);
  }

  {
    Json::Value s;
    
    DummyInstancesJob job;
    job.AddInstance("hello");
    ASSERT_EQ(1u, job.GetInstancesCount());
    ASSERT_EQ(1u, job.GetCommandsCount());
    job.AddTrailingStep();
    ASSERT_EQ(1u, job.GetInstancesCount());
    ASSERT_EQ(2u, job.GetCommandsCount());
    
    job.Start();
    ASSERT_EQ(2u, job.GetCommandsCount());
    ASSERT_EQ(0u, job.GetPosition());
    ASSERT_TRUE(job.HasTrailingStep());
    ASSERT_FALSE(job.IsTrailingStepDone());

    {
      DummyUnserializer unserializer;
      ASSERT_TRUE(CheckIdempotentSetOfInstances(unserializer, job));
    }
    
    ASSERT_EQ(JobStepCode_Continue, job.Step("jobId").GetCode());
    ASSERT_EQ(1u, job.GetPosition());
    ASSERT_FALSE(job.IsTrailingStepDone());
    
    {
      DummyUnserializer unserializer;
      ASSERT_TRUE(CheckIdempotentSetOfInstances(unserializer, job));
    }

    ASSERT_EQ(JobStepCode_Success, job.Step("jobId").GetCode());
    ASSERT_EQ(2u, job.GetPosition());
    ASSERT_TRUE(job.IsTrailingStepDone());
    
    {
      DummyUnserializer unserializer;
      ASSERT_TRUE(CheckIdempotentSetOfInstances(unserializer, job));
    }

    ASSERT_THROW(job.Step("jobId"), OrthancException);
  }
}


TEST(JobsSerialization, RemoteModalityParameters)
{
  Json::Value s;

  {
    RemoteModalityParameters modality;
    ASSERT_FALSE(modality.IsAdvancedFormatNeeded());
    modality.Serialize(s, false);
    ASSERT_EQ(Json::arrayValue, s.type());
    ASSERT_FALSE(modality.IsDicomTlsEnabled());
    ASSERT_FALSE(modality.HasTimeout());
    ASSERT_EQ(0u, modality.GetTimeout());
  }

  {
    RemoteModalityParameters modality(s);
    ASSERT_FALSE(modality.IsAdvancedFormatNeeded());
    ASSERT_EQ("ORTHANC", modality.GetApplicationEntityTitle());
    ASSERT_EQ("127.0.0.1", modality.GetHost());
    ASSERT_EQ(104u, modality.GetPortNumber());
    ASSERT_EQ(ModalityManufacturer_Generic, modality.GetManufacturer());
    ASSERT_TRUE(modality.IsRequestAllowed(DicomRequestType_Echo));
    ASSERT_TRUE(modality.IsRequestAllowed(DicomRequestType_Find));
    ASSERT_TRUE(modality.IsRequestAllowed(DicomRequestType_FindWorklist));
    ASSERT_TRUE(modality.IsRequestAllowed(DicomRequestType_Get));
    ASSERT_TRUE(modality.IsRequestAllowed(DicomRequestType_Store));
    ASSERT_TRUE(modality.IsRequestAllowed(DicomRequestType_Move));
    ASSERT_TRUE(modality.IsRequestAllowed(DicomRequestType_NAction));
    ASSERT_TRUE(modality.IsRequestAllowed(DicomRequestType_NEventReport));
    ASSERT_TRUE(modality.IsTranscodingAllowed());
    ASSERT_FALSE(modality.IsDicomTlsEnabled());
    ASSERT_FALSE(modality.HasLocalAet());
    ASSERT_THROW(modality.GetLocalAet(), OrthancException);
    ASSERT_FALSE(modality.HasTimeout());
    ASSERT_EQ(0u, modality.GetTimeout());
  }

  s = Json::nullValue;

  {
    RemoteModalityParameters modality;
    ASSERT_THROW(modality.SetPortNumber(0), OrthancException);
    ASSERT_THROW(modality.SetPortNumber(65535), OrthancException);
    modality.SetApplicationEntityTitle("HELLO");
    modality.SetHost("world");
    modality.SetPortNumber(45);
    modality.SetManufacturer(ModalityManufacturer_GenericNoWildcardInDates);
    ASSERT_FALSE(modality.IsAdvancedFormatNeeded());
    modality.Serialize(s, true);
    ASSERT_EQ(Json::objectValue, s.type());
    ASSERT_FALSE(modality.HasLocalAet());
    ASSERT_FALSE(modality.HasTimeout());
    ASSERT_EQ(0u, modality.GetTimeout());
  }

  {
    RemoteModalityParameters modality(s);
    ASSERT_EQ("HELLO", modality.GetApplicationEntityTitle());
    ASSERT_EQ("world", modality.GetHost());
    ASSERT_EQ(45u, modality.GetPortNumber());
    ASSERT_EQ(ModalityManufacturer_GenericNoWildcardInDates, modality.GetManufacturer());
    ASSERT_TRUE(modality.IsRequestAllowed(DicomRequestType_Echo));
    ASSERT_TRUE(modality.IsRequestAllowed(DicomRequestType_Find));
    ASSERT_TRUE(modality.IsRequestAllowed(DicomRequestType_FindWorklist));
    ASSERT_TRUE(modality.IsRequestAllowed(DicomRequestType_Get));
    ASSERT_TRUE(modality.IsRequestAllowed(DicomRequestType_Store));
    ASSERT_TRUE(modality.IsRequestAllowed(DicomRequestType_Move));
    ASSERT_TRUE(modality.IsRequestAllowed(DicomRequestType_NAction));
    ASSERT_TRUE(modality.IsRequestAllowed(DicomRequestType_NEventReport));
    ASSERT_TRUE(modality.IsTranscodingAllowed());
    ASSERT_FALSE(modality.IsDicomTlsEnabled());
    ASSERT_FALSE(modality.HasLocalAet());
    ASSERT_FALSE(modality.HasTimeout());
    ASSERT_EQ(0u, modality.GetTimeout());
  }

  s["Port"] = "46";
  
  {
    RemoteModalityParameters modality(s);
    ASSERT_EQ(46u, modality.GetPortNumber());
  }

  s["Port"] = -1;     ASSERT_THROW(RemoteModalityParameters m(s), OrthancException);
  s["Port"] = 65535;  ASSERT_THROW(RemoteModalityParameters m(s), OrthancException);
  s["Port"] = "nope"; ASSERT_THROW(RemoteModalityParameters m(s), OrthancException);

  std::set<DicomRequestType> operations;
  operations.insert(DicomRequestType_Echo);
  operations.insert(DicomRequestType_Find);
  operations.insert(DicomRequestType_FindWorklist);
  operations.insert(DicomRequestType_Get);
  operations.insert(DicomRequestType_Move);
  operations.insert(DicomRequestType_Store);
  operations.insert(DicomRequestType_NAction);
  operations.insert(DicomRequestType_NEventReport);

  ASSERT_EQ(8u, operations.size());

  for (std::set<DicomRequestType>::const_iterator 
         it = operations.begin(); it != operations.end(); ++it)
  {
    {
      RemoteModalityParameters modality;
      modality.SetRequestAllowed(*it, false);
      ASSERT_TRUE(modality.IsAdvancedFormatNeeded());

      modality.Serialize(s, false);
      ASSERT_EQ(Json::objectValue, s.type());
    }

    {
      RemoteModalityParameters modality(s);

      ASSERT_FALSE(modality.IsRequestAllowed(*it));

      for (std::set<DicomRequestType>::const_iterator 
             it2 = operations.begin(); it2 != operations.end(); ++it2)
      {
        if (*it2 != *it)
        {
          ASSERT_TRUE(modality.IsRequestAllowed(*it2));
        }
      }
    }
  }

  s = Json::nullValue;

  {
    RemoteModalityParameters modality;
    modality.SetLocalAet("hello");
    modality.SetTimeout(42);
    ASSERT_TRUE(modality.IsAdvancedFormatNeeded());
    modality.Serialize(s, true);
    ASSERT_EQ(Json::objectValue, s.type());
    ASSERT_TRUE(modality.HasLocalAet());
    ASSERT_TRUE(modality.HasTimeout());
    ASSERT_EQ(42u, modality.GetTimeout());
  }

  {
    RemoteModalityParameters modality(s);
    ASSERT_TRUE(modality.HasLocalAet());
    ASSERT_EQ("hello", modality.GetLocalAet());
    ASSERT_TRUE(modality.HasTimeout());
    ASSERT_EQ(42u, modality.GetTimeout());
  }

  {
    Json::Value t;
    t["AllowStorageCommitment"] = false;
    t["AET"] = "AET";
    t["Host"] = "host";
    t["Port"] = "104";
    
    RemoteModalityParameters modality(t);
    ASSERT_TRUE(modality.IsAdvancedFormatNeeded());
    ASSERT_EQ("AET", modality.GetApplicationEntityTitle());
    ASSERT_EQ("host", modality.GetHost());
    ASSERT_EQ(104u, modality.GetPortNumber());
    ASSERT_FALSE(modality.IsRequestAllowed(DicomRequestType_NAction));
    ASSERT_FALSE(modality.IsRequestAllowed(DicomRequestType_NEventReport));
    ASSERT_TRUE(modality.IsTranscodingAllowed());
    ASSERT_FALSE(modality.IsDicomTlsEnabled());
    ASSERT_FALSE(modality.HasLocalAet());
    ASSERT_THROW(modality.GetLocalAet(), OrthancException);
    ASSERT_FALSE(modality.HasTimeout());
    ASSERT_EQ(0u, modality.GetTimeout());
  }

  {
    Json::Value t;
    t["AllowNAction"] = false;
    t["AllowNEventReport"] = true;
    t["AET"] = "AET";
    t["Host"] = "host";
    t["Port"] = "104";
    t["AllowTranscoding"] = false;
    t["UseDicomTls"] = true;
    t["LocalAet"] = "world";
    t["Timeout"] = 20;
    
    RemoteModalityParameters modality(t);
    ASSERT_TRUE(modality.IsAdvancedFormatNeeded());
    ASSERT_EQ("AET", modality.GetApplicationEntityTitle());
    ASSERT_EQ("host", modality.GetHost());
    ASSERT_EQ(104u, modality.GetPortNumber());
    ASSERT_FALSE(modality.IsRequestAllowed(DicomRequestType_NAction));
    ASSERT_TRUE(modality.IsRequestAllowed(DicomRequestType_NEventReport));
    ASSERT_FALSE(modality.IsTranscodingAllowed());
    ASSERT_TRUE(modality.IsDicomTlsEnabled());
    ASSERT_TRUE(modality.HasLocalAet());
    ASSERT_EQ("world", modality.GetLocalAet());
    ASSERT_TRUE(modality.HasTimeout());
    ASSERT_EQ(20u, modality.GetTimeout());
  }

  {
    Json::Value t;
    t["AllowNAction"] = true;
    t["AllowNEventReport"] = true;
    t["AET"] = "AET";
    t["Host"] = "host";
    t["Port"] = "104";
    
    RemoteModalityParameters modality(t);
    ASSERT_FALSE(modality.IsAdvancedFormatNeeded());
    ASSERT_EQ("AET", modality.GetApplicationEntityTitle());
    ASSERT_EQ("host", modality.GetHost());
    ASSERT_EQ(104u, modality.GetPortNumber());
    ASSERT_TRUE(modality.IsRequestAllowed(DicomRequestType_NAction));
    ASSERT_TRUE(modality.IsRequestAllowed(DicomRequestType_NEventReport));
    ASSERT_TRUE(modality.IsTranscodingAllowed());
    ASSERT_FALSE(modality.IsDicomTlsEnabled());
    ASSERT_FALSE(modality.HasLocalAet());
    ASSERT_THROW(modality.GetLocalAet(), OrthancException);
  }
}



TEST(JobsSerialization, DicomAssociationParameters)
{
  {
    DicomAssociationParameters a;

    Json::Value v = Json::objectValue;
    a.SerializeJob(v);
    ASSERT_EQ(Json::objectValue, v.type());
    ASSERT_EQ("ORTHANC", v["LocalAet"].asString());
    ASSERT_EQ(DicomAssociationParameters::GetDefaultTimeout(), v["Timeout"].asUInt());
    ASSERT_TRUE(v.isMember("Remote"));
    ASSERT_TRUE(v.isMember("MaximumPduLength"));

    ASSERT_EQ(5u, v.getMemberNames().size());
  
    DicomAssociationParameters b;
    b.UnserializeJob(v);
    ASSERT_EQ("ANY-SCP", b.GetRemoteModality().GetApplicationEntityTitle());
    ASSERT_EQ("127.0.0.1", b.GetRemoteModality().GetHost());
    ASSERT_EQ(104u, b.GetRemoteModality().GetPortNumber());
    ASSERT_EQ("ORTHANC", b.GetLocalApplicationEntityTitle());
    ASSERT_EQ(DicomAssociationParameters::GetDefaultMaximumPduLength(), b.GetMaximumPduLength());
    ASSERT_FALSE(b.GetRemoteModality().IsDicomTlsEnabled());
    ASSERT_FALSE(b.GetRemoteModality().HasLocalAet());
    ASSERT_THROW(b.GetRemoteModality().GetLocalAet(), OrthancException);
    ASSERT_FALSE(b.GetRemoteModality().HasTimeout());
    ASSERT_EQ(0u, b.GetRemoteModality().GetTimeout());
    ASSERT_TRUE(b.IsRemoteCertificateRequired());
  }

  {
    RemoteModalityParameters p;
    p.SetApplicationEntityTitle("WORLD");
    p.SetPortNumber(4242);
    p.SetHost("hello.world.com");
    p.SetDicomTlsEnabled(true);
    p.SetTimeout(42);
    
    DicomAssociationParameters a("HELLO", p);
    a.SetOwnCertificatePath("key", "crt");
    a.SetTrustedCertificatesPath("trusted");
    a.SetRemoteCertificateRequired(false);

    ASSERT_THROW(a.SetMaximumPduLength(4095), OrthancException);
    ASSERT_THROW(a.SetMaximumPduLength(131073), OrthancException);
    a.SetMaximumPduLength(4096);
    a.SetMaximumPduLength(131072);

    Json::Value v = Json::objectValue;
    a.SerializeJob(v);

    ASSERT_EQ(8u, v.getMemberNames().size());
  
    DicomAssociationParameters b = DicomAssociationParameters::UnserializeJob(v);

    ASSERT_EQ("WORLD", b.GetRemoteModality().GetApplicationEntityTitle());
    ASSERT_EQ("hello.world.com", b.GetRemoteModality().GetHost());
    ASSERT_EQ(4242u, b.GetRemoteModality().GetPortNumber());
    ASSERT_EQ("HELLO", b.GetLocalApplicationEntityTitle());
    ASSERT_TRUE(b.GetRemoteModality().IsDicomTlsEnabled());
    ASSERT_EQ("key", b.GetOwnPrivateKeyPath());
    ASSERT_EQ("crt", b.GetOwnCertificatePath());
    ASSERT_EQ("trusted", b.GetTrustedCertificatesPath());
    ASSERT_EQ(131072u, b.GetMaximumPduLength());
    ASSERT_TRUE(b.GetRemoteModality().HasTimeout());
    ASSERT_EQ(42u, b.GetRemoteModality().GetTimeout());
    ASSERT_FALSE(b.IsRemoteCertificateRequired());
  }  
}


TEST(SerializationToolbox, Numbers)
{
  {
    int32_t i;
    ASSERT_FALSE(SerializationToolbox::ParseInteger32(i, ""));
    ASSERT_FALSE(SerializationToolbox::ParseInteger32(i, "ee"));
    ASSERT_TRUE(SerializationToolbox::ParseInteger32(i, "42"));  ASSERT_EQ(42, i);
    ASSERT_TRUE(SerializationToolbox::ParseInteger32(i, "-42"));  ASSERT_EQ(-42, i);
    ASSERT_TRUE(SerializationToolbox::ParseInteger32(i, "-2147483648")); ASSERT_EQ(-2147483648l, i);
    ASSERT_TRUE(SerializationToolbox::ParseInteger32(i, "2147483647")); ASSERT_EQ(2147483647l, i);
    ASSERT_FALSE(SerializationToolbox::ParseInteger32(i, "-2147483649"));
    ASSERT_FALSE(SerializationToolbox::ParseInteger32(i, "2147483648"));
    ASSERT_FALSE(SerializationToolbox::ParseInteger32(i, "-2\\-3\\-4"));
    ASSERT_TRUE(SerializationToolbox::ParseFirstInteger32(i, "-2\\-3\\-4"));  ASSERT_EQ(-2, i);
  }

  {
    uint32_t i;
    ASSERT_FALSE(SerializationToolbox::ParseUnsignedInteger32(i, ""));
    ASSERT_FALSE(SerializationToolbox::ParseUnsignedInteger32(i, "ee"));
    ASSERT_TRUE(SerializationToolbox::ParseUnsignedInteger32(i, "42"));  ASSERT_EQ(42u, i);
    ASSERT_FALSE(SerializationToolbox::ParseUnsignedInteger32(i, "-42"));
    ASSERT_TRUE(SerializationToolbox::ParseUnsignedInteger32(i, "4294967295")); ASSERT_EQ(4294967295u, i);
    ASSERT_FALSE(SerializationToolbox::ParseUnsignedInteger32(i, "4294967296"));
    ASSERT_FALSE(SerializationToolbox::ParseUnsignedInteger32(i, "2\\3\\4"));
    ASSERT_TRUE(SerializationToolbox::ParseFirstUnsignedInteger32(i, "2\\3\\4"));  ASSERT_EQ(2u, i);
  }

  {
    int64_t i;
    ASSERT_FALSE(SerializationToolbox::ParseInteger64(i, ""));
    ASSERT_FALSE(SerializationToolbox::ParseInteger64(i, "ee"));
    ASSERT_TRUE(SerializationToolbox::ParseInteger64(i, "42"));  ASSERT_EQ(42, i);
    ASSERT_TRUE(SerializationToolbox::ParseInteger64(i, "-42"));  ASSERT_EQ(-42, i);
    ASSERT_TRUE(SerializationToolbox::ParseInteger64(i, "-2147483649")); ASSERT_EQ(-2147483649ll, i);
    ASSERT_TRUE(SerializationToolbox::ParseInteger64(i, "2147483648")); ASSERT_EQ(2147483648ll, i);
    ASSERT_FALSE(SerializationToolbox::ParseInteger64(i, "-2\\-3\\-4"));
    ASSERT_TRUE(SerializationToolbox::ParseFirstInteger64(i, "-2\\-3\\-4"));  ASSERT_EQ(-2, i);
  }

  {
    uint64_t i;
    ASSERT_FALSE(SerializationToolbox::ParseUnsignedInteger64(i, ""));
    ASSERT_FALSE(SerializationToolbox::ParseUnsignedInteger64(i, "ee"));
    ASSERT_TRUE(SerializationToolbox::ParseUnsignedInteger64(i, "42"));  ASSERT_EQ(42u, i);
    ASSERT_FALSE(SerializationToolbox::ParseUnsignedInteger64(i, "-42"));
    ASSERT_TRUE(SerializationToolbox::ParseUnsignedInteger64(i, "4294967296")); ASSERT_EQ(4294967296lu, i);
    ASSERT_FALSE(SerializationToolbox::ParseUnsignedInteger64(i, "2\\3\\4"));
    ASSERT_TRUE(SerializationToolbox::ParseFirstUnsignedInteger64(i, "2\\3\\4"));  ASSERT_EQ(2u, i);
  }

  {
    float i;
    ASSERT_FALSE(SerializationToolbox::ParseFloat(i, ""));
    ASSERT_FALSE(SerializationToolbox::ParseFloat(i, "ee"));
    ASSERT_TRUE(SerializationToolbox::ParseFloat(i, "42"));  ASSERT_FLOAT_EQ(42.0f, i);
    ASSERT_TRUE(SerializationToolbox::ParseFloat(i, "-42"));  ASSERT_FLOAT_EQ(-42.0f, i);
    ASSERT_FALSE(SerializationToolbox::ParseFloat(i, "2\\3\\4"));
    ASSERT_TRUE(SerializationToolbox::ParseFirstFloat(i, "1.367\\2.367\\3.367"));  ASSERT_FLOAT_EQ(1.367f, i);

    ASSERT_TRUE(SerializationToolbox::ParseFloat(i, "1.2"));  ASSERT_FLOAT_EQ(1.2f, i);
    ASSERT_TRUE(SerializationToolbox::ParseFloat(i, "-1.2e+2"));  ASSERT_FLOAT_EQ(-120.0f, i);
    ASSERT_TRUE(SerializationToolbox::ParseFloat(i, "-1e-2"));  ASSERT_FLOAT_EQ(-0.01f, i);
    ASSERT_TRUE(SerializationToolbox::ParseFloat(i, "1.3671875"));  ASSERT_FLOAT_EQ(1.3671875f, i);
  }

  {
    double i;
    ASSERT_FALSE(SerializationToolbox::ParseDouble(i, ""));
    ASSERT_FALSE(SerializationToolbox::ParseDouble(i, "ee"));
    ASSERT_TRUE(SerializationToolbox::ParseDouble(i, "42"));  ASSERT_DOUBLE_EQ(42.0, i);
    ASSERT_TRUE(SerializationToolbox::ParseDouble(i, "-42"));  ASSERT_DOUBLE_EQ(-42.0, i);
    ASSERT_FALSE(SerializationToolbox::ParseDouble(i, "2\\3\\4"));
    ASSERT_TRUE(SerializationToolbox::ParseFirstDouble(i, "1.367\\2.367\\3.367"));  ASSERT_DOUBLE_EQ(1.367, i);

    ASSERT_TRUE(SerializationToolbox::ParseDouble(i, "1.2"));  ASSERT_DOUBLE_EQ(1.2, i);
    ASSERT_TRUE(SerializationToolbox::ParseDouble(i, "-1.2e+2"));  ASSERT_DOUBLE_EQ(-120.0, i);
    ASSERT_TRUE(SerializationToolbox::ParseDouble(i, "-1e-2"));  ASSERT_DOUBLE_EQ(-0.01, i);
    ASSERT_TRUE(SerializationToolbox::ParseDouble(i, "1.3671875"));  ASSERT_DOUBLE_EQ(1.3671875, i);
  }
}
