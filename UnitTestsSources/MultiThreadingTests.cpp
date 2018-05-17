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





#include "../OrthancServer/ServerContext.h"
#include "../Core/Logging.h"

#include "../Core/DicomNetworking/IDicomConnectionManager.h"
#include "../Core/JobsEngine/Operations/JobOperationValues.h"
#include "../Core/JobsEngine/Operations/StringOperationValue.h"
#include "../Core/JobsEngine/Operations/LogJobOperation.h"

namespace Orthanc
{
  class DicomInstanceValue : public JobOperationValue
  {
  private:
    ServerContext&   context_;
    std::string      id_;

  public:
    DicomInstanceValue(ServerContext& context,
                       const std::string& id) :
      JobOperationValue(Type_DicomInstance),
      context_(context),
      id_(id)
    {
    }

    const std::string& GetId() const
    {
      return id_;
    }

    void ReadContent(std::string& dicom) const
    {
      context_.ReadDicom(dicom, id_);
    }

    virtual JobOperationValue* Clone() const
    {
      return new DicomInstanceValue(context_, id_);
    }
  };


  class StoreScuOperation : public IJobOperation
  {
  private:
    std::string              localAet_;
    RemoteModalityParameters modality_;

  public:
    StoreScuOperation(const std::string& localAet,
                      const RemoteModalityParameters& modality) :
      localAet_(localAet),
      modality_(modality)
    {
    }

    virtual void Apply(JobOperationValues& outputs,
                       const JobOperationValue& input,
                       IDicomConnectionManager& manager)
    {
      std::auto_ptr<IDicomConnectionManager::IResource> resource
        (manager.AcquireConnection(localAet_, modality_));

      if (resource.get() == NULL)
      {
        LOG(ERROR) << "Cannot connect to modality: " << modality_.GetApplicationEntityTitle();
        return;
      }

      if (input.GetType() != JobOperationValue::Type_DicomInstance)
      {
        throw OrthancException(ErrorCode_BadParameterType);
      }

      const DicomInstanceValue& instance = dynamic_cast<const DicomInstanceValue&>(input);

      LOG(INFO) << "Sending instance " << instance.GetId() << " to modality \"" 
                << modality_.GetApplicationEntityTitle() << "\"";

      try
      {
        std::string dicom;
        instance.ReadContent(dicom);
        resource->GetConnection().Store(dicom);
        outputs.Append(instance.Clone());
      }
      catch (OrthancException& e)
      {
        LOG(ERROR) << "Unable to send instance " << instance.GetId() << " to modality \"" 
                   << modality_.GetApplicationEntityTitle() << "\": " << e.What();
      }
    }
  };


  class DeleteResourceOperation : public IJobOperation
  {
  private:
    ServerContext&  context_;

  public:
    DeleteResourceOperation(ServerContext& context) :
    context_(context)
    {
    }

    virtual void Apply(JobOperationValues& outputs,
                       const JobOperationValue& input,
                       IDicomConnectionManager& manager)
    {
      switch (input.GetType())
      {
        case JobOperationValue::Type_DicomInstance:
        {
          const DicomInstanceValue& instance = dynamic_cast<const DicomInstanceValue&>(input);
          LOG(INFO) << "Deleting instance: " << instance.GetId();

          try
          {
            Json::Value tmp;
            context_.DeleteResource(tmp, instance.GetId(), ResourceType_Instance);
          }
          catch (OrthancException& e)
          {
            LOG(ERROR) << "Unable to delete instance " << instance.GetId() << ": " << e.What();
          }

          break;
        }

        default:
          break;
      }
    }
  };



  class TimeoutDicomConnectionManager : public IDicomConnectionManager
  {
  private:
    class Resource : public IDicomConnectionManager::IResource
    {
    private:
      TimeoutDicomConnectionManager&  that_;
      boost::mutex::scoped_lock        lock_;

    public:
      Resource(TimeoutDicomConnectionManager& that) : 
      that_(that),
      lock_(that.mutex_)
      {
      }

      virtual ~Resource()
      {
        that_.Touch();
      }

      virtual DicomUserConnection& GetConnection()
      {
        if (that_.connection_.get() == NULL)
        {
          throw OrthancException(ErrorCode_InternalError);
        }

        return *that_.connection_;
      }
    };

