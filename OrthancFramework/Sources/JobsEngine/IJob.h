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

#include "JobStepResult.h"

#include <boost/noncopyable.hpp>
#include <json/value.h>

namespace Orthanc
{
  class ORTHANC_PUBLIC IJob : public boost::noncopyable
  {
  public:
    virtual ~IJob()
    {
    }

    // Method called once the job enters the jobs engine
    virtual void Start() = 0;
    
    virtual JobStepResult Step(const std::string& jobId) = 0;

    // Method called once the job is resubmitted after a failure
    virtual void Reset() = 0;

    // For pausing/canceling/ending jobs: This method must release allocated resources
    virtual void Stop(JobStopReason reason) = 0;

    virtual float GetProgress() = 0;

    virtual void GetJobType(std::string& target) = 0;
    
    virtual void GetPublicContent(Json::Value& value) = 0;

    virtual bool Serialize(Json::Value& value) = 0;

    // This function can only be called if the job has reached its
    // "success" state
    virtual bool GetOutput(std::string& output,
                           MimeType& mime,
                           const std::string& key) = 0;
  };
}
