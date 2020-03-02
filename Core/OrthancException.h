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

#include "Compatibility.h"
#include "Enumerations.h"
#include "Logging.h"

#include <stdint.h>
#include <string>
#include <memory>

namespace Orthanc
{
  class OrthancException
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
