/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2018 Osimis S.A., Belgium
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

  void RestApiOutput::AnswerJson(const Json::Value& value)
  {
    CheckStatus();

    if (convertJsonToXml_)
    {
#if ORTHANC_ENABLE_PUGIXML == 1
      std::string s;
      Toolbox::JsonToXml(s, value);
      output_.SetContentType("application/xml; charset=utf-8");
      output_.Answer(s);
#else
      LOG(ERROR) << "Orthanc was compiled without XML support";
      throw OrthancException(ErrorCode_InternalError);
#endif
    }
    else
    {
      Json::StyledWriter writer;
      output_.SetContentType("application/json; charset=utf-8");
      output_.Answer(writer.write(value));
    }

    alreadySent_ = true;
  }

  void RestApiOutput::AnswerBuffer(const std::string& buffer,
                                   const std::string& contentType)
  {
    AnswerBuffer(buffer.size() == 0 ? NULL : buffer.c_str(),
                 buffer.size(), contentType);
  }

  void RestApiOutput::AnswerBuffer(const void* buffer,
                                   size_t length,
                                   const std::string& contentType)
  {
    CheckStatus();
    output_.SetContentType(contentType.c_str());
    output_.Answer(buffer, length);
    alreadySent_ = true;
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
}
