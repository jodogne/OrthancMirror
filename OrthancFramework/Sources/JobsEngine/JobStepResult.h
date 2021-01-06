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

#include "../Enumerations.h"

namespace Orthanc
{
  class OrthancException;
  
  class ORTHANC_PUBLIC JobStepResult
  {
  private:
    JobStepCode   code_;
    unsigned int  timeout_;
    ErrorCode     error_;
    std::string   failureDetails_;
    
    explicit JobStepResult(JobStepCode code) :
      code_(code),
      timeout_(0),
      error_(ErrorCode_Success)
    {
    }

  public:
    explicit JobStepResult();

    static JobStepResult Success();

    static JobStepResult Continue();

    static JobStepResult Retry(unsigned int timeout);

    static JobStepResult Failure(const ErrorCode& error,
                                 const char* details);

    static JobStepResult Failure(const OrthancException& exception);

    JobStepCode GetCode() const;

    unsigned int GetRetryTimeout() const;

    ErrorCode GetFailureCode() const;

    const std::string& GetFailureDetails() const;
  };
}