    boost::mutex                         mutex_;
    std::auto_ptr<DicomUserConnection>   connection_;
    boost::posix_time::ptime             lastUse_;
    boost::posix_time::time_duration     timeout_;

    static boost::posix_time::ptime GetNow()
    {
      return boost::posix_time::microsec_clock::universal_time();
    }

    void Touch()
    {
      lastUse_ = GetNow();
    }

    void CheckTimeoutInternal()
    {
      if (connection_.get() != NULL &&
          (GetNow() - lastUse_) >= timeout_)
      {
        connection_.reset(NULL);
      }
    }

  public:
    TimeoutDicomConnectionManager() :
      timeout_(boost::posix_time::milliseconds(1000))
    {
    }

    void SetTimeout(unsigned int timeout)
    {
      boost::mutex::scoped_lock lock(mutex_);

      timeout_ = boost::posix_time::milliseconds(timeout);
      CheckTimeoutInternal();
    }

    unsigned int GetTimeout()
    {
      boost::mutex::scoped_lock lock(mutex_);
      return timeout_.total_milliseconds();
    }

    void Close()
    {
      boost::mutex::scoped_lock lock(mutex_);
      connection_.reset(NULL);
    }

    void CheckTimeout()
    {
      boost::mutex::scoped_lock lock(mutex_);
      CheckTimeoutInternal();
    }

    virtual IResource* AcquireConnection(const std::string& localAet,
                                         const RemoteModalityParameters& remote)
    {
      boost::mutex::scoped_lock lock(mutex_);

      if (connection_.get() == NULL ||
          !connection_->IsSameAssociation(localAet, remote))
      {
        connection_.reset(new DicomUserConnection(localAet, remote));
      }

      return new Resource(*this);
    }
  };



  class SequenceOfOperationsJob : public IJob
  {
  public:
    class IObserver : public boost::noncopyable
    {
    public:
      virtual ~IObserver()
      {
      }

      virtual void SignalDone(const SequenceOfOperationsJob& job) = 0;
    };

  private:
    class Operation : public boost::noncopyable
    {
    private:
      JobOperationValues            originalInputs_;
      JobOperationValues            workInputs_;
      std::auto_ptr<IJobOperation>  operation_;
      std::list<Operation*>         nextOperations_;
      size_t                        currentInput_;

    public:
      Operation(IJobOperation* operation) :
      operation_(operation),
      currentInput_(0)
      {
        if (operation == NULL)
        {
          throw OrthancException(ErrorCode_NullPointer);
        }
      }

      void AddOriginalInput(const JobOperationValue& value)
      {
        if (currentInput_ != 0)
        {
          // Cannot add input after processing has started
          throw OrthancException(ErrorCode_BadSequenceOfCalls);
        }
        else
        {
          originalInputs_.Append(value.Clone());
        }
      }

      const JobOperationValues& GetOriginalInputs() const
      {
        return originalInputs_;
      }

      void Reset()
      {
        workInputs_.Clear();
        currentInput_ = 0;
      }

      void AddNextOperation(Operation& other)
      {
        if (currentInput_ != 0)
        {
          // Cannot add input after processing has started
          throw OrthancException(ErrorCode_BadSequenceOfCalls);
        }
        else
        {
          nextOperations_.push_back(&other);
        }
      }

      bool IsDone() const
      {
        return currentInput_ >= originalInputs_.GetSize() + workInputs_.GetSize();
      }

      void Step()
      {
        if (IsDone())
        {
          throw OrthancException(ErrorCode_BadSequenceOfCalls);
        }

        const JobOperationValue* input;

        if (currentInput_ < originalInputs_.GetSize())
        {
          input = &originalInputs_.GetValue(currentInput_);
        }
        else
        {
          input = &workInputs_.GetValue(currentInput_ - originalInputs_.GetSize());
        }

        JobOperationValues outputs;
        operation_->Apply(outputs, *input);

        if (!nextOperations_.empty())
        {
          std::list<Operation*>::iterator first = nextOperations_.begin();
          outputs.Move((*first)->workInputs_);

          std::list<Operation*>::iterator current = first;
          ++current;

          while (current != nextOperations_.end())
          {
            (*first)->workInputs_.Copy((*current)->workInputs_);
            ++current;
          }
        }

        currentInput_ += 1;
      }
    };


