/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2024 Osimis S.A., Belgium
 * Copyright (C) 2021-2024 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#include "ThreadedSetOfInstancesJob.h"

#include "../../../OrthancFramework/Sources/Logging.h"
#include "../../../OrthancFramework/Sources/OrthancException.h"
#include "../../../OrthancFramework/Sources/SerializationToolbox.h"
#include "../ServerContext.h"

#include <boost/lexical_cast.hpp>
#include <cassert>

namespace Orthanc
{
  static const char* EXIT_WORKER_MESSAGE = "exit";

   ThreadedSetOfInstancesJob::ThreadedSetOfInstancesJob(ServerContext& context,
                                                        bool hasPostProcessing,
                                                        bool keepSource,
                                                        size_t workersCount) :
    hasPostProcessing_(hasPostProcessing),
    started_(false),
    stopRequested_(false),
    permissive_(false),
    currentStep_(ThreadedJobStep_NotStarted),
    workersCount_(workersCount),
    context_(context),
    keepSource_(keepSource),
    errorCode_(ErrorCode_Success)
  {
  }


  ThreadedSetOfInstancesJob::~ThreadedSetOfInstancesJob()
  {
    // no need to lock mutex here since we access variables used only by the "master" thread

    StopWorkers();
    WaitWorkersComplete();
  }


  void ThreadedSetOfInstancesJob::InitWorkers(size_t workersCount)
  {
    // no need to lock mutex here since we access variables used only by the "master" thread

    for (size_t i = 0; i < workersCount; i++)
    {
      instancesWorkers_.push_back(boost::shared_ptr<boost::thread>(new boost::thread(InstanceWorkerThread, this)));
    }
  }


  void ThreadedSetOfInstancesJob::WaitWorkersComplete()
  {
    // no need to lock mutex here since we access variables used only by the "master" thread

    // send a dummy "exit" message to all workers such that they stop waiting for messages on the queue
    for (size_t i = 0; i < instancesWorkers_.size(); i++)
    {
      instancesToProcessQueue_.Enqueue(new SingleValueObject<std::string>(EXIT_WORKER_MESSAGE));
    }

    for (size_t i = 0; i < instancesWorkers_.size(); i++)
    {
      if (instancesWorkers_[i]->joinable())
      {
        instancesWorkers_[i]->join();
      }
    }

    instancesWorkers_.clear();
  }


  void ThreadedSetOfInstancesJob::StopWorkers()
  {
    boost::recursive_mutex::scoped_lock lock(mutex_);

    instancesToProcessQueue_.Clear();
    stopRequested_ = true;
  }


  void ThreadedSetOfInstancesJob::Stop(JobStopReason reason)
  {
    // no need to lock mutex here since we access variables used or set only by the "master" thread

    if (reason == JobStopReason_Canceled ||
        reason == JobStopReason_Failure ||
        reason == JobStopReason_Retry)
    {
      // deallocate resources
      StopWorkers();
      WaitWorkersComplete();
    }
    else if (reason == JobStopReason_Paused)
    {
      // keep resources allocated.
      // note that, right now, since all instances are queued from the start, this kind of jobs is not paused while in ProcessingInstances state
    }
  }


