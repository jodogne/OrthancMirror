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


#include "HttpOutput.h"

#include <vector>
#include <stdio.h>
#include <boost/lexical_cast.hpp>
#include "../OrthancException.h"
#include "../Toolbox.h"
#include "../../OrthancCppClient/HttpException.h"

namespace Orthanc
{
  void HttpOutput::SendString(const std::string& s)
  {
    if (s.size() > 0)
      Send(&s[0], s.size());
  }

  void HttpOutput::SendOkHeader(const std::string& contentType)
  {
    SendOkHeader(contentType.c_str(), false, 0, NULL);
  }

  void HttpOutput::SendOkHeader(const char* contentType,
                                bool hasContentLength,
                                uint64_t contentLength,
                                const char* contentFilename)
  {
    std::string s = "HTTP/1.1 200 OK\r\n";

    if (contentType && contentType[0] != '\0')
    {
      s += "Content-Type: " + std::string(contentType) + "\r\n";
    }

    if (hasContentLength)
    {
      s += "Content-Length: " + boost::lexical_cast<std::string>(contentLength) + "\r\n";
    }

    if (contentFilename && contentFilename[0] != '\0')
    {
      s += "Content-Disposition: attachment; filename=\"" + std::string(contentFilename) + "\"\r\n";
    }

    s += "\r\n";

    Send(&s[0], s.size());
  }


  void HttpOutput::SendMethodNotAllowedError(const std::string& allowed)
  {
    std::string s = 
      "HTTP/1.1 405 " + std::string(HttpException::GetDescription(Orthanc_HttpStatus_405_MethodNotAllowed)) +
      "\r\nAllow: " + allowed + 
      "\r\n\r\n";
    Send(&s[0], s.size());
  }


  void HttpOutput::SendHeader(Orthanc_HttpStatus status)
  {
    if (status == Orthanc_HttpStatus_200_Ok ||
        status == Orthanc_HttpStatus_405_MethodNotAllowed)
    {
      throw OrthancException("Please use the dedicated methods to this HTTP status code in HttpOutput");
    }
    
    SendHeaderInternal(status);
  }


  void HttpOutput::SendHeaderInternal(Orthanc_HttpStatus status)
  {
    std::string s = "HTTP/1.1 " + 
      boost::lexical_cast<std::string>(status) +
      " " + std::string(HttpException::GetDescription(status)) +
      "\r\n\r\n";
    Send(&s[0], s.size());
  }


  void HttpOutput::AnswerBufferWithContentType(const std::string& buffer,
                                               const std::string& contentType)
  {
    SendOkHeader(contentType.c_str(), true, buffer.size(), NULL);
    SendString(buffer);
  }


  void HttpOutput::AnswerBufferWithContentType(const void* buffer,
                                               size_t size,
                                               const std::string& contentType)
  {
    SendOkHeader(contentType.c_str(), true, size, NULL);
    Send(buffer, size);
  }


  void HttpOutput::AnswerFileWithContentType(const std::string& path,
                                             const std::string& contentType,
                                             const char* filename)
  {
    uint64_t fileSize = Toolbox::GetFileSize(path);
  
    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp)
    {
      SendHeaderInternal(Orthanc_HttpStatus_500_InternalServerError);
      return;
    }
  
    SendOkHeader(contentType.c_str(), true, fileSize, filename);

    std::vector<uint8_t> buffer(1024 * 1024);  // Chunks of 1MB

    for (;;)
    {
      size_t nbytes = fread(&buffer[0], 1, buffer.size(), fp);
      if (nbytes == 0)
      {
        break;
      }
      else
      {
        Send(&buffer[0], nbytes);
      }
    }

    fclose(fp);
  }


  void HttpOutput::AnswerFileAutodetectContentType(const std::string& path,
                                                   const char* filename)
  {
    AnswerFileWithContentType(path, Toolbox::AutodetectMimeType(path), filename);
  }


  void HttpOutput::AnswerFile(const FileStorage& storage,
                              const std::string& uuid,
                              const std::string& contentType,
                              const char* filename)
  {
    boost::filesystem::path p(storage.GetPath(uuid));
    AnswerFileWithContentType(p.string(), contentType, filename);
  }



  void HttpOutput::Redirect(const std::string& path)
  {
    std::string s = 
      "HTTP/1.1 301 " + std::string(HttpException::GetDescription(Orthanc_HttpStatus_301_MovedPermanently)) + 
      "\r\nLocation: " + path +
      "\r\n\r\n";
    Send(&s[0], s.size());  
  }
}
