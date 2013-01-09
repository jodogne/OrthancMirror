/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012 Medical Physics Department, CHU of Liege,
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
#include "HttpHandler.h"

namespace Orthanc
{
  class HttpOutput
  {
  private:
    typedef std::list< std::pair<std::string, std::string> >  Header;

    void SendHeaderInternal(Orthanc_HttpStatus status);

    void PrepareOkHeader(Header& header,
                         const char* contentType,
                         bool hasContentLength,
                         uint64_t contentLength,
                         const char* contentFilename);

    void SendOkHeader(const Header& header);

  public:
    virtual ~HttpOutput()
    {
    }

    virtual void Send(const void* buffer, size_t length) = 0;

    void SendOkHeader(const char* contentType,
                      bool hasContentLength,
                      uint64_t contentLength,
                      const char* contentFilename);

    void SendString(const std::string& s);

    void SendMethodNotAllowedError(const std::string& allowed);

    void SendHeader(Orthanc_HttpStatus status);

    void Redirect(const std::string& path);

    // Higher-level constructs to send entire buffers ----------------------------

    void AnswerBufferWithContentType(const std::string& buffer,
                                     const std::string& contentType);

    void AnswerBufferWithContentType(const std::string& buffer,
                                     const std::string& contentType,
                                     const HttpHandler::Arguments& cookies);

    void AnswerBufferWithContentType(const void* buffer,
                                     size_t size,
                                     const std::string& contentType);
  };
}
