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


#pragma once

#include "../Enumerations.h"
#include "IHttpOutputStream.h"
#include "IHttpStreamAnswer.h"

#include <list>
#include <string>
#include <stdint.h>
#include <map>
#include <vector>

namespace Orthanc
{
  class ORTHANC_PUBLIC HttpOutput : public boost::noncopyable
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
        State_Done,
        State_WritingStream
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

      void StartStreamInternal(const std::string& contentType);

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

      void CheckHeadersCompatibilityWithMultipart() const;

      void StartStream(const std::string& contentType);

      void SendStreamItem(const void* data,
                          size_t size);

      void CloseStream();
    };

    StateMachine stateMachine_;
    bool         isDeflateAllowed_;
    bool         isGzipAllowed_;

    HttpCompression GetPreferredCompression(size_t bodySize) const;

  public:
    HttpOutput(IHttpOutputStream& stream,
               bool isKeepAlive);

    void SetDeflateAllowed(bool allowed);

    bool IsDeflateAllowed() const;

    void SetGzipAllowed(bool allowed);

    bool IsGzipAllowed() const;

    void SendStatus(HttpStatus status,
		    const char* message,
		    size_t messageSize);

    void SendStatus(HttpStatus status);

    void SendStatus(HttpStatus status,
                    const std::string& message);

    void SetContentType(MimeType contentType);
    
    void SetContentType(const std::string& contentType);

    void SetContentFilename(const char* filename);

    void SetCookie(const std::string& cookie,
                   const std::string& value);

    void AddHeader(const std::string& key,
                   const std::string& value);

    void Answer(const void* buffer, 
                size_t length);

    void Answer(const std::string& str);

    void AnswerEmpty();

    void SendMethodNotAllowed(const std::string& allowed);

    void Redirect(const std::string& path);

    void SendUnauthorized(const std::string& realm);

    void StartMultipart(const std::string& subType,
                        const std::string& contentType);

    void SendMultipartItem(const void* item,
                           size_t size,
                           const std::map<std::string, std::string>& headers);

    void CloseMultipart();

    bool IsWritingMultipart() const;

    void Answer(IHttpStreamAnswer& stream);

    /**
     * This method is a replacement to the combination
     * "StartMultipart()" + "SendMultipartItem()". It generates the
     * same answer, but it gives a chance to compress the body if
     * "Accept-Encoding: gzip" is provided by the client, which is not
     * possible in chunked transfers.
     **/
    void AnswerMultipartWithoutChunkedTransfer(
      const std::string& subType,
      const std::string& contentType,
      const std::vector<const void*>& parts,
      const std::vector<size_t>& sizes,
      const std::vector<const std::map<std::string, std::string>*>& headers);

    /**
     * Contrarily to "Answer()", this method doesn't bufferizes the
     * stream before sending it, which reduces memory but cannot be
     * used to handle compression using "Content-Encoding".
     **/
    void AnswerWithoutBuffering(IHttpStreamAnswer& stream);
  };
}
