/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2013 Medical Physics Department, CHU of Liege,
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


#include "HttpOutput.h"

#include <iostream>
#include <vector>
#include <stdio.h>
#include <boost/lexical_cast.hpp>
#include "../OrthancException.h"
#include "../Toolbox.h"

namespace Orthanc
{
  void HttpOutput::SendString(const std::string& s)
  {
    if (s.size() > 0)
    {
      Send(&s[0], s.size());
    }
  }

  void HttpOutput::PrepareOkHeader(Header& header,
                                   const char* contentType,
                                   bool hasContentLength,
                                   uint64_t contentLength,
                                   const char* contentFilename)
  {
    header.clear();

    if (contentType && contentType[0] != '\0')
    {
      header.push_back(std::make_pair("Content-Type", std::string(contentType)));
    }

    if (hasContentLength)
    {
      header.push_back(std::make_pair("Content-Length", boost::lexical_cast<std::string>(contentLength)));
    }

    if (contentFilename && contentFilename[0] != '\0')
    {
      std::string attachment = "attachment; filename=\"" + std::string(contentFilename) + "\"";
      header.push_back(std::make_pair("Content-Disposition", attachment));
    }
  }

  void HttpOutput::SendOkHeader(const char* contentType,
                                bool hasContentLength,
                                uint64_t contentLength,
                                const char* contentFilename)
  {
    Header header;
    PrepareOkHeader(header, contentType, hasContentLength, contentLength, contentFilename);
    SendOkHeader(header);
  }

  void HttpOutput::SendOkHeader(const Header& header)
  {
    std::string s = "HTTP/1.1 200 OK\r\n";

    for (Header::const_iterator 
           it = header.begin(); it != header.end(); ++it)
    {
      s += it->first + ": " + it->second + "\r\n";
    }

    s += "\r\n";

    Send(&s[0], s.size());
  }


  void HttpOutput::SendMethodNotAllowedError(const std::string& allowed)
  {
    std::string s = 
      "HTTP/1.1 405 " + std::string(EnumerationToString(HttpStatus_405_MethodNotAllowed)) +
      "\r\nAllow: " + allowed + 
      "\r\n\r\n";
    Send(&s[0], s.size());
  }


  void HttpOutput::SendHeader(HttpStatus status)
  {
    if (status == HttpStatus_200_Ok ||
        status == HttpStatus_405_MethodNotAllowed)
    {
      throw OrthancException("Please use the dedicated methods to this HTTP status code in HttpOutput");
    }
    
    SendHeaderInternal(status);
  }


  void HttpOutput::SendHeaderInternal(HttpStatus status)
  {
    std::string s = "HTTP/1.1 " + 
      boost::lexical_cast<std::string>(status) +
      " " + std::string(EnumerationToString(status)) +
      "\r\n\r\n";
    Send(&s[0], s.size());
  }


  void HttpOutput::AnswerBufferWithContentType(const std::string& buffer,
                                               const std::string& contentType)
  {
    SendOkHeader(contentType.c_str(), true, buffer.size(), NULL);
    SendString(buffer);
  }


  void HttpOutput::PrepareCookies(Header& header,
                                  const HttpHandler::Arguments& cookies)
  {
    for (HttpHandler::Arguments::const_iterator it = cookies.begin();
         it != cookies.end(); ++it)
    {
      header.push_back(std::make_pair("Set-Cookie", it->first + "=" + it->second));
    }
  }


  void HttpOutput::AnswerBufferWithContentType(const std::string& buffer,
                                               const std::string& contentType,
                                               const HttpHandler::Arguments& cookies)
  {
    Header header;
    PrepareOkHeader(header, contentType.c_str(), true, buffer.size(), NULL);
    PrepareCookies(header, cookies);
    SendOkHeader(header);
    SendString(buffer);
  }


  void HttpOutput::AnswerBufferWithContentType(const void* buffer,
                                               size_t size,
                                               const std::string& contentType)
  {
    SendOkHeader(contentType.c_str(), true, size, NULL);
    Send(buffer, size);
  }


  void HttpOutput::AnswerBufferWithContentType(const void* buffer,
                                               size_t size,
                                               const std::string& contentType,
                                               const HttpHandler::Arguments& cookies)
  {
    Header header;
    PrepareOkHeader(header, contentType.c_str(), true, size, NULL);
    PrepareCookies(header, cookies);
    SendOkHeader(header);
    Send(buffer, size);
  }



  void HttpOutput::Redirect(const std::string& path)
  {
    std::string s = 
      "HTTP/1.1 301 " + std::string(EnumerationToString(HttpStatus_301_MovedPermanently)) + 
      "\r\nLocation: " + path +
      "\r\n\r\n";
    Send(&s[0], s.size());  
  }
}
