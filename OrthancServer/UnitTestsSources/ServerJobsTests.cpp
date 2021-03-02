/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
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
#include <gtest/gtest.h>

#include "../../OrthancFramework/Sources/Compatibility.h"
#include "../../OrthancFramework/Sources/FileStorage/MemoryStorageArea.h"
#include "../../OrthancFramework/Sources/JobsEngine/Operations/LogJobOperation.h"
#include "../../OrthancFramework/Sources/Logging.h"
#include "../../OrthancFramework/Sources/SerializationToolbox.h"

#include "../Sources/Database/SQLiteDatabaseWrapper.h"
#include "../Sources/ServerContext.h"
#include "../Sources/ServerJobs/LuaJobManager.h"
#include "../Sources/ServerJobs/OrthancJobUnserializer.h"

#include "../Sources/ServerJobs/Operations/DeleteResourceOperation.h"
#include "../Sources/ServerJobs/Operations/DicomInstanceOperationValue.h"
#include "../Sources/ServerJobs/Operations/ModifyInstanceOperation.h"
#include "../Sources/ServerJobs/Operations/StorePeerOperation.h"
#include "../Sources/ServerJobs/Operations/StoreScuOperation.h"
#include "../Sources/ServerJobs/Operations/SystemCallOperation.h"

#include "../Sources/ServerJobs/ArchiveJob.h"
#include "../Sources/ServerJobs/DicomModalityStoreJob.h"
#include "../Sources/ServerJobs/DicomMoveScuJob.h"
#include "../Sources/ServerJobs/MergeStudyJob.h"
#include "../Sources/ServerJobs/OrthancPeerStoreJob.h"
#include "../Sources/ServerJobs/ResourceModificationJob.h"
#include "../Sources/ServerJobs/SplitStudyJob.h"


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
    
    DummyInstancesJob(const Json::Value& value) :
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


TEST(JobsEngine, DISABLED_Lua)
{
  JobsEngine engine(10);
  engine.SetThreadSleep(10);
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
                                         IJobOperation& operation)
{
  Json::Value a = 42;
  operation.Serialize(a);
  
  std::unique_ptr<IJobOperation> unserialized(unserializer.UnserializeOperation(a));
  
  Json::Value b = 43;
  unserialized->Serialize(b);

  return CheckSameJson(a, b);
}


static bool CheckIdempotentSerialization(IJobUnserializer& unserializer,
                                         IJobOperationValue& value)
{
  Json::Value a = 42;
  value.Serialize(a);
  
  std::unique_ptr<IJobOperationValue> unserialized(unserializer.UnserializeValue(a));
  
  Json::Value b = 43;
  unserialized->Serialize(b);

  return CheckSameJson(a, b);
}


TEST(JobsSerialization, GenericOperations)
{   
  DummyUnserializer unserializer;
  Json::Value s;

  {
    LogJobOperation operation;

    ASSERT_TRUE(CheckIdempotentSerialization(unserializer, operation));
    operation.Serialize(s);
  }

  ASSERT_THROW(unserializer.UnserializeJob(s), OrthancException);
  ASSERT_THROW(unserializer.UnserializeValue(s), OrthancException);

  {
    std::unique_ptr<IJobOperation> operation;
    operation.reset(unserializer.UnserializeOperation(s));

    // Make sure that we have indeed unserialized a log operation
    Json::Value dummy;
    ASSERT_THROW(dynamic_cast<DeleteResourceOperation&>(*operation).Serialize(dummy), std::bad_cast);
    dynamic_cast<LogJobOperation&>(*operation).Serialize(dummy);
  }
}