  JobStepResult ThreadedSetOfInstancesJob::Step(const std::string& jobId)
  {
    // no need to lock mutex here since we access variables used or set only by the "master" thread

    if (!started_)
    {
      throw OrthancException(ErrorCode_InternalError);
    }

    if (GetInstancesCount() == 0)
    {
      // No instances to handle: We're done
      return JobStepResult::Success();
    }
    
    try
    {
      if (currentStep_ == ThreadedJobStep_NotStarted)
      {
        // create the workers and enqueue all instances
        for (std::set<std::string>::const_iterator it = instancesToProcess_.begin(); it != instancesToProcess_.end(); ++it)
        {
          instancesToProcessQueue_.Enqueue(new SingleValueObject<std::string>(*it));
        }

        InitWorkers(workersCount_);
        currentStep_ = ThreadedJobStep_ProcessingInstances;
      }
      else if (currentStep_ == ThreadedJobStep_ProcessingInstances)
      {
        // wait until all instances are processed by the workers
        if (instancesToProcessQueue_.GetSize() != 0)
        {
          // "slow down" the job main thread, to avoid using 100% of a core simply to check that other threads are done
          boost::this_thread::sleep(boost::posix_time::milliseconds(5));

          return JobStepResult::Continue();
        }
        else
        {
          WaitWorkersComplete();

          // check job has really completed !!! it might have been interrupted because of an error
          if ((processedInstances_.size() != instancesToProcess_.size())
            || (!IsPermissive() && failedInstances_.size() > 0))
          {
            return JobStepResult::Failure(GetErrorCode(), NULL);
          }

          currentStep_ = ThreadedJobStep_PostProcessingInstances;
          return JobStepResult::Continue();
        }
      }
      else if (currentStep_ == ThreadedJobStep_PostProcessingInstances)
      {
        if (HasPostProcessingStep())
        {
          PostProcessInstances();
        }

        currentStep_ = ThreadedJobStep_Cleanup;
        return JobStepResult::Continue();
      }
      else if (currentStep_ == ThreadedJobStep_Cleanup)
      {
        // clean after the post processing step
        if (HasCleanupStep())
        {
          for (std::set<std::string>::const_iterator it = instancesToProcess_.begin(); it != instancesToProcess_.end(); ++it)
          {
            Json::Value tmp;
            context_.DeleteResource(tmp, *it, ResourceType_Instance);
          }
        }

        currentStep_ = ThreadedJobStep_Done;
        return JobStepResult::Success();
      }
    }
    catch (OrthancException& e)
    {
      if (permissive_)
      {
        LOG(WARNING) << "Ignoring an error in a permissive job: " << e.What();
      }
      else
      {
        return JobStepResult::Failure(e);
      }
    }

    return JobStepResult::Continue();
  }


  bool ThreadedSetOfInstancesJob::HasPostProcessingStep() const
  {
    boost::recursive_mutex::scoped_lock lock(mutex_);

    return hasPostProcessing_;
  }

  void ThreadedSetOfInstancesJob::PostProcessInstances()
  {
    if (HasPostProcessingStep())
    {
      throw OrthancException(ErrorCode_InternalError, "Job with post-processing should override PostProcessInstances");
    }
  }


  bool ThreadedSetOfInstancesJob::HasCleanupStep() const
  {
    boost::recursive_mutex::scoped_lock lock(mutex_);

    return !keepSource_;
  }


  void ThreadedSetOfInstancesJob::InstanceWorkerThread(ThreadedSetOfInstancesJob* that)
  {
    static uint16_t threadCounter = 0;
    Logging::SetCurrentThreadName(std::string("JOB-INS-WORK-") + boost::lexical_cast<std::string>(threadCounter++));
    threadCounter %= 1000;

    while (true)
    {
      std::unique_ptr<SingleValueObject<std::string> > instanceId(dynamic_cast<SingleValueObject<std::string>*>(that->instancesToProcessQueue_.Dequeue(0)));
      if (that->stopRequested_                // no lock(mutex) to access this variable, this is safe since it's just reading a boolean
        || instanceId->GetValue() == EXIT_WORKER_MESSAGE)
      {
        return;
      }

      bool processed = false;
      
      try
      {
        processed = that->HandleInstance(instanceId->GetValue());
      }
      catch (const Orthanc::OrthancException& e)
      {
        if (that->IsPermissive())
        {
          LOG(WARNING) << "Ignoring an error in a permissive job: " << e.What();
        }
        else
        {
          LOG(ERROR) << "Error in a non-permissive job: " << e.What();
          that->SetErrorCode(e.GetErrorCode());
          that->StopWorkers();
          return;
        }
      }
      catch (...)
      {
        LOG(ERROR) << "Native exception while executing a job";
        that->SetErrorCode(ErrorCode_InternalError);
        that->StopWorkers();
        return;
      }

      {
        boost::recursive_mutex::scoped_lock lock(that->mutex_);
        
        that->processedInstances_.insert(instanceId->GetValue());

        if (!processed)
        {
          that->failedInstances_.insert(instanceId->GetValue()); 
        }
      }

    }
  }


  size_t ThreadedSetOfInstancesJob::GetInstancesCount() const
  {
    boost::recursive_mutex::scoped_lock lock(mutex_);
    
    return instancesToProcess_.size();
  }


  void ThreadedSetOfInstancesJob::GetFailedInstances(std::set<std::string>& target) const
  {
    boost::recursive_mutex::scoped_lock lock(mutex_);

    target = failedInstances_;
  }