    std::string                       jobType_;
    bool                              done_;
    boost::mutex                      mutex_;
    std::vector<Operation*>           operations_;
    size_t                            current_;
    boost::condition_variable         operationAdded_;
    boost::posix_time::time_duration  trailingTimeout_;
    std::list<IObserver*>             observers_;

    // Invoked from constructors
    void Setup()
    {
      done_ = false;
      current_ = 0;
      trailingTimeout_ = boost::posix_time::milliseconds(1000); 
    }

  public:
    SequenceOfOperationsJob() :
      jobType_("SequenceOfOperations")
    {
      Setup();
    }    

    SequenceOfOperationsJob(const std::string& jobType) :
      jobType_(jobType)
    {
      Setup();
    }    

    virtual ~SequenceOfOperationsJob()
    {
      for (size_t i = 0; i < operations_.size(); i++)
      {
        if (operations_[i] != NULL)
        {
          delete operations_[i];
        }
      }
    }

    void Register(IObserver& observer)
    {
      boost::mutex::scoped_lock lock(mutex_);
      observers_.push_back(&observer);
    }

    // This lock allows adding new operations to the end of the job,
    // from another thread than the worker thread, after the job has
    // been submitted for processing
    class Lock : public boost::noncopyable
    {
    private:
      SequenceOfOperationsJob&   that_;
      boost::mutex::scoped_lock  lock_;

    public:
      Lock(SequenceOfOperationsJob& that) :
      that_(that),
      lock_(that.mutex_)
      {
      }

      bool IsDone() const
      {
        return that_.done_;
      }

      void SetTrailingOperationTimeout(unsigned int timeout)
      {
        that_.trailingTimeout_ = boost::posix_time::milliseconds(timeout);
      }

      size_t AddOperation(IJobOperation* operation)
      {
        if (IsDone())
        {
          throw OrthancException(ErrorCode_BadSequenceOfCalls);
        }

        that_.operations_.push_back(new Operation(operation));
        that_.operationAdded_.notify_one();

        return that_.operations_.size() - 1;
      }

      size_t GetOperationsCount() const
      {
        return that_.operations_.size();
      }

      void AddInput(size_t index,
                    const JobOperationValue& value)
      {
        if (IsDone())
        {
          throw OrthancException(ErrorCode_BadSequenceOfCalls);
        }
        else if (index >= that_.operations_.size() ||
                 index < that_.current_)
        {
          throw OrthancException(ErrorCode_ParameterOutOfRange);
        }
        else
        {
          that_.operations_[index]->AddOriginalInput(value);
        }
      }
      
      void Connect(size_t input,
                   size_t output)
      {
        if (IsDone())
        {
          throw OrthancException(ErrorCode_BadSequenceOfCalls);
        }
        else if (input >= output ||
                 input >= that_.operations_.size() ||
                 output >= that_.operations_.size() ||
                 input < that_.current_ ||
                 output < that_.current_)
        {
          throw OrthancException(ErrorCode_ParameterOutOfRange);
        }
        else
        {
          Operation& a = *that_.operations_[input];
          Operation& b = *that_.operations_[output];
          a.AddNextOperation(b);
        }
      }
    };

    virtual void Start()
    {
    }

    virtual JobStepResult ExecuteStep()
    {
      boost::mutex::scoped_lock lock(mutex_);

      if (current_ == operations_.size())
      {
        LOG(INFO) << "Executing the trailing timeout in the sequence of operations";
        operationAdded_.timed_wait(lock, trailingTimeout_);
            
        if (current_ == operations_.size())
        {
          // No operation was added during the trailing timeout: The
          // job is over
          LOG(INFO) << "The sequence of operations is over";
          done_ = true;

          for (std::list<IObserver*>::iterator it = observers_.begin(); 
               it != observers_.end(); ++it)
          {
            (*it)->SignalDone(*this);
          }

          return JobStepResult::Success();
        }
        else
        {
          LOG(INFO) << "New operation added to the sequence of operations";
        }
      }

      assert(current_ < operations_.size());

      while (current_ < operations_.size() &&
             operations_[current_]->IsDone())
      {
        current_++;
      }

      if (current_ < operations_.size())
      {
        operations_[current_]->Step();
      }

      return JobStepResult::Continue();
    }

