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


#pragma once

#include "Compatibility.h"  // For std::unique_ptr<>
#include "Enumerations.h"
#include "OrthancFramework.h"

#include <json/value.h>
#include <stdint.h>  // For uint16_t

namespace Orthanc
{
  class ORTHANC_PUBLIC ErrorPayload
  {
  private:
    ErrorPayloadType              type_;
    std::unique_ptr<Json::Value>  content_;

  public:
    ErrorPayload() :
      type_(ErrorPayloadType_None)
    {
    }

    ErrorPayload(const ErrorPayload& other);

    ErrorPayload& operator= (const ErrorPayload& other);

    void ClearContent();

    void SetContent(ErrorPayloadType type,
                    const Json::Value& content);

    bool HasContent() const
    {
      return content_.get() != NULL;
    }

    ErrorPayloadType GetType() const;

    const Json::Value& GetContent() const;

    void Format(Json::Value& target) const;
  };


  // TODO: Shouldn't copies of OrthancException be avoided completely
  // (i.e., tag OrthancException as boost::noncopyable)?
  class ORTHANC_PUBLIC OrthancException
  {
  private:
    OrthancException();  // Forbidden
    
    OrthancException& operator= (const OrthancException&);  // Forbidden

    ErrorCode  errorCode_;
    HttpStatus httpStatus_;
    bool       logged_;    // has the exception already been logged ?  (to avoid double logs)

    // New in Orthanc 1.5.0
    std::unique_ptr<std::string>  details_;
    
    // New in Orthanc 1.12.10
    ErrorPayload  payload_;

  public:
    OrthancException(const OrthancException& other);

    explicit OrthancException(ErrorCode errorCode);

    OrthancException(ErrorCode errorCode,
                     const std::string& details,
                     bool log = true);

    ErrorCode GetErrorCode() const;

    HttpStatus GetHttpStatus() const;

    const char* What() const;

    bool HasDetails() const;

    const char* GetDetails() const;

    bool HasBeenLogged() const
    {
      return logged_;
    }

    OrthancException& SetHttpStatus(HttpStatus status)
    {
      httpStatus_ = status;
      return *this;
    }

    OrthancException& SetPayload(const ErrorPayload& payload)
    {
      payload_ = payload;
      return *this;
    }

    OrthancException& SetPayload(ErrorPayloadType type,
                                 const Json::Value& content)
    {
      payload_.SetContent(type, content);
      return *this;
    }

    const ErrorPayload& GetPayload() const
    {
      return payload_;
    }
  };
}
