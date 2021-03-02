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


#include "PrecompiledHeaders.h"
#include "OrthancException.h"

#include "Logging.h"


namespace Orthanc
{
  OrthancException::OrthancException(const OrthancException& other) : 
    errorCode_(other.errorCode_),
    httpStatus_(other.httpStatus_)
  {
    if (other.details_.get() != NULL)
    {
      details_.reset(new std::string(*other.details_));
    }
  }

  OrthancException::OrthancException(ErrorCode errorCode) : 
    errorCode_(errorCode),
    httpStatus_(ConvertErrorCodeToHttpStatus(errorCode))
  {
  }

  OrthancException::OrthancException(ErrorCode errorCode,
                                     const std::string& details,
                                     bool log) :
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

  OrthancException::OrthancException(ErrorCode errorCode,
                                     HttpStatus httpStatus) :
    errorCode_(errorCode),
    httpStatus_(httpStatus)
  {
  }

  OrthancException::OrthancException(ErrorCode errorCode,
                                     HttpStatus httpStatus,
                                     const std::string& details,
                                     bool log) :
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

  ErrorCode OrthancException::GetErrorCode() const
  {
    return errorCode_;
  }

  HttpStatus OrthancException::GetHttpStatus() const
  {
    return httpStatus_;
  }

  const char* OrthancException::What() const
  {
    return EnumerationToString(errorCode_);
  }

  bool OrthancException::HasDetails() const
  {
    return details_.get() != NULL;
  }

  const char* OrthancException::GetDetails() const
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
}
