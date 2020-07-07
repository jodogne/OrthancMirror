/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
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

#include "Compatibility.h"
#include "Enumerations.h"
#include "Logging.h"
#include "OrthancFramework.h"

#include <stdint.h>
#include <string>
#include <memory>

namespace Orthanc
{
  class ORTHANC_PUBLIC OrthancException
  {
  private:
    OrthancException();  // Forbidden
    
    OrthancException& operator= (const OrthancException&);  // Forbidden

    ErrorCode  errorCode_;
    HttpStatus httpStatus_;

    // New in Orthanc 1.5.0
    std::unique_ptr<std::string>  details_;
    
  public:
    OrthancException(const OrthancException& other) : 
      errorCode_(other.errorCode_),
      httpStatus_(other.httpStatus_)
    {
      if (other.details_.get() != NULL)
      {
        details_.reset(new std::string(*other.details_));
      }
    }

    explicit OrthancException(ErrorCode errorCode) : 
      errorCode_(errorCode),
      httpStatus_(ConvertErrorCodeToHttpStatus(errorCode))
    {
    }

    OrthancException(ErrorCode errorCode,
                     const std::string& details,
                     bool log = true) :
      errorCode_(errorCode),
      httpStatus_(ConvertErrorCodeToHttpStatus(errorCode)),
      details_(new std::string(details))
    {
#if ORTHANC_ENABLE_LOGGING == 1
      if (log)
      {
        LOG(ERROR) << EnumerationToString(errorCode_) << ": " << details;
      }
#endif
    }

    OrthancException(ErrorCode errorCode,
                     HttpStatus httpStatus) :
      errorCode_(errorCode),
      httpStatus_(httpStatus)
    {
    }

    OrthancException(ErrorCode errorCode,
                     HttpStatus httpStatus,
                     const std::string& details,
                     bool log = true) :
      errorCode_(errorCode),
      httpStatus_(httpStatus),
      details_(new std::string(details))
    {
#if ORTHANC_ENABLE_LOGGING == 1
      if (log)
      {
        LOG(ERROR) << EnumerationToString(errorCode_) << ": " << details;
      }
#endif
    }

    ErrorCode GetErrorCode() const
    {
      return errorCode_;
    }

    HttpStatus GetHttpStatus() const
    {
      return httpStatus_;
    }

    const char* What() const
    {
      return EnumerationToString(errorCode_);
    }

    bool HasDetails() const
    {
      return details_.get() != NULL;
    }

    const char* GetDetails() const
    {
      if (details_.get() == NULL)
      {
        return "";
      }
      else
      {
        return details_->c_str();
      }
    }
  };
}
