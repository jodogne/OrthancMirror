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
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#pragma once

#include <string>
#include <stdint.h>
#include "../Enumerations.h"
#include "../FileStorage.h"

namespace Orthanc
{
  class HttpOutput
  {
  private:
    void SendHeaderInternal(Orthanc_HttpStatus status);

    void SendOkHeader(const char* contentType,
                      bool hasContentLength,
                      uint64_t contentLength);

  public:
    virtual ~HttpOutput()
    {
    }

    virtual void Send(const void* buffer, size_t length) = 0;

    void SendString(const std::string& s);

    void SendOkHeader();

    void SendOkHeader(uint64_t contentLength);

    void SendOkHeader(const std::string& contentType);

    void SendOkHeader(const std::string& contentType,
                      uint64_t contentLength);

    void SendMethodNotAllowedError(const std::string& allowed);

    void SendHeader(Orthanc_HttpStatus status);


    // Higher-level constructs to send entire files or buffers -------------------

    void AnswerBuffer(const std::string& buffer)
    {
      AnswerBufferWithContentType(buffer, "");
    }

    void AnswerBufferWithContentType(const std::string& buffer,
                                     const std::string& contentType);

    void AnswerBufferWithContentType(const void* buffer,
                                     size_t size,
                                     const std::string& contentType);

    void AnswerFile(const std::string& path)
    {
      AnswerFileWithContentType(path, "");
    }

    void AnswerFileWithContentType(const std::string& path,
                                   const std::string& contentType);

    void AnswerFileAutodetectContentType(const std::string& path); 

    void AnswerFile(const FileStorage& storage,
                    const std::string& uuid)
    {
      AnswerFile(storage, uuid, "");
    }

    void AnswerFile(const FileStorage& storage,
                    const std::string& uuid,
                    const std::string& contentType);

    void Redirect(const std::string& path);
  };
}
