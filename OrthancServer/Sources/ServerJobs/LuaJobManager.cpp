/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2021-2023 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#include "../PrecompiledHeadersServer.h"
#include "LuaJobManager.h"

#include "../OrthancConfiguration.h"
#include "../../../OrthancFramework/Sources/Logging.h"

#include "../../../OrthancFramework/Sources/JobsEngine/Operations/LogJobOperation.h"
#include "Operations/DeleteResourceOperation.h"
#include "Operations/ModifyInstanceOperation.h"
#include "Operations/StorePeerOperation.h"
#include "Operations/StoreScuOperation.h"
#include "Operations/SystemCallOperation.h"

#include "../../../OrthancFramework/Sources/JobsEngine/Operations/NullOperationValue.h"
#include "../../../OrthancFramework/Sources/JobsEngine/Operations/StringOperationValue.h"
#include "Operations/DicomInstanceOperationValue.h"

namespace Orthanc
{
  void LuaJobManager::SignalDone(const SequenceOfOperationsJob& job)
  {
    boost::mutex::scoped_lock lock(mutex_);

    if (&job == currentJob_)
    {
      currentId_.clear();
      currentJob_ = NULL;
    }
  }


  LuaJobManager::LuaJobManager() :
    currentJob_(NULL),
    maxOperations_(1000),
    priority_(0),
    trailingTimeout_(5000)
  {
    unsigned int dicomTimeout;
    
    {
      OrthancConfiguration::ReaderLock lock;
      dicomTimeout = lock.GetConfiguration().GetUnsignedIntegerParameter("DicomAssociationCloseDelay", 5);
    }

    connectionManager_.SetInactivityTimeout(dicomTimeout * 1000);  // Milliseconds expected
    CLOG(INFO, LUA) << "Lua: DICOM associations will be closed after "
                    << dicomTimeout << " seconds of inactivity";
  }


  void LuaJobManager::SetMaxOperationsPerJob(size_t count)
  {
    boost::mutex::scoped_lock lock(mutex_);
    maxOperations_ = count;
  }


  void LuaJobManager::SetPriority(int priority)
  {
    boost::mutex::scoped_lock lock(mutex_);
    priority_ = priority;
  }


  void LuaJobManager::SetTrailingOperationTimeout(unsigned int timeout)
  {
    boost::mutex::scoped_lock lock(mutex_);
    trailingTimeout_ = timeout;
  }


  void LuaJobManager::AwakeTrailingSleep()
  {
    boost::mutex::scoped_lock lock(mutex_);

    CLOG(INFO, LUA) << "Awaking trailing sleep";

    if (currentJob_ != NULL)
    {
      currentJob_->AwakeTrailingSleep();
    }
  }


  LuaJobManager::Lock::Lock(LuaJobManager& that,
                            JobsEngine& engine) :
    that_(that),
    lock_(that.mutex_),
    engine_(engine)
  {
    if (that_.currentJob_ == NULL)
    {
      isNewJob_ = true;
    }
    else
    {
      jobLock_.reset(new SequenceOfOperationsJob::Lock(*that_.currentJob_));

      if (jobLock_->IsDone() ||
          jobLock_->GetOperationsCount() >= that_.maxOperations_)
      {
        jobLock_.reset(NULL);
        isNewJob_ = true;
      }
      else
      {
        isNewJob_ = false;
      }
    }

    if (isNewJob_)
    {
      // Need to create a new job, as the previous one is either
      // finished, or is getting too long
      that_.currentJob_ = new SequenceOfOperationsJob;
      that_.currentJob_->Register(that_);
      that_.currentJob_->SetDescription("Lua");

      {
        jobLock_.reset(new SequenceOfOperationsJob::Lock(*that_.currentJob_));
        jobLock_->SetTrailingOperationTimeout(that_.trailingTimeout_);
      }
    }

    assert(jobLock_.get() != NULL);
  }


  LuaJobManager::Lock::~Lock()
  {
    bool isEmpty;
    
    assert(jobLock_.get() != NULL);
    isEmpty = (isNewJob_ &&
               jobLock_->GetOperationsCount() == 0);
    
    jobLock_.reset(NULL);

    if (isNewJob_)
    {
      if (isEmpty)
      {
        // No operation was added, discard the newly created job
        isNewJob_ = false;
        delete that_.currentJob_;
        that_.currentJob_ = NULL;
      }
      else
      {
        engine_.GetRegistry().Submit(that_.currentId_, that_.currentJob_, that_.priority_);
      }
    }
  }


  size_t LuaJobManager::Lock::AddDeleteResourceOperation(ServerContext& context)
  {
    assert(jobLock_.get() != NULL);
    return jobLock_->AddOperation(new DeleteResourceOperation(context));
  }


  size_t LuaJobManager::Lock::AddLogOperation()
  {
    assert(jobLock_.get() != NULL);
    return jobLock_->AddOperation(new LogJobOperation);
  }


  size_t LuaJobManager::Lock::AddStoreScuOperation(ServerContext& context,
                                                   const std::string& localAet,
                                                   const RemoteModalityParameters& modality)
  {
    assert(jobLock_.get() != NULL);
    return jobLock_->AddOperation(new StoreScuOperation(
                                    context, that_.connectionManager_, localAet, modality));    
  }


  size_t LuaJobManager::Lock::AddStorePeerOperation(const WebServiceParameters& peer)
  {
    assert(jobLock_.get() != NULL);
    return jobLock_->AddOperation(new StorePeerOperation(peer));    
  }


  size_t LuaJobManager::Lock::AddSystemCallOperation(const std::string& command)
  {
    assert(jobLock_.get() != NULL);
    return jobLock_->AddOperation(new SystemCallOperation(command));    
  }
 

  size_t LuaJobManager::Lock::AddSystemCallOperation
  (const std::string& command,
   const std::vector<std::string>& preArguments,
   const std::vector<std::string>& postArguments)
  {
    assert(jobLock_.get() != NULL);
    return jobLock_->AddOperation
      (new SystemCallOperation(command, preArguments, postArguments));
  }


  size_t LuaJobManager::Lock::AddModifyInstanceOperation(ServerContext& context,
                                                         DicomModification* modification)
  {
    assert(jobLock_.get() != NULL);
    return jobLock_->AddOperation
      (new ModifyInstanceOperation(context, RequestOrigin_Lua, modification));
  }


  void LuaJobManager::Lock::AddNullInput(size_t operation)
  {
    assert(jobLock_.get() != NULL);
    NullOperationValue null;
    jobLock_->AddInput(operation, null);
  }


  void LuaJobManager::Lock::AddStringInput(size_t operation,
                                           const std::string& content)
  {
    assert(jobLock_.get() != NULL);
    StringOperationValue value(content);
    jobLock_->AddInput(operation, value);
  }


  void LuaJobManager::Lock::AddDicomInstanceInput(size_t operation,
                                                  ServerContext& context,
                                                  const std::string& instanceId)
  {
    assert(jobLock_.get() != NULL);
    DicomInstanceOperationValue value(context, instanceId);
    jobLock_->AddInput(operation, value);
  }


  void LuaJobManager::Lock::Connect(size_t operation1,
                                    size_t operation2)
  {
    assert(jobLock_.get() != NULL);
    jobLock_->Connect(operation1, operation2);
  }
}
