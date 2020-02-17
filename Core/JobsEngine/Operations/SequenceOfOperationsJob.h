/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
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


#pragma once

#include "../IJob.h"
#include "IJobOperation.h"

#include "../../DicomNetworking/TimeoutDicomConnectionManager.h"

#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>

#include <list>

namespace Orthanc
{
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
    class Operation;

    std::string                       description_;
    bool                              done_;
    boost::mutex                      mutex_;
    std::vector<Operation*>           operations_;
    size_t                            current_;
    boost::condition_variable         operationAdded_;
    boost::posix_time::time_duration  trailingTimeout_;
    std::list<IObserver*>             observers_;
    TimeoutDicomConnectionManager     connectionManager_;

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

      void SetTrailingOperationTimeout(unsigned int timeout);

      void SetDicomAssociationTimeout(unsigned int timeout);
      
      size_t AddOperation(IJobOperation* operation);

      size_t GetOperationsCount() const
      {
        return that_.operations_.size();
      }

      void AddInput(size_t index,
                    const JobOperationValue& value);
      
      void Connect(size_t input,
                   size_t output);
    };

    virtual void Start() ORTHANC_OVERRIDE
    {
    }

    virtual JobStepResult Step(const std::string& jobId) ORTHANC_OVERRIDE;

    virtual void Reset() ORTHANC_OVERRIDE;

    virtual void Stop(JobStopReason reason) ORTHANC_OVERRIDE;

    virtual float GetProgress() ORTHANC_OVERRIDE;

    virtual void GetJobType(std::string& target) ORTHANC_OVERRIDE
    {
      target = "SequenceOfOperations";
    }

    virtual void GetPublicContent(Json::Value& value) ORTHANC_OVERRIDE;

    virtual bool Serialize(Json::Value& value) ORTHANC_OVERRIDE;

    virtual bool GetOutput(std::string& output,
                           MimeType& mime,
                           const std::string& key) ORTHANC_OVERRIDE
    {
      return false;
    }

    void AwakeTrailingSleep()
    {
      operationAdded_.notify_one();
    }
  };
}
