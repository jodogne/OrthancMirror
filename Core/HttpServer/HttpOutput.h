/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2014 Medical Physics Department, CHU of Liege,
 * Belgium
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

#include <list>
#include <string>
#include <stdint.h>
#include "../Enumerations.h"
#include "IHttpOutputStream.h"
#include "HttpHandler.h"

namespace Orthanc
{
  class HttpOutput
  {
  private:
    typedef std::list< std::pair<std::string, std::string> >  Header;

    class StateMachine : public boost::noncopyable
    {
    private:
      enum State
      {
        State_WaitingHttpStatus,
        State_WritingHeader,      
        State_WritingBody
      };

      IHttpOutputStream& stream_;
      State state_;

    public:
      StateMachine(IHttpOutputStream& stream) : 
        stream_(stream),
        state_(State_WaitingHttpStatus)
      {
      }

      void SendHttpStatus(HttpStatus status);

      void SendHeaderData(const void* buffer, size_t length);

      void SendHeaderString(const std::string& str);

      void SendBodyData(const void* buffer, size_t length);

      void SendBodyString(const std::string& str);
    };

    void PrepareOkHeader(Header& header,
                         const char* contentType,
                         bool hasContentLength,
                         uint64_t contentLength,
                         const char* contentFilename);

    void SendOkHeader(const Header& header);

    void PrepareCookies(Header& header,
                        const HttpHandler::Arguments& cookies);

    StateMachine stateMachine_;

  public:
    HttpOutput(IHttpOutputStream& stream) : stateMachine_(stream)
    {
    }

    void SendOkHeader(const char* contentType,
                      bool hasContentLength,
                      uint64_t contentLength,
                      const char* contentFilename);

    void SendBodyData(const void* buffer, size_t length)
    {
      stateMachine_.SendBodyData(buffer, length);
    }

    void SendBodyString(const std::string& str)
    {
      stateMachine_.SendBodyString(str);
    }

    void SendMethodNotAllowedError(const std::string& allowed);

    void SendHeader(HttpStatus status);

    void Redirect(const std::string& path);

    void SendUnauthorized(const std::string& realm);

    // Higher-level constructs to send entire buffers ----------------------------

    void AnswerBufferWithContentType(const std::string& buffer,
                                     const std::string& contentType);

    void AnswerBufferWithContentType(const std::string& buffer,
                                     const std::string& contentType,
                                     const HttpHandler::Arguments& cookies);

    void AnswerBufferWithContentType(const void* buffer,
                                     size_t size,
                                     const std::string& contentType);

    void AnswerBufferWithContentType(const void* buffer,
                                     size_t size,
                                     const std::string& contentType,
                                     const HttpHandler::Arguments& cookies);
  };
}
