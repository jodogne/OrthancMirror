/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2021-2023 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#pragma once

#include "../../../OrthancFramework/Sources/Compatibility.h"  // For ORTHANC_OVERRIDE
#include "../../../OrthancFramework/Sources/JobsEngine/IJob.h"
#include "../../../OrthancFramework/Sources/MultiThreading/SharedMessageQueue.h"

#include <set>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread.hpp>

namespace Orthanc
{
  class ServerContext;

  // This class is a threaded version of SetOfInstancesJob merged with CleaningInstancesJob
  class ORTHANC_PUBLIC ThreadedSetOfInstancesJob : public IJob
  {
  public:
    enum ThreadedJobStep  // cannot use "Step" since there is a method with this name !
    {
      ThreadedJobStep_NotStarted,
      ThreadedJobStep_ProcessingInstances,
      ThreadedJobStep_PostProcessingInstances,
      ThreadedJobStep_Cleanup,
      ThreadedJobStep_Done
    };

  private:
    std::set<std::string>               instancesToProcess_;  // the list of source instances ids to process
    std::set<std::string>               failedInstances_;     // the list of source instances ids that failed processing
    std::set<std::string>               processedInstances_;  // the list of source instances ids that have been processed (including failed ones)

    SharedMessageQueue                  instancesToProcessQueue_;
    std::vector<boost::shared_ptr<boost::thread> >         instancesWorkers_;

    bool                    hasPostProcessing_;  // final step before "KeepSource" cleanup
    bool                    started_;
    bool                    stopRequested_;
    bool                    permissive_;
    ThreadedJobStep         currentStep_;
    std::string             description_;
    size_t                  workersCount_;

    ServerContext&          context_;
    bool                    keepSource_;
    ErrorCode               errorCode_;
  
  protected:
    mutable boost::recursive_mutex      mutex_;
    std::set<std::string>               parentResources_;

  public:
    ThreadedSetOfInstancesJob(ServerContext& context,
                              bool hasTrailingStep,
                              bool keepSource,
                              size_t workersCount);

    explicit ThreadedSetOfInstancesJob(ServerContext& context,
                                       const Json::Value& source,
                                       bool hasTrailingStep,
                                       bool defaultKeepSource);

    virtual ~ThreadedSetOfInstancesJob();

  protected:
    virtual bool HandleInstance(const std::string& instance) = 0;

    virtual void PostProcessInstances();

    void InitWorkers(size_t workersCount);

    void StopWorkers();

    void WaitWorkersComplete();

    static void InstanceWorkerThread(ThreadedSetOfInstancesJob* that);

    const std::string& GetInstance(size_t index) const;

    bool HasPostProcessingStep() const;

    bool HasCleanupStep() const;

    void SetErrorCode(ErrorCode errorCode);

    ErrorCode GetErrorCode() const;

  public:

    ThreadedJobStep GetCurrentStep() const;

    void SetDescription(const std::string& description);

    const std::string& GetDescription() const;

    void SetKeepSource(bool keep);

    bool IsKeepSource() const;

    void GetInstances(std::set<std::string>& target) const;

    void GetFailedInstances(std::set<std::string>& target) const;

    size_t GetInstancesCount() const;

    void AddInstances(const std::list<std::string>& instances);

    void AddParentResource(const std::string &resource);

    bool IsPermissive() const;

    void SetPermissive(bool permissive);

    virtual void Reset() ORTHANC_OVERRIDE;
    
    virtual void Start() ORTHANC_OVERRIDE;
    
    virtual void Stop(JobStopReason reason) ORTHANC_OVERRIDE;

    virtual float GetProgress() ORTHANC_OVERRIDE;

    bool IsStarted() const;

    virtual JobStepResult Step(const std::string& jobId) ORTHANC_OVERRIDE;
    
    virtual void GetPublicContent(Json::Value& value) ORTHANC_OVERRIDE;
    
    virtual bool Serialize(Json::Value& target) ORTHANC_OVERRIDE;

    virtual bool GetOutput(std::string& output,
                           MimeType& mime,
                           std::string& filename,
                           const std::string& key) ORTHANC_OVERRIDE;

    bool IsFailedInstance(const std::string& instance) const;

    ServerContext& GetContext() const
    {
      return context_;
    }

  };
}
