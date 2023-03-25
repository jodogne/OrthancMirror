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


#include "../PrecompiledHeaders.h"
#include "JobStepResult.h"

#include "../OrthancException.h"

namespace Orthanc
{
  JobStepResult::JobStepResult() :
    code_(JobStepCode_Failure),
    timeout_(0),
    error_(ErrorCode_InternalError)
  {
  }

  JobStepResult JobStepResult::Success()
  {
    return JobStepResult(JobStepCode_Success);
  }

  JobStepResult JobStepResult::Continue()
  {
    return JobStepResult(JobStepCode_Continue);
  }

  JobStepResult JobStepResult::Retry(unsigned int timeout)
  {
    JobStepResult result(JobStepCode_Retry);
    result.timeout_ = timeout;
    return result;
  }


  JobStepResult JobStepResult::Failure(const ErrorCode& error,
                                       const char* details)
  {
    JobStepResult result(JobStepCode_Failure);
    result.error_ = error;

    if (details != NULL)
    {
      result.failureDetails_ = details;
    }
    
    return result;
  }


  JobStepResult JobStepResult::Failure(const OrthancException& exception)
  {
    return Failure(exception.GetErrorCode(),
                   exception.HasDetails() ? exception.GetDetails() : NULL);
  }

  JobStepCode JobStepResult::GetCode() const
  {
    return code_;
  }
  

  unsigned int JobStepResult::GetRetryTimeout() const
  {
    if (code_ == JobStepCode_Retry)
    {
      return timeout_;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }


  ErrorCode JobStepResult::GetFailureCode() const
  {
    if (code_ == JobStepCode_Failure)
    {
      return error_;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }


  const std::string& JobStepResult::GetFailureDetails() const
  {
    if (code_ == JobStepCode_Failure)
    {
      return failureDetails_;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }
}
