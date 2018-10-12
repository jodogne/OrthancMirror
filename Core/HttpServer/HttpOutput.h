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


#pragma once

#include "../Enumerations.h"
#include "IHttpOutputStream.h"
#include "IHttpStreamAnswer.h"

#include <list>
#include <string>
#include <stdint.h>
#include <map>

namespace Orthanc
{
  class HttpOutput : public boost::noncopyable
  {
  private:
    typedef std::list< std::pair<std::string, std::string> >  Header;

    class StateMachine : public boost::noncopyable
    {
    public:
      enum State
      {
        State_WritingHeader,      
        State_WritingBody,
        State_WritingMultipart,
        State_Done
      };

    private:
      IHttpOutputStream& stream_;
      State state_;

      HttpStatus status_;
      bool hasContentLength_;
      uint64_t contentLength_;
      uint64_t contentPosition_;
      bool keepAlive_;
      std::list<std::string> headers_;

      std::string multipartBoundary_;
      std::string multipartContentType_;

    public:
      StateMachine(IHttpOutputStream& stream,
                   bool isKeepAlive);

      ~StateMachine();

      void SetHttpStatus(HttpStatus status);

      void SetContentLength(uint64_t length);

      void SetContentType(const char* contentType);

      void SetContentFilename(const char* filename);

      void SetCookie(const std::string& cookie,
                     const std::string& value);

      void AddHeader(const std::string& header,
                     const std::string& value);

      void ClearHeaders();

      void SendBody(const void* buffer, size_t length);

      void StartMultipart(const std::string& subType,
                          const std::string& contentType);

      void SendMultipartItem(const void* item, 
                             size_t length,
                             const std::map<std::string, std::string>& headers);

      void CloseMultipart();

      void CloseBody();

      State GetState() const
      {
        return state_;
      }
    };

    StateMachine stateMachine_;
    bool         isDeflateAllowed_;
    bool         isGzipAllowed_;

    HttpCompression GetPreferredCompression(size_t bodySize) const;

  public:
    HttpOutput(IHttpOutputStream& stream,
               bool isKeepAlive) : 
      stateMachine_(stream, isKeepAlive),
      isDeflateAllowed_(false),
      isGzipAllowed_(false)
    {
    }

    void SetDeflateAllowed(bool allowed)
    {
      isDeflateAllowed_ = allowed;
    }

    bool IsDeflateAllowed() const
    {
      return isDeflateAllowed_;
    }

    void SetGzipAllowed(bool allowed)
    {
      isGzipAllowed_ = allowed;
    }

    bool IsGzipAllowed() const
    {
      return isGzipAllowed_;
    }

    void SendStatus(HttpStatus status,
		    const char* message,
		    size_t messageSize);

    void SendStatus(HttpStatus status)
    {
      SendStatus(status, NULL, 0);
    }

    void SendStatus(HttpStatus status,
		    const std::string& message)
    {
      SendStatus(status, message.c_str(), message.size());
    }

    void SetContentType(const char* contentType)
    {
      stateMachine_.SetContentType(contentType);
    }

    void SetContentFilename(const char* filename)
    {
      stateMachine_.SetContentFilename(filename);
    }

    void SetCookie(const std::string& cookie,
                   const std::string& value)
    {
      stateMachine_.SetCookie(cookie, value);
    }

    void AddHeader(const std::string& key,
                   const std::string& value)
    {
      stateMachine_.AddHeader(key, value);
    }

    void Answer(const void* buffer, 
                size_t length);

    void Answer(const std::string& str);

    void AnswerEmpty();

    void SendMethodNotAllowed(const std::string& allowed);

    void Redirect(const std::string& path);

    void SendUnauthorized(const std::string& realm);

    void StartMultipart(const std::string& subType,
                        const std::string& contentType)
    {
      stateMachine_.StartMultipart(subType, contentType);
    }

    void SendMultipartItem(const void* item, 
                           size_t size,
                           const std::map<std::string, std::string>& headers)
    {
      stateMachine_.SendMultipartItem(item, size, headers);
    }

    void CloseMultipart()
    {
      stateMachine_.CloseMultipart();
    }

    bool IsWritingMultipart() const
    {
      return stateMachine_.GetState() == StateMachine::State_WritingMultipart;
    }

    void Answer(IHttpStreamAnswer& stream);
  };
}