TEST(JobsSerialization, DicomInstanceOrigin)
{   
  Json::Value s;
  std::string t;

  {
    DicomInstanceOrigin origin;

    s = 42;
    origin.Serialize(s);
  }

  {
    DicomInstanceOrigin origin(s);
    ASSERT_EQ(RequestOrigin_Unknown, origin.GetRequestOrigin());
    ASSERT_EQ("", std::string(origin.GetRemoteAetC()));
    ASSERT_FALSE(origin.LookupRemoteIp(t));
    ASSERT_FALSE(origin.LookupRemoteAet(t));
    ASSERT_FALSE(origin.LookupCalledAet(t));
    ASSERT_FALSE(origin.LookupHttpUsername(t));
  }

  {
    DicomInstanceOrigin origin(DicomInstanceOrigin::FromDicomProtocol("host", "aet", "called"));

    s = 42;
    origin.Serialize(s);
  }

  {
    DicomInstanceOrigin origin(s);
    ASSERT_EQ(RequestOrigin_DicomProtocol, origin.GetRequestOrigin());
    ASSERT_EQ("aet", std::string(origin.GetRemoteAetC()));
    ASSERT_TRUE(origin.LookupRemoteIp(t));   ASSERT_EQ("host", t);
    ASSERT_TRUE(origin.LookupRemoteAet(t));  ASSERT_EQ("aet", t);
    ASSERT_TRUE(origin.LookupCalledAet(t));  ASSERT_EQ("called", t);
    ASSERT_FALSE(origin.LookupHttpUsername(t));
  }

  {
    DicomInstanceOrigin origin(DicomInstanceOrigin::FromHttp("host", "username"));

    s = 42;
    origin.Serialize(s);
  }

  {
    DicomInstanceOrigin origin(s);
    ASSERT_EQ(RequestOrigin_RestApi, origin.GetRequestOrigin());
    ASSERT_EQ("", std::string(origin.GetRemoteAetC()));
    ASSERT_TRUE(origin.LookupRemoteIp(t));     ASSERT_EQ("host", t);
    ASSERT_FALSE(origin.LookupRemoteAet(t));
    ASSERT_FALSE(origin.LookupCalledAet(t));
    ASSERT_TRUE(origin.LookupHttpUsername(t)); ASSERT_EQ("username", t);
  }

  {
    DicomInstanceOrigin origin(DicomInstanceOrigin::FromLua());

    s = 42;
    origin.Serialize(s);
  }

  {
    DicomInstanceOrigin origin(s);
    ASSERT_EQ(RequestOrigin_Lua, origin.GetRequestOrigin());
    ASSERT_FALSE(origin.LookupRemoteIp(t));
    ASSERT_FALSE(origin.LookupRemoteAet(t));
    ASSERT_FALSE(origin.LookupCalledAet(t));
    ASSERT_FALSE(origin.LookupHttpUsername(t));
  }

  {
    DicomInstanceOrigin origin(DicomInstanceOrigin::FromPlugins());

    s = 42;
    origin.Serialize(s);
  }

  {
    DicomInstanceOrigin origin(s);
    ASSERT_EQ(RequestOrigin_Plugins, origin.GetRequestOrigin());
    ASSERT_FALSE(origin.LookupRemoteIp(t));
    ASSERT_FALSE(origin.LookupRemoteAet(t));
    ASSERT_FALSE(origin.LookupCalledAet(t));
    ASSERT_FALSE(origin.LookupHttpUsername(t));
  }

  {
    DicomInstanceOrigin origin(DicomInstanceOrigin::FromWebDav());

    s = 42;
    origin.Serialize(s);
  }

  {
    DicomInstanceOrigin origin(s);
    ASSERT_EQ(RequestOrigin_WebDav, origin.GetRequestOrigin());
    ASSERT_EQ("", std::string(origin.GetRemoteAetC()));
    ASSERT_FALSE(origin.LookupRemoteIp(t));
    ASSERT_FALSE(origin.LookupRemoteAet(t));
    ASSERT_FALSE(origin.LookupCalledAet(t));
    ASSERT_FALSE(origin.LookupHttpUsername(t));
  }
}


namespace
{
  class OrthancJobsSerialization : public testing::Test
  {
  private:
    MemoryStorageArea              storage_;
    SQLiteDatabaseWrapper          db_;   // The SQLite DB is in memory
    std::unique_ptr<ServerContext>   context_;

  public:
    OrthancJobsSerialization()
    {
      db_.Open();
      context_.reset(new ServerContext(db_, storage_, true /* running unit tests */, 10));
      context_->SetupJobsEngine(true, false);
    }

    virtual ~OrthancJobsSerialization() ORTHANC_OVERRIDE
    {
      context_->Stop();
      context_.reset(NULL);
      db_.Close();
    }

    ServerContext& GetContext() 
    {
      return *context_;
    }