  void ThreadedSetOfInstancesJob::GetInstances(std::set<std::string>& target) const
  {
    boost::recursive_mutex::scoped_lock lock(mutex_);

    target = instancesToProcess_;
  }


  bool ThreadedSetOfInstancesJob::IsFailedInstance(const std::string &instance) const
  {
    boost::recursive_mutex::scoped_lock lock(mutex_);

    return failedInstances_.find(instance) != failedInstances_.end();
  }


  void ThreadedSetOfInstancesJob::Start()
  {
    boost::recursive_mutex::scoped_lock lock(mutex_);

    started_ = true;
  }


  // Reset is called when resubmitting a failed job
  void ThreadedSetOfInstancesJob::Reset()
  {
    boost::recursive_mutex::scoped_lock lock(mutex_);

    if (started_)
    {
      // We actually cannot clean the instances that would have been generated during a previous run
      // because the generated instances may or may not have the same orthanc ids as the source
      // it is too dangerous to guess if they should be deleted or not
      currentStep_ = ThreadedJobStep_NotStarted;
      stopRequested_ = false;
      processedInstances_.clear();
      failedInstances_.clear();
      instancesToProcessQueue_.Clear();
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }


  static const char* KEY_FAILED_INSTANCES = "FailedInstances";
  static const char* KEY_PARENT_RESOURCES = "ParentResources";
  static const char* KEY_DESCRIPTION = "Description";
  static const char* KEY_PERMISSIVE = "Permissive";
  static const char* KEY_CURRENT_STEP = "CurrentStep";
  static const char* KEY_TYPE = "Type";
  static const char* KEY_INSTANCES = "Instances";
  static const char* KEY_INSTANCES_COUNT = "InstancesCount";
  static const char* KEY_FAILED_INSTANCES_COUNT = "FailedInstancesCount";
  static const char* KEY_KEEP_SOURCE = "KeepSource";
  static const char* KEY_WORKERS_COUNT = "WorkersCount";


  void ThreadedSetOfInstancesJob::GetPublicContent(Json::Value& target)
  {
    boost::recursive_mutex::scoped_lock lock(mutex_);

    target[KEY_DESCRIPTION] = GetDescription();
    target[KEY_INSTANCES_COUNT] = static_cast<uint32_t>(GetInstancesCount());
    target[KEY_FAILED_INSTANCES_COUNT] = static_cast<uint32_t>(failedInstances_.size());

    if (!parentResources_.empty())
    {
      SerializationToolbox::WriteSetOfStrings(target, parentResources_, KEY_PARENT_RESOURCES);
    }
  }


  bool ThreadedSetOfInstancesJob::Serialize(Json::Value& target)
  {
    boost::recursive_mutex::scoped_lock lock(mutex_);

    target = Json::objectValue;

    std::string type;
    GetJobType(type);
    target[KEY_TYPE] = type;
    
    target[KEY_PERMISSIVE] = permissive_;
    target[KEY_CURRENT_STEP] = static_cast<unsigned int>(currentStep_);
    target[KEY_DESCRIPTION] = description_;
    target[KEY_KEEP_SOURCE] = keepSource_;
    target[KEY_WORKERS_COUNT] = static_cast<unsigned int>(workersCount_);
    
    SerializationToolbox::WriteSetOfStrings(target, instancesToProcess_, KEY_INSTANCES);
    SerializationToolbox::WriteSetOfStrings(target, failedInstances_, KEY_FAILED_INSTANCES);
    SerializationToolbox::WriteSetOfStrings(target, parentResources_, KEY_PARENT_RESOURCES);

    return true;
  }


  ThreadedSetOfInstancesJob::ThreadedSetOfInstancesJob(ServerContext& context,
                                                       const Json::Value& source,
                                                       bool hasPostProcessing,
                                                       bool defaultKeepSource) :
    hasPostProcessing_(hasPostProcessing),
    started_(false),
    stopRequested_(false),
    permissive_(false),
    currentStep_(ThreadedJobStep_NotStarted),
    workersCount_(1),
    context_(context),
    keepSource_(defaultKeepSource),
    errorCode_(ErrorCode_Success)
  {
    SerializationToolbox::ReadSetOfStrings(failedInstances_, source, KEY_FAILED_INSTANCES);

    if (source.isMember(KEY_PARENT_RESOURCES))
    {
      // Backward compatibility with Orthanc <= 1.5.6
      SerializationToolbox::ReadSetOfStrings(parentResources_, source, KEY_PARENT_RESOURCES);
    }
    
    if (source.isMember(KEY_KEEP_SOURCE))
    {
      keepSource_ = SerializationToolbox::ReadBoolean(source, KEY_KEEP_SOURCE);
    }

    if (source.isMember(KEY_WORKERS_COUNT))
    {
      workersCount_ = SerializationToolbox::ReadUnsignedInteger(source, KEY_WORKERS_COUNT);
    }

    if (source.isMember(KEY_INSTANCES))
    {
      SerializationToolbox::ReadSetOfStrings(instancesToProcess_, source, KEY_INSTANCES);
    }

    if (source.isMember(KEY_CURRENT_STEP))
    {
      currentStep_ = static_cast<ThreadedJobStep>(SerializationToolbox::ReadUnsignedInteger(source, KEY_CURRENT_STEP));
    }
  }


  void ThreadedSetOfInstancesJob::SetKeepSource(bool keep)
  {
    boost::recursive_mutex::scoped_lock lock(mutex_);

    if (IsStarted())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    keepSource_ = keep;
  }

  bool ThreadedSetOfInstancesJob::IsKeepSource() const
  {
    boost::recursive_mutex::scoped_lock lock(mutex_);

    return keepSource_;
  }

  float ThreadedSetOfInstancesJob::GetProgress()
  {
    boost::recursive_mutex::scoped_lock lock(mutex_);

    if (GetInstancesCount() == 0)
    {
      return 1;
    }
    else
    {
      size_t totalProgress = GetInstancesCount();
      size_t currentProgress = processedInstances_.size();
      
      if (HasPostProcessingStep())
      {
        ++totalProgress;
        if (currentStep_ > ThreadedJobStep_PostProcessingInstances)
        {
          ++currentProgress;
        }
      }

      if (HasCleanupStep())
      {
        ++totalProgress;
        if (currentStep_ > ThreadedJobStep_Cleanup)
        {
          ++currentProgress;
        }
      }

      return (static_cast<float>(currentProgress) /
              static_cast<float>(totalProgress));
    }
  }


  ThreadedSetOfInstancesJob::ThreadedJobStep ThreadedSetOfInstancesJob::GetCurrentStep() const
  {
    boost::recursive_mutex::scoped_lock lock(mutex_);

    return currentStep_;
  }


  void ThreadedSetOfInstancesJob::SetDescription(const std::string &description)
  {
    boost::recursive_mutex::scoped_lock lock(mutex_);

    description_ = description;
  }


  const std::string& ThreadedSetOfInstancesJob::GetDescription() const
  {
    boost::recursive_mutex::scoped_lock lock(mutex_);

    return description_;
  }

  void ThreadedSetOfInstancesJob::SetErrorCode(ErrorCode errorCode)
  {
    boost::recursive_mutex::scoped_lock lock(mutex_);

    errorCode_ = errorCode;
  }

  ErrorCode ThreadedSetOfInstancesJob::GetErrorCode() const
  {
    boost::recursive_mutex::scoped_lock lock(mutex_);

    return errorCode_;
  }

  bool ThreadedSetOfInstancesJob::IsPermissive() const
  {
    boost::recursive_mutex::scoped_lock lock(mutex_);

    return permissive_;
  }


  void ThreadedSetOfInstancesJob::SetPermissive(bool permissive)
  {
    boost::recursive_mutex::scoped_lock lock(mutex_);

    if (started_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      permissive_ = permissive;
    }
  }


  bool ThreadedSetOfInstancesJob::IsStarted() const
  {
    boost::recursive_mutex::scoped_lock lock(mutex_);

    return started_;
  }


  void ThreadedSetOfInstancesJob::AddInstances(const std::list<std::string>& instances)
  {
    boost::recursive_mutex::scoped_lock lock(mutex_);

    for (std::list<std::string>::const_iterator
           it = instances.begin(); it != instances.end(); ++it)
    {
      instancesToProcess_.insert(*it);
    }
  }


  void ThreadedSetOfInstancesJob::AddParentResource(const std::string &resource)
  {
    boost::recursive_mutex::scoped_lock lock(mutex_);

    parentResources_.insert(resource);
  }

}
