/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
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

#include "../IJob.h"
#include "IJobOperation.h"

#include "../../Compatibility.h"  // For ORTHANC_OVERRIDE

#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>

#include <list>

namespace Orthanc
{
  class ORTHANC_PUBLIC SequenceOfOperationsJob : public IJob
  {
  public:
    class ORTHANC_PUBLIC IObserver : public boost::noncopyable
    {
    public:
      virtual ~IObserver()
      {
      }

      virtual void SignalDone(const SequenceOfOperationsJob& job) = 0;
    };

  private:
    class Operation;

    std::string                       description_;
    bool                              done_;
    boost::mutex                      mutex_;
    std::vector<Operation*>           operations_;
    size_t                            current_;
    boost::condition_variable         operationAdded_;
    boost::posix_time::time_duration  trailingTimeout_;
    std::list<IObserver*>             observers_;

    void NotifyDone() const;

  public:
    SequenceOfOperationsJob();

    SequenceOfOperationsJob(IJobUnserializer& unserializer,
                            const Json::Value& serialized);

    virtual ~SequenceOfOperationsJob();

    void SetDescription(const std::string& description);

    void GetDescription(std::string& description);

    void Register(IObserver& observer);

    // This lock allows adding new operations to the end of the job,
    // from another thread than the worker thread, after the job has
    // been submitted for processing
    class ORTHANC_PUBLIC Lock : public boost::noncopyable
    {
    private:
      SequenceOfOperationsJob&   that_;
      boost::mutex::scoped_lock  lock_;

#if ORTHANC_BUILDING_FRAMEWORK_LIBRARY == 1
      ORTHANC_DEPRECATED(void AddInput(size_t index,
                                       const JobOperationValue& value));
#endif
      
    public:
      explicit Lock(SequenceOfOperationsJob& that);

      bool IsDone() const;

      void SetTrailingOperationTimeout(unsigned int timeout);
      
      size_t AddOperation(IJobOperation* operation);

      size_t GetOperationsCount() const;

      void AddInput(size_t index,
                    const IJobOperationValue& value);
      
      void Connect(size_t input,
                   size_t output);
    };

    virtual void Start() ORTHANC_OVERRIDE;

    virtual JobStepResult Step(const std::string& jobId) ORTHANC_OVERRIDE;

    virtual void Reset() ORTHANC_OVERRIDE;

    virtual void Stop(JobStopReason reason) ORTHANC_OVERRIDE;

    virtual float GetProgress() ORTHANC_OVERRIDE;

    virtual void GetJobType(std::string& target) ORTHANC_OVERRIDE;

    virtual void GetPublicContent(Json::Value& value) ORTHANC_OVERRIDE;

    virtual bool Serialize(Json::Value& value) ORTHANC_OVERRIDE;

    virtual bool GetOutput(std::string& output,
                           MimeType& mime,
                           const std::string& key) ORTHANC_OVERRIDE;

    void AwakeTrailingSleep();
  };
}