    bool CreateInstance(std::string& id)
    {
      // Create a sample DICOM file
      ParsedDicomFile dicom(true);
      dicom.Replace(DICOM_TAG_PATIENT_NAME, std::string("JODOGNE"),
                    false, DicomReplaceMode_InsertIfAbsent, "");

      std::unique_ptr<DicomInstanceToStore> toStore(DicomInstanceToStore::CreateFromParsedDicomFile(dicom));

      return (context_->Store(id, *toStore, StoreInstanceMode_Default) == StoreStatus_Success);
    }
  };
}


TEST_F(OrthancJobsSerialization, Values)
{
  std::string id;
  ASSERT_TRUE(CreateInstance(id));

  Json::Value s;
  OrthancJobUnserializer unserializer(GetContext());
    
  {
    DicomInstanceOperationValue instance(GetContext(), id);

    ASSERT_TRUE(CheckIdempotentSerialization(unserializer, instance));
    instance.Serialize(s);
  }

  std::unique_ptr<IJobOperationValue> value;
  value.reset(unserializer.UnserializeValue(s));
  ASSERT_EQ(IJobOperationValue::Type_DicomInstance, value->GetType());
  ASSERT_EQ(id, dynamic_cast<DicomInstanceOperationValue&>(*value).GetId());

  {
    std::string content;
    dynamic_cast<DicomInstanceOperationValue&>(*value).ReadDicom(content);

    ParsedDicomFile dicom(content);
    ASSERT_TRUE(dicom.GetTagValue(content, DICOM_TAG_PATIENT_NAME));
    ASSERT_EQ("JODOGNE", content);
  }
}


TEST_F(OrthancJobsSerialization, Operations)
{
  std::string id;
  ASSERT_TRUE(CreateInstance(id));

  Json::Value s;
  OrthancJobUnserializer unserializer(GetContext()); 

  // DeleteResourceOperation
  
  {
    DeleteResourceOperation operation(GetContext());

    ASSERT_TRUE(CheckIdempotentSerialization(unserializer, operation));
    operation.Serialize(s);
  }

  std::unique_ptr<IJobOperation> operation;

  {
    operation.reset(unserializer.UnserializeOperation(s));

    Json::Value dummy;
    ASSERT_THROW(dynamic_cast<LogJobOperation&>(*operation).Serialize(dummy), std::bad_cast);
    dynamic_cast<DeleteResourceOperation&>(*operation).Serialize(dummy);
  }

  // StorePeerOperation

  {
    WebServiceParameters peer;
    peer.SetUrl("http://localhost/");
    peer.SetCredentials("username", "password");
    peer.SetPkcs11Enabled(true);

    StorePeerOperation operation(peer);

    ASSERT_TRUE(CheckIdempotentSerialization(unserializer, operation));
    operation.Serialize(s);
  }

  {
    operation.reset(unserializer.UnserializeOperation(s));

    const StorePeerOperation& tmp = dynamic_cast<StorePeerOperation&>(*operation);
    ASSERT_EQ("http://localhost/", tmp.GetPeer().GetUrl());
    ASSERT_EQ("username", tmp.GetPeer().GetUsername());
    ASSERT_EQ("password", tmp.GetPeer().GetPassword());
    ASSERT_TRUE(tmp.GetPeer().IsPkcs11Enabled());
  }

  // StoreScuOperation

  {
    TimeoutDicomConnectionManager luaManager;
    
    {
      RemoteModalityParameters modality;
      modality.SetApplicationEntityTitle("REMOTE");
      modality.SetHost("192.168.1.1");
      modality.SetPortNumber(1000);
      modality.SetManufacturer(ModalityManufacturer_GE);

      StoreScuOperation operation(GetContext(), luaManager, "TEST", modality);

      ASSERT_TRUE(CheckIdempotentSerialization(unserializer, operation));
      operation.Serialize(s);
    }

    {
      operation.reset(unserializer.UnserializeOperation(s));

      const StoreScuOperation& tmp = dynamic_cast<StoreScuOperation&>(*operation);
      ASSERT_EQ("REMOTE", tmp.GetRemoteModality().GetApplicationEntityTitle());
      ASSERT_EQ("192.168.1.1", tmp.GetRemoteModality().GetHost());
      ASSERT_EQ(1000, tmp.GetRemoteModality().GetPortNumber());
      ASSERT_EQ(ModalityManufacturer_GE, tmp.GetRemoteModality().GetManufacturer());
      ASSERT_EQ("TEST", tmp.GetLocalAet());
    }
  }

  // SystemCallOperation

  {
    SystemCallOperation operation(std::string("echo"));
    operation.AddPreArgument("a");
    operation.AddPreArgument("b");
    operation.AddPostArgument("c");

    ASSERT_TRUE(CheckIdempotentSerialization(unserializer, operation));
    operation.Serialize(s);
  }

  {
    operation.reset(unserializer.UnserializeOperation(s));

    const SystemCallOperation& tmp = dynamic_cast<SystemCallOperation&>(*operation);
    ASSERT_EQ("echo", tmp.GetCommand());
    ASSERT_EQ(2u, tmp.GetPreArgumentsCount());
    ASSERT_EQ(1u, tmp.GetPostArgumentsCount());
    ASSERT_EQ("a", tmp.GetPreArgument(0));
    ASSERT_EQ("b", tmp.GetPreArgument(1));
    ASSERT_EQ("c", tmp.GetPostArgument(0));
  }

  // ModifyInstanceOperation

  {
    std::unique_ptr<DicomModification> modification(new DicomModification);
    modification->SetupAnonymization(DicomVersion_2008);
    
    ModifyInstanceOperation operation(GetContext(), RequestOrigin_Lua, modification.release());

    ASSERT_TRUE(CheckIdempotentSerialization(unserializer, operation));
    operation.Serialize(s);
  }

  {
    operation.reset(unserializer.UnserializeOperation(s));

    const ModifyInstanceOperation& tmp = dynamic_cast<ModifyInstanceOperation&>(*operation);
    ASSERT_EQ(RequestOrigin_Lua, tmp.GetRequestOrigin());
    ASSERT_TRUE(tmp.GetModification().IsRemoved(DICOM_TAG_STUDY_DESCRIPTION));
  }
}


