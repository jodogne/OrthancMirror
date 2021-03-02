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


#include "../PrecompiledHeaders.h"
#include "JobStatus.h"

#include "../OrthancException.h"

namespace Orthanc
{
  JobStatus::JobStatus() :
    errorCode_(ErrorCode_InternalError),
    progress_(0),
    jobType_("Invalid"),
    publicContent_(Json::objectValue),
    hasSerialized_(false)
  {
  }

  
  JobStatus::JobStatus(ErrorCode code,
                       const std::string& details,
                       IJob& job) :
    errorCode_(code),
    progress_(job.GetProgress()),
    publicContent_(Json::objectValue),
    details_(details)
  {
    if (progress_ < 0)
    {
      progress_ = 0;
    }
      
    if (progress_ > 1)
    {
      progress_ = 1;
    }

    job.GetJobType(jobType_);
    job.GetPublicContent(publicContent_);

    hasSerialized_ = job.Serialize(serialized_);
  }


  const Json::Value& JobStatus::GetSerialized() const
  {
    if (!hasSerialized_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      return serialized_;
    }
  }
}
