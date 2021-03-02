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

#include "../HttpServer/HttpOutput.h"
#include "../HttpServer/HttpFileSender.h"

#include <json/value.h>

namespace Orthanc
{
  class RestApiOutput
  {
  private:
    HttpOutput&           output_;
    HttpMethod            method_;
    bool                  alreadySent_;
    bool                  convertJsonToXml_;

    void CheckStatus();

    void SignalErrorInternal(HttpStatus status,
			     const char* message,
			     size_t messageSize);

  public:
    RestApiOutput(HttpOutput& output,
                  HttpMethod method);

    ~RestApiOutput();

    void SetConvertJsonToXml(bool convert)
    {
      convertJsonToXml_ = convert;
    }

    bool IsConvertJsonToXml() const
    {
      return convertJsonToXml_;
    }

    void AnswerStream(IHttpStreamAnswer& stream);

    void AnswerJson(const Json::Value& value);

    void AnswerBuffer(const std::string& buffer,
                      MimeType contentType);

    void AnswerBuffer(const void* buffer,
                      size_t length,
                      MimeType contentType);

    void SignalError(HttpStatus status);

    void SignalError(HttpStatus status,
		     const std::string& message);

    void Redirect(const std::string& path);

    void SetCookie(const std::string& name,
                   const std::string& value,
                   unsigned int maxAge = 0);

    void ResetCookie(const std::string& name);

    void Finalize();
  };
}