TEST_F(OrthancJobsSerialization, Jobs)
{
  Json::Value s;

  // ArchiveJob

  {
    ArchiveJob job(GetContext(), false, false);
    ASSERT_FALSE(job.Serialize(s));  // Cannot serialize this
  }

  // DicomModalityStoreJob

  OrthancJobUnserializer unserializer(GetContext()); 

  {
    RemoteModalityParameters modality;
    modality.SetApplicationEntityTitle("REMOTE");
    modality.SetHost("192.168.1.1");
    modality.SetPortNumber(1000);
    modality.SetManufacturer(ModalityManufacturer_GE);

    DicomModalityStoreJob job(GetContext());
    job.SetLocalAet("LOCAL");
    job.SetRemoteModality(modality);
    job.SetMoveOriginator("MOVESCU", 42);

    ASSERT_TRUE(CheckIdempotentSetOfInstances(unserializer, job));
    ASSERT_TRUE(job.Serialize(s));
  }

  {
    std::unique_ptr<IJob> job;
    job.reset(unserializer.UnserializeJob(s));

    DicomModalityStoreJob& tmp = dynamic_cast<DicomModalityStoreJob&>(*job);
    ASSERT_EQ("LOCAL", tmp.GetParameters().GetLocalApplicationEntityTitle());
    ASSERT_EQ("REMOTE", tmp.GetParameters().GetRemoteModality().GetApplicationEntityTitle());
    ASSERT_EQ("192.168.1.1", tmp.GetParameters().GetRemoteModality().GetHost());
    ASSERT_EQ(1000, tmp.GetParameters().GetRemoteModality().GetPortNumber());
    ASSERT_EQ(ModalityManufacturer_GE, tmp.GetParameters().GetRemoteModality().GetManufacturer());
    ASSERT_TRUE(tmp.HasMoveOriginator());
    ASSERT_EQ("MOVESCU", tmp.GetMoveOriginatorAet());
    ASSERT_EQ(42, tmp.GetMoveOriginatorId());
  }

  // OrthancPeerStoreJob

  {
    WebServiceParameters peer;
    peer.SetUrl("http://localhost/");
    peer.SetCredentials("username", "password");
    peer.SetPkcs11Enabled(true);

    OrthancPeerStoreJob job(GetContext());
    job.SetPeer(peer);
    
    ASSERT_TRUE(CheckIdempotentSetOfInstances(unserializer, job));
    ASSERT_TRUE(job.Serialize(s));
  }

  {
    std::unique_ptr<IJob> job;
    job.reset(unserializer.UnserializeJob(s));

    OrthancPeerStoreJob& tmp = dynamic_cast<OrthancPeerStoreJob&>(*job);
    ASSERT_EQ("http://localhost/", tmp.GetPeer().GetUrl());
    ASSERT_EQ("username", tmp.GetPeer().GetUsername());
    ASSERT_EQ("password", tmp.GetPeer().GetPassword());
    ASSERT_TRUE(tmp.GetPeer().IsPkcs11Enabled());
    ASSERT_FALSE(tmp.IsTranscode());
    ASSERT_THROW(tmp.GetTransferSyntax(), OrthancException);
  }

  {
    OrthancPeerStoreJob job(GetContext());
    ASSERT_THROW(job.SetTranscode("nope"), OrthancException);
    job.SetTranscode("1.2.840.10008.1.2.4.50");
    
    ASSERT_TRUE(CheckIdempotentSetOfInstances(unserializer, job));
    ASSERT_TRUE(job.Serialize(s));
  }

  {
    std::unique_ptr<IJob> job;
    job.reset(unserializer.UnserializeJob(s));

    OrthancPeerStoreJob& tmp = dynamic_cast<OrthancPeerStoreJob&>(*job);
    ASSERT_EQ("http://127.0.0.1:8042/", tmp.GetPeer().GetUrl());
    ASSERT_EQ("", tmp.GetPeer().GetUsername());
    ASSERT_EQ("", tmp.GetPeer().GetPassword());
    ASSERT_FALSE(tmp.GetPeer().IsPkcs11Enabled());
    ASSERT_TRUE(tmp.IsTranscode());
    ASSERT_EQ(DicomTransferSyntax_JPEGProcess1, tmp.GetTransferSyntax());
  }

  // ResourceModificationJob

  {
    std::unique_ptr<DicomModification> modification(new DicomModification);
    modification->SetupAnonymization(DicomVersion_2008);    

    ResourceModificationJob job(GetContext());
    job.SetModification(modification.release(), ResourceType_Patient, true);
    job.SetOrigin(DicomInstanceOrigin::FromLua());

    job.AddTrailingStep();  // Necessary since 1.7.0
    ASSERT_TRUE(CheckIdempotentSetOfInstances(unserializer, job));
    ASSERT_TRUE(job.Serialize(s));
  }

  {
    std::unique_ptr<IJob> job;
    job.reset(unserializer.UnserializeJob(s));

    ResourceModificationJob& tmp = dynamic_cast<ResourceModificationJob&>(*job);
    ASSERT_TRUE(tmp.IsAnonymization());
    ASSERT_FALSE(tmp.IsTranscode());
    ASSERT_THROW(tmp.GetTransferSyntax(), OrthancException);
    ASSERT_EQ(RequestOrigin_Lua, tmp.GetOrigin().GetRequestOrigin());
    ASSERT_TRUE(tmp.GetModification().IsRemoved(DICOM_TAG_STUDY_DESCRIPTION));
  }

  {
    ResourceModificationJob job(GetContext());
    ASSERT_THROW(job.SetTranscode("nope"), OrthancException);
    job.SetTranscode(DicomTransferSyntax_JPEGProcess1);

    job.AddTrailingStep();  // Necessary since 1.7.0
    ASSERT_TRUE(CheckIdempotentSetOfInstances(unserializer, job));
    ASSERT_TRUE(job.Serialize(s));
  }

  {
    std::unique_ptr<IJob> job;
    job.reset(unserializer.UnserializeJob(s));

    ResourceModificationJob& tmp = dynamic_cast<ResourceModificationJob&>(*job);
    ASSERT_FALSE(tmp.IsAnonymization());
    ASSERT_TRUE(tmp.IsTranscode());
    ASSERT_EQ(DicomTransferSyntax_JPEGProcess1, tmp.GetTransferSyntax());
    ASSERT_EQ(RequestOrigin_Unknown, tmp.GetOrigin().GetRequestOrigin());
  }

  // SplitStudyJob

  std::string instance;
  ASSERT_TRUE(CreateInstance(instance));

  std::string study, series;

  {
    ServerContext::DicomCacheLocker lock(GetContext(), instance);
    study = lock.GetDicom().GetHasher().HashStudy();
    series = lock.GetDicom().GetHasher().HashSeries();
  }

  {
    std::list<std::string> tmp;
    GetContext().GetIndex().GetAllUuids(tmp, ResourceType_Study);
    ASSERT_EQ(1u, tmp.size());
    ASSERT_EQ(study, tmp.front());
    GetContext().GetIndex().GetAllUuids(tmp, ResourceType_Series);
    ASSERT_EQ(1u, tmp.size());
    ASSERT_EQ(series, tmp.front());
  }

  std::string study2;

  {
    std::string a, b;

    {
      ASSERT_THROW(SplitStudyJob(GetContext(), std::string("nope")), OrthancException);

      SplitStudyJob job(GetContext(), study);
      job.SetKeepSource(true);
      job.AddSourceSeries(series);
      ASSERT_THROW(job.AddSourceSeries("nope"), OrthancException);
      job.SetOrigin(DicomInstanceOrigin::FromLua());
      job.Replace(DICOM_TAG_PATIENT_NAME, "hello");
      job.Remove(DICOM_TAG_PATIENT_BIRTH_DATE);
      ASSERT_THROW(job.Replace(DICOM_TAG_SERIES_DESCRIPTION, "nope"), OrthancException);
      ASSERT_THROW(job.Remove(DICOM_TAG_SERIES_DESCRIPTION), OrthancException);
    
      ASSERT_TRUE(job.GetTargetStudy().empty());
      a = job.GetTargetStudyUid();
      ASSERT_TRUE(job.LookupTargetSeriesUid(b, series));

      job.AddTrailingStep();
      job.Start();
      ASSERT_EQ(JobStepCode_Continue, job.Step("jobId").GetCode());
      ASSERT_EQ(JobStepCode_Success, job.Step("jobId").GetCode());

      study2 = job.GetTargetStudy();
      ASSERT_FALSE(study2.empty());

      ASSERT_TRUE(CheckIdempotentSetOfInstances(unserializer, job));
      ASSERT_TRUE(job.Serialize(s));
    }

    {
      std::unique_ptr<IJob> job;
      job.reset(unserializer.UnserializeJob(s));

      SplitStudyJob& tmp = dynamic_cast<SplitStudyJob&>(*job);
      ASSERT_TRUE(tmp.IsKeepSource());
      ASSERT_EQ(study, tmp.GetSourceStudy());
      ASSERT_EQ(a, tmp.GetTargetStudyUid());
      ASSERT_EQ(RequestOrigin_Lua, tmp.GetOrigin().GetRequestOrigin());

      std::string s;
      ASSERT_EQ(study2, tmp.GetTargetStudy());
      ASSERT_FALSE(tmp.LookupTargetSeriesUid(s, "nope"));
      ASSERT_TRUE(tmp.LookupTargetSeriesUid(s, series));
      ASSERT_EQ(b, s);

      ASSERT_FALSE(tmp.LookupReplacement(s, DICOM_TAG_STUDY_DESCRIPTION));
      ASSERT_TRUE(tmp.LookupReplacement(s, DICOM_TAG_PATIENT_NAME));
      ASSERT_EQ("hello", s);
      ASSERT_FALSE(tmp.IsRemoved(DICOM_TAG_PATIENT_NAME));
      ASSERT_TRUE(tmp.IsRemoved(DICOM_TAG_PATIENT_BIRTH_DATE));
    }
  }

  {
    std::list<std::string> tmp;
    GetContext().GetIndex().GetAllUuids(tmp, ResourceType_Study);
    ASSERT_EQ(2u, tmp.size());
    GetContext().GetIndex().GetAllUuids(tmp, ResourceType_Series);
    ASSERT_EQ(2u, tmp.size());
  }

  // MergeStudyJob

  {
    ASSERT_THROW(SplitStudyJob(GetContext(), std::string("nope")), OrthancException);

    MergeStudyJob job(GetContext(), study);
    job.SetKeepSource(true);
    job.AddSource(study2);
    ASSERT_THROW(job.AddSourceSeries("nope"), OrthancException);
    ASSERT_THROW(job.AddSourceStudy("nope"), OrthancException);
    ASSERT_THROW(job.AddSource("nope"), OrthancException);
    job.SetOrigin(DicomInstanceOrigin::FromLua());
    
    ASSERT_EQ(job.GetTargetStudy(), study);

    job.AddTrailingStep();
    job.Start();
    ASSERT_EQ(JobStepCode_Continue, job.Step("jobId").GetCode());
    ASSERT_EQ(JobStepCode_Success, job.Step("jobId").GetCode());

    ASSERT_TRUE(CheckIdempotentSetOfInstances(unserializer, job));
    ASSERT_TRUE(job.Serialize(s));
  }

  {
    std::list<std::string> tmp;
    GetContext().GetIndex().GetAllUuids(tmp, ResourceType_Study);
    ASSERT_EQ(2u, tmp.size());
    GetContext().GetIndex().GetAllUuids(tmp, ResourceType_Series);
    ASSERT_EQ(3u, tmp.size());
  }

  {
    std::unique_ptr<IJob> job;
    job.reset(unserializer.UnserializeJob(s));

    MergeStudyJob& tmp = dynamic_cast<MergeStudyJob&>(*job);
    ASSERT_TRUE(tmp.IsKeepSource());
    ASSERT_EQ(study, tmp.GetTargetStudy());
    ASSERT_EQ(RequestOrigin_Lua, tmp.GetOrigin().GetRequestOrigin());
  }
}



