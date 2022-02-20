/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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
#include "RestApiOutput.h"

#include "../Logging.h"
#include "../OrthancException.h"
#include "../Toolbox.h"

#include <boost/lexical_cast.hpp>


namespace Orthanc
{
  RestApiOutput::RestApiOutput(HttpOutput& output,
                               HttpMethod method) : 
    output_(output),
    method_(method),
    convertJsonToXml_(false)
  {
    alreadySent_ = false;
  }

  RestApiOutput::~RestApiOutput()
  {
  }

  void RestApiOutput::Finalize()
  {
    if (!alreadySent_)
    {
      if (method_ == HttpMethod_Post)
      {
        output_.SendStatus(HttpStatus_400_BadRequest);
      }
      else
      {
        output_.SendStatus(HttpStatus_404_NotFound);
      }
    }
  }
  
  void RestApiOutput::CheckStatus()
  {
    if (alreadySent_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }


  void RestApiOutput::AnswerStream(IHttpStreamAnswer& stream)
  {
    CheckStatus();
    output_.Answer(stream);
    alreadySent_ = true;
  }


  void RestApiOutput::AnswerWithoutBuffering(IHttpStreamAnswer& stream)
  {
    CheckStatus();
    output_.AnswerWithoutBuffering(stream);
    alreadySent_ = true;
  }


  void RestApiOutput::AnswerJson(const Json::Value& value)
  {
    CheckStatus();

    if (convertJsonToXml_)
    {
#if ORTHANC_ENABLE_PUGIXML == 1
      std::string s;
      Toolbox::JsonToXml(s, value);

      output_.SetContentType(MIME_XML_UTF8);
      output_.Answer(s);
#else
      throw OrthancException(ErrorCode_InternalError,
                             "Orthanc was compiled without XML support");
#endif
    }
    else
    {
      std::string s;
      Toolbox::WriteStyledJson(s, value);
      output_.SetContentType(MIME_JSON_UTF8);      
      output_.Answer(s);
    }

    alreadySent_ = true;
  }

  void RestApiOutput::AnswerBuffer(const std::string& buffer,
                                   MimeType contentType)
  {
    AnswerBuffer(buffer.size() == 0 ? NULL : buffer.c_str(),
                 buffer.size(), contentType);
  }

  void RestApiOutput::AnswerBuffer(const void* buffer,
                                   size_t length,
                                   MimeType contentType)
  {
    CheckStatus();

    if (convertJsonToXml_ &&
        contentType == MimeType_Json)
    {
      Json::Value json;
      if (Toolbox::ReadJson(json, buffer, length))
      {
        AnswerJson(json);
      }
      else
      {
        throw OrthancException(ErrorCode_BadFileFormat,
                               "The REST API tries and answers with an invalid JSON file");
      } 
    }
    else
    {
      output_.SetContentType(contentType);
      output_.Answer(buffer, length);
      alreadySent_ = true;
    }
  }

  void RestApiOutput::Redirect(const std::string& path)
  {
    CheckStatus();
    output_.Redirect(path);
    alreadySent_ = true;
  }

  void RestApiOutput::SignalErrorInternal(HttpStatus status,
					  const char* message,
					  size_t messageSize)
  {
    if (status != HttpStatus_400_BadRequest &&
        status != HttpStatus_403_Forbidden &&
        status != HttpStatus_500_InternalServerError &&
        status != HttpStatus_415_UnsupportedMediaType)
    {
      throw OrthancException(ErrorCode_BadHttpStatusInRest);
    }

    CheckStatus();
    output_.SendStatus(status, message, messageSize);
    alreadySent_ = true;    
  }

  void RestApiOutput::SignalError(HttpStatus status)
  {
    SignalErrorInternal(status, NULL, 0);
  }

  void RestApiOutput::SignalError(HttpStatus status,
				  const std::string& message)
  {
    SignalErrorInternal(status, message.c_str(), message.size());
  }

  void RestApiOutput::SetCookie(const std::string& name,
                                const std::string& value,
                                unsigned int maxAge)
  {
    if (name.find(";") != std::string::npos ||
        name.find(" ") != std::string::npos ||
        value.find(";") != std::string::npos ||
        value.find(" ") != std::string::npos)
    {
      throw OrthancException(ErrorCode_NotImplemented);
    }

    CheckStatus();

    std::string v = value + ";path=/";

    if (maxAge != 0)
    {
      v += ";max-age=" + boost::lexical_cast<std::string>(maxAge);
    }

    output_.SetCookie(name, v);
  }

  void RestApiOutput::ResetCookie(const std::string& name)
  {
    // This marks the cookie to be deleted by the browser in 1 second,
    // and before it actually gets deleted, its value is set to the
    // empty string
    SetCookie(name, "", 1);
  }

  void RestApiOutput::SetContentFilename(const char* filename)
  {
    output_.SetContentFilename(filename);
  }
}
