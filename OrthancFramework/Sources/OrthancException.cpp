/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2025 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2025 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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
  ErrorPayload::ErrorPayload(const ErrorPayload& other) :
    type_(other.type_)
  {
    if (other.content_.get() != NULL)
    {
      content_.reset(new Json::Value(*other.content_));
    }
  }


  ErrorPayload& ErrorPayload::operator= (const ErrorPayload& other)
  {
    if (other.HasContent())
    {
      SetContent(other.GetType(), other.GetContent());
    }
    else
    {
      ClearContent();
    }

    return *this;
  }


  void ErrorPayload::ClearContent()
  {
    type_ = ErrorPayloadType_None;
    content_.reset(NULL);
  }


  void ErrorPayload::SetContent(ErrorPayloadType type,
                                const Json::Value& content)
  {
    if (type == ErrorPayloadType_None ||
        content.isNull())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
    else
    {
      type_ = type;
      content_.reset(new Json::Value(content));
    }
  }


  ErrorPayloadType ErrorPayload::GetType() const
  {
    if (HasContent())
    {
      return type_;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }


  const Json::Value& ErrorPayload::GetContent() const
  {
    if (HasContent())
    {
      return *content_;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }


  void ErrorPayload::Format(Json::Value& target) const
  {
    target["Type"] = EnumerationToString(GetType());
    target["Content"] = GetContent();
  }


  OrthancException::OrthancException(const OrthancException& other) : 
    errorCode_(other.errorCode_),
    httpStatus_(other.httpStatus_),
    logged_(false),
    payload_(other.payload_)
  {
    if (other.details_.get() != NULL)
    {
      details_.reset(new std::string(*other.details_));
    }
  }

  OrthancException::OrthancException(ErrorCode errorCode) : 
    errorCode_(errorCode),
    httpStatus_(ConvertErrorCodeToHttpStatus(errorCode)),
    logged_(false)
  {
  }

  OrthancException::OrthancException(ErrorCode errorCode,
                                     const std::string& details,
                                     LogException log) :
    errorCode_(errorCode),
    httpStatus_(ConvertErrorCodeToHttpStatus(errorCode)),
    logged_(log == LogException_Yes),
    details_(new std::string(details))
  {
#if ORTHANC_ENABLE_LOGGING == 1
    if (log == LogException_Yes)
    {
      LOG(ERROR) << EnumerationToString(errorCode_) << ": " << details;
    }
#endif
  }


  OrthancException::OrthancException(ErrorCode errorCode,
                                     HttpStatus httpStatus) :
    errorCode_(errorCode),
    httpStatus_(httpStatus),
    logged_(false)
  {
  }


  OrthancException::OrthancException(ErrorCode errorCode,
                                     const std::string& details,
                                     ErrorPayloadType payloadType,
                                     const Json::Value& payload,
                                     LogException log) :
    errorCode_(errorCode),
    httpStatus_(ConvertErrorCodeToHttpStatus(errorCode)),
    logged_(log == LogException_Yes),
    details_(new std::string(details))
  {
    payload_.SetContent(payloadType, payload);

#if ORTHANC_ENABLE_LOGGING == 1
    if (log == LogException_Yes)
    {
      LOG(ERROR) << EnumerationToString(errorCode_) << ": " << details;
    }
#endif
  }


  OrthancException::OrthancException(ErrorCode errorCode,
                                     HttpStatus httpStatus,
                                     const std::string& details,
                                     ErrorPayloadType payloadType,
                                     const Json::Value& payload,
                                     LogException log) :
    errorCode_(errorCode),
    httpStatus_(httpStatus),
    logged_(log == LogException_Yes),
    details_(new std::string(details))
  {
    payload_.SetContent(payloadType, payload);

#if ORTHANC_ENABLE_LOGGING == 1
    if (log == LogException_Yes)
    {
      LOG(ERROR) << EnumerationToString(errorCode_) << ": " << details;
    }
#endif
  }

  OrthancException::OrthancException(ErrorCode errorCode,
                                     HttpStatus httpStatus,
                                     const std::string& details,
                                     LogException log) :
    errorCode_(errorCode),
    httpStatus_(httpStatus),
    logged_(log == LogException_Yes),
    details_(new std::string(details))
  {
#if ORTHANC_ENABLE_LOGGING == 1
    if (log == LogException_Yes)
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