TEST_F(OrthancJobsSerialization, DicomAssociationParameters)
{
  Json::Value v;

  {
    v = Json::objectValue;
    DicomAssociationParameters p;
    p.SerializeJob(v);
  }

  {
    DicomAssociationParameters p = DicomAssociationParameters::UnserializeJob(v);
    ASSERT_EQ("ORTHANC", p.GetLocalApplicationEntityTitle());
    ASSERT_EQ("ANY-SCP", p.GetRemoteModality().GetApplicationEntityTitle());
    ASSERT_EQ(104u, p.GetRemoteModality().GetPortNumber());
    ASSERT_EQ(ModalityManufacturer_Generic, p.GetRemoteModality().GetManufacturer());
    ASSERT_EQ("127.0.0.1", p.GetRemoteModality().GetHost());
    ASSERT_EQ(DicomAssociationParameters::GetDefaultTimeout(), p.GetTimeout());
  }

  {
    v = Json::objectValue;
    DicomAssociationParameters p;
    p.SetLocalApplicationEntityTitle("HELLO");
    p.SetRemoteApplicationEntityTitle("WORLD");
    p.SetRemotePort(42);
    p.SetRemoteHost("MY_HOST");
    p.SetTimeout(43);
    p.SerializeJob(v);
  }

  {
    DicomAssociationParameters p = DicomAssociationParameters::UnserializeJob(v);
    ASSERT_EQ("HELLO", p.GetLocalApplicationEntityTitle());
    ASSERT_EQ("WORLD", p.GetRemoteModality().GetApplicationEntityTitle());
    ASSERT_EQ(42u, p.GetRemoteModality().GetPortNumber());
    ASSERT_EQ(ModalityManufacturer_Generic, p.GetRemoteModality().GetManufacturer());
    ASSERT_EQ("MY_HOST", p.GetRemoteModality().GetHost());
    ASSERT_EQ(43u, p.GetTimeout());
  }
  
  {
    DicomModalityStoreJob job(GetContext());
    job.Serialize(v);
  }
  
  {
    OrthancJobUnserializer unserializer(GetContext());
    std::unique_ptr<DicomModalityStoreJob> job(
      dynamic_cast<DicomModalityStoreJob*>(unserializer.UnserializeJob(v)));
    ASSERT_EQ("ORTHANC", job->GetParameters().GetLocalApplicationEntityTitle());
    ASSERT_EQ("ANY-SCP", job->GetParameters().GetRemoteModality().GetApplicationEntityTitle());
    ASSERT_EQ("127.0.0.1", job->GetParameters().GetRemoteModality().GetHost());
    ASSERT_EQ(104u, job->GetParameters().GetRemoteModality().GetPortNumber());
    ASSERT_EQ(ModalityManufacturer_Generic, job->GetParameters().GetRemoteModality().GetManufacturer());
    ASSERT_EQ(DicomAssociationParameters::GetDefaultTimeout(), job->GetParameters().GetTimeout());
    ASSERT_FALSE(job->HasMoveOriginator());
    ASSERT_THROW(job->GetMoveOriginatorAet(), OrthancException);
    ASSERT_THROW(job->GetMoveOriginatorId(), OrthancException);
    ASSERT_FALSE(job->HasStorageCommitment());
  }
  
  {
    RemoteModalityParameters r;
    r.SetApplicationEntityTitle("HELLO");
    r.SetPortNumber(42);
    r.SetHost("MY_HOST");

    DicomModalityStoreJob job(GetContext());
    job.SetLocalAet("WORLD");
    job.SetRemoteModality(r);
    job.SetTimeout(43);
    job.SetMoveOriginator("ORIGINATOR", 100);
    job.EnableStorageCommitment(true);
    job.Serialize(v);
  }
  
  {
    OrthancJobUnserializer unserializer(GetContext());
    std::unique_ptr<DicomModalityStoreJob> job(
      dynamic_cast<DicomModalityStoreJob*>(unserializer.UnserializeJob(v)));
    ASSERT_EQ("WORLD", job->GetParameters().GetLocalApplicationEntityTitle());
    ASSERT_EQ("HELLO", job->GetParameters().GetRemoteModality().GetApplicationEntityTitle());
    ASSERT_EQ("MY_HOST", job->GetParameters().GetRemoteModality().GetHost());
    ASSERT_EQ(42u, job->GetParameters().GetRemoteModality().GetPortNumber());
    ASSERT_EQ(ModalityManufacturer_Generic, job->GetParameters().GetRemoteModality().GetManufacturer());
    ASSERT_EQ(43u, job->GetParameters().GetTimeout());
    ASSERT_TRUE(job->HasMoveOriginator());
    ASSERT_EQ("ORIGINATOR", job->GetMoveOriginatorAet());
    ASSERT_EQ(100, job->GetMoveOriginatorId());
    ASSERT_TRUE(job->HasStorageCommitment());
  }
    
  {
    DicomMoveScuJob job(GetContext());
    job.Serialize(v);
  }
  
  {
    OrthancJobUnserializer unserializer(GetContext());
    std::unique_ptr<DicomMoveScuJob> job(
      dynamic_cast<DicomMoveScuJob*>(unserializer.UnserializeJob(v)));
    ASSERT_EQ("ORTHANC", job->GetParameters().GetLocalApplicationEntityTitle());
    ASSERT_EQ("ANY-SCP", job->GetParameters().GetRemoteModality().GetApplicationEntityTitle());
    ASSERT_EQ("127.0.0.1", job->GetParameters().GetRemoteModality().GetHost());
    ASSERT_EQ(104u, job->GetParameters().GetRemoteModality().GetPortNumber());
    ASSERT_EQ(ModalityManufacturer_Generic, job->GetParameters().GetRemoteModality().GetManufacturer());
    ASSERT_EQ(DicomAssociationParameters::GetDefaultTimeout(), job->GetParameters().GetTimeout());
  }
  
  {
    RemoteModalityParameters r;
    r.SetApplicationEntityTitle("HELLO");
    r.SetPortNumber(42);
    r.SetHost("MY_HOST");

    DicomMoveScuJob job(GetContext());
    job.SetLocalAet("WORLD");
    job.SetRemoteModality(r);
    job.SetTimeout(43);
    job.Serialize(v);
  }
  
  {
    OrthancJobUnserializer unserializer(GetContext());
    std::unique_ptr<DicomMoveScuJob> job(
      dynamic_cast<DicomMoveScuJob*>(unserializer.UnserializeJob(v)));
    ASSERT_EQ("WORLD", job->GetParameters().GetLocalApplicationEntityTitle());
    ASSERT_EQ("HELLO", job->GetParameters().GetRemoteModality().GetApplicationEntityTitle());
    ASSERT_EQ("MY_HOST", job->GetParameters().GetRemoteModality().GetHost());
    ASSERT_EQ(42u, job->GetParameters().GetRemoteModality().GetPortNumber());
    ASSERT_EQ(ModalityManufacturer_Generic, job->GetParameters().GetRemoteModality().GetManufacturer());
    ASSERT_EQ(43u, job->GetParameters().GetTimeout());
  }
}