    virtual void SignalResubmit()
    {
      boost::mutex::scoped_lock lock(mutex_);
      
      current_ = 0;
      done_ = false;

      for (size_t i = 0; i < operations_.size(); i++)
      {
        operations_[i]->Reset();
      }
    }

    virtual void ReleaseResources()
    {
    }

    virtual float GetProgress()
    {
      boost::mutex::scoped_lock lock(mutex_);
      
      return (static_cast<float>(current_) / 
              static_cast<float>(operations_.size() + 1));
    }

    virtual void GetJobType(std::string& target)
    {
      target = jobType_;
    }

    virtual void GetPublicContent(Json::Value& value)
    {
      boost::mutex::scoped_lock lock(mutex_);

      value["CountOperations"] = static_cast<unsigned int>(operations_.size());
    }

    virtual void GetInternalContent(Json::Value& value)
    {
      // TODO
    }
  };


  class LuaJobManager : private SequenceOfOperationsJob::IObserver
  {
  public:
    typedef SequenceOfOperationsJob::Lock  Lock;

  private:
    boost::mutex                   mutex_;
    JobsEngine&                    engine_;
    TimeoutDicomConnectionManager  connectionManager_;
    std::string                    currentId_;
    SequenceOfOperationsJob*       currentJob_;
    size_t                         maxOperations_;
    int                            priority_;
    unsigned int                   trailingTimeout_;
    bool                           continue_;
    boost::thread                  connectionTimeoutThread_;

    static void ConnectionTimeoutThread(LuaJobManager* manager)
    {
      while (manager->continue_)
      {
        manager->connectionManager_.CheckTimeout();
        boost::this_thread::sleep(boost::posix_time::milliseconds(100));
      }
    }
    
    virtual void SignalDone(const SequenceOfOperationsJob& job)
    {
      boost::mutex::scoped_lock lock(mutex_);

      if (&job == currentJob_)
      {
        currentId_.clear();
        currentJob_ = NULL;
      }
    }

  public:
    LuaJobManager(JobsEngine&  engine) :
    engine_(engine),
    currentJob_(NULL),
    maxOperations_(1000),
    priority_(0),
    continue_(true)
    {
      connectionTimeoutThread_ = boost::thread(ConnectionTimeoutThread, this);
    }

    ~LuaJobManager()
    {
      continue_ = false;

      if (connectionTimeoutThread_.joinable())
      {
        connectionTimeoutThread_.join();
      }
    }

    void SetMaxOperationsPerJob(size_t count)
    {
      boost::mutex::scoped_lock lock(mutex_);
      maxOperations_ = count;
    }

    void SetPriority(int priority)
    {
      boost::mutex::scoped_lock lock(mutex_);
      priority_ = priority;
    }

    void SetTrailingOperationTimeout(unsigned int timeout)
    {
      boost::mutex::scoped_lock lock(mutex_);
      trailingTimeout_ = timeout;
    }

    Lock* Modify()
    {
      boost::mutex::scoped_lock lock(mutex_);

      if (currentJob_ != NULL)
      {
        std::auto_ptr<Lock> result(new Lock(*currentJob_));

        if (!result->IsDone() &&
            result->GetOperationsCount() < maxOperations_)
        {
          return result.release();
        }
      }

      // Need to create a new job, as the previous one is either
      // finished, or is getting too long
      currentJob_ = new SequenceOfOperationsJob;
      engine_.GetRegistry().Submit(currentId_, currentJob_, priority_);

      std::auto_ptr<Lock> result(new Lock(*currentJob_));
      result->SetTrailingOperationTimeout(trailingTimeout_);

      return result.release();
    }
  };
}


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


TEST(JobsEngine, Lua)
{
  JobsEngine engine;
  engine.SetWorkersCount(2);
  engine.Start();

  LuaJobManager lua(engine);
  lua.SetMaxOperationsPerJob(5);
  lua.SetTrailingOperationTimeout(200);

  for (size_t i = 0; i < 30; i++)
  {
    boost::this_thread::sleep(boost::posix_time::milliseconds(150));
    std::auto_ptr<LuaJobManager::Lock> lock(lua.Modify());
    size_t a = lock->AddOperation(new LogJobOperation);
    lock->AddInput(a, StringOperationValue(boost::lexical_cast<std::string>(i)));
  }

  boost::this_thread::sleep(boost::posix_time::milliseconds(2000));

  engine.Stop();

}
