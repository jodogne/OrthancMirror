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
#include "HttpOutput.h"

#include "../Logging.h"
#include "../OrthancException.h"
#include "../SystemToolbox.h"
#include "../Toolbox.h"
#include "../Compression/GzipCompressor.h"
#include "../Compression/ZlibCompressor.h"

#include <iostream>
#include <vector>
#include <stdio.h>
#include <boost/lexical_cast.hpp>


namespace Orthanc
{
  HttpOutput::StateMachine::StateMachine(IHttpOutputStream& stream,
                                         bool isKeepAlive) : 
    stream_(stream),
    state_(State_WritingHeader),
    status_(HttpStatus_200_Ok),
    hasContentLength_(false),
    contentPosition_(0),
    keepAlive_(isKeepAlive)
  {
  }

  HttpOutput::StateMachine::~StateMachine()
  {
    if (state_ != State_Done)
    {
      //asm volatile ("int3;");
      //LOG(ERROR) << "This HTTP answer does not contain any body";
    }

    if (hasContentLength_ && contentPosition_ != contentLength_)
    {
      LOG(ERROR) << "This HTTP answer has not sent the proper number of bytes in its body";
    }
  }


  void HttpOutput::StateMachine::SetHttpStatus(HttpStatus status)
  {
    if (state_ != State_WritingHeader)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    status_ = status;
  }


  void HttpOutput::StateMachine::SetContentLength(uint64_t length)
  {
    if (state_ != State_WritingHeader)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    hasContentLength_ = true;
    contentLength_ = length;
  }

  void HttpOutput::StateMachine::SetContentType(const char* contentType)
  {
    AddHeader("Content-Type", contentType);
  }

  void HttpOutput::StateMachine::SetContentFilename(const char* filename)
  {
    // TODO Escape double quotes
    AddHeader("Content-Disposition", "filename=\"" + std::string(filename) + "\"");
  }

  void HttpOutput::StateMachine::SetCookie(const std::string& cookie,
                                           const std::string& value)
  {
    if (state_ != State_WritingHeader)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    // TODO Escape "=" characters
    AddHeader("Set-Cookie", cookie + "=" + value);
  }


  void HttpOutput::StateMachine::AddHeader(const std::string& header,
                                           const std::string& value)
  {
    if (state_ != State_WritingHeader)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    headers_.push_back(header + ": " + value + "\r\n");
  }

  void HttpOutput::StateMachine::ClearHeaders()
  {
    if (state_ != State_WritingHeader)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    headers_.clear();
  }

  void HttpOutput::StateMachine::SendBody(const void* buffer, size_t length)
  {
    if (state_ == State_Done)
    {
      if (length == 0)
      {
        return;
      }
      else
      {
        LOG(ERROR) << "Because of keep-alive connections, the entire body must be sent at once or Content-Length must be given";
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }
    }

    if (state_ == State_WritingMultipart)
    {
      throw OrthancException(ErrorCode_InternalError);
    }

    if (state_ == State_WritingHeader)
    {
      // Send the HTTP header before writing the body

      stream_.OnHttpStatusReceived(status_);

      std::string s = "HTTP/1.1 " + 
        boost::lexical_cast<std::string>(status_) +
        " " + std::string(EnumerationToString(status_)) +
        "\r\n";

      if (keepAlive_)
      {
        s += "Connection: keep-alive\r\n";
      }

      for (std::list<std::string>::const_iterator
             it = headers_.begin(); it != headers_.end(); ++it)
      {
        s += *it;
      }

      if (status_ != HttpStatus_200_Ok)
      {
        hasContentLength_ = false;
      }

      uint64_t contentLength = (hasContentLength_ ? contentLength_ : length);
      s += "Content-Length: " + boost::lexical_cast<std::string>(contentLength) + "\r\n\r\n";

      stream_.Send(true, s.c_str(), s.size());
      state_ = State_WritingBody;
    }

    if (hasContentLength_ &&
        contentPosition_ + length > contentLength_)
    {
      LOG(ERROR) << "The body size exceeds what was declared with SetContentSize()";
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    if (length > 0)
    {
      stream_.Send(false, buffer, length);
      contentPosition_ += length;
    }

    if (!hasContentLength_ ||
        contentPosition_ == contentLength_)
    {
      state_ = State_Done;
    }
  }


  void HttpOutput::StateMachine::CloseBody()
  {
    switch (state_)
    {
      case State_WritingHeader:
        SetContentLength(0);
        SendBody(NULL, 0);
        break;

      case State_WritingBody:
        if (!hasContentLength_ ||
            contentPosition_ == contentLength_)
        {
          state_ = State_Done;
        }
        else
        {
          LOG(ERROR) << "The body size has not reached what was declared with SetContentSize()";
          throw OrthancException(ErrorCode_BadSequenceOfCalls);
        }

        break;

      case State_WritingMultipart:
        LOG(ERROR) << "Cannot invoke CloseBody() with multipart outputs";
        throw OrthancException(ErrorCode_BadSequenceOfCalls);

      case State_Done:
        return;  // Ignore

      default:
        throw OrthancException(ErrorCode_InternalError);
    }      
  }


  HttpCompression HttpOutput::GetPreferredCompression(size_t bodySize) const
  {
#if 0
    // TODO Do not compress small files?
    if (bodySize < 512)
    {
      return HttpCompression_None;
    }
#endif

    // Prefer "gzip" over "deflate" if the choice is offered

    if (isGzipAllowed_)
    {
      return HttpCompression_Gzip;
    }
    else if (isDeflateAllowed_)
    {
      return HttpCompression_Deflate;
    }
    else
    {
      return HttpCompression_None;
    }
  }


  void HttpOutput::SendMethodNotAllowed(const std::string& allowed)
  {
    stateMachine_.ClearHeaders();
    stateMachine_.SetHttpStatus(HttpStatus_405_MethodNotAllowed);
    stateMachine_.AddHeader("Allow", allowed);
    stateMachine_.SendBody(NULL, 0);
  }


  void HttpOutput::SendStatus(HttpStatus status,
			      const char* message,
			      size_t messageSize)
  {
    if (status == HttpStatus_301_MovedPermanently ||
        status == HttpStatus_401_Unauthorized ||
        status == HttpStatus_405_MethodNotAllowed)
    {
      LOG(ERROR) << "Please use the dedicated methods to this HTTP status code in HttpOutput";
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
    
    stateMachine_.SetHttpStatus(status);
    stateMachine_.SendBody(message, messageSize);
  }


  void HttpOutput::Redirect(const std::string& path)
  {
    stateMachine_.ClearHeaders();
    stateMachine_.SetHttpStatus(HttpStatus_301_MovedPermanently);
    stateMachine_.AddHeader("Location", path);
    stateMachine_.SendBody(NULL, 0);
  }


  void HttpOutput::SendUnauthorized(const std::string& realm)
  {
    stateMachine_.ClearHeaders();
    stateMachine_.SetHttpStatus(HttpStatus_401_Unauthorized);
    stateMachine_.AddHeader("WWW-Authenticate", "Basic realm=\"" + realm + "\"");
    stateMachine_.SendBody(NULL, 0);
  }

  void HttpOutput::Answer(const void* buffer, 
                          size_t length)
  {
    if (length == 0)
    {
      AnswerEmpty();
      return;
    }

    HttpCompression compression = GetPreferredCompression(length);

    if (compression == HttpCompression_None)
    {
      stateMachine_.SetContentLength(length);
      stateMachine_.SendBody(buffer, length);
      return;
    }

    std::string compressed, encoding;

    switch (compression)
    {
      case HttpCompression_Deflate:
      {
        encoding = "deflate";
        ZlibCompressor compressor;
        // Do not prefix the buffer with its uncompressed size, to be compatible with "deflate"
        compressor.SetPrefixWithUncompressedSize(false);  
        compressor.Compress(compressed, buffer, length);
        break;
      }

      case HttpCompression_Gzip:
      {
        encoding = "gzip";
        GzipCompressor compressor;
        compressor.Compress(compressed, buffer, length);
        break;
      }

      default:
        throw OrthancException(ErrorCode_InternalError);
    }

    LOG(TRACE) << "Compressing a HTTP answer using " << encoding;

    // The body is empty, do not use HTTP compression
    if (compressed.size() == 0)
    {
      AnswerEmpty();
    }
    else
    {
      stateMachine_.AddHeader("Content-Encoding", encoding);
      stateMachine_.SetContentLength(compressed.size());
      stateMachine_.SendBody(compressed.c_str(), compressed.size());
    }

    stateMachine_.CloseBody();
  }


  void HttpOutput::Answer(const std::string& str)
  {
    Answer(str.size() == 0 ? NULL : str.c_str(), str.size());
  }


  void HttpOutput::AnswerEmpty()
  {
    stateMachine_.CloseBody();
  }


  void HttpOutput::StateMachine::StartMultipart(const std::string& subType,
                                                const std::string& contentType)
  {
    if (subType != "mixed" &&
        subType != "related")
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    if (keepAlive_)
    {
      LOG(ERROR) << "Multipart answers are not implemented together with keep-alive connections";
      throw OrthancException(ErrorCode_NotImplemented);
    }

    if (state_ != State_WritingHeader)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    if (status_ != HttpStatus_200_Ok)
    {
      SendBody(NULL, 0);
      return;
    }

    stream_.OnHttpStatusReceived(status_);

    std::string header = "HTTP/1.1 200 OK\r\n";

    // Possibly add the cookies
    for (std::list<std::string>::const_iterator
           it = headers_.begin(); it != headers_.end(); ++it)
    {
      if (!Toolbox::StartsWith(*it, "Set-Cookie: "))
      {
        LOG(ERROR) << "The only headers that can be set in multipart answers are Set-Cookie (here: " << *it << " is set)";
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }

      header += *it;
    }

    multipartBoundary_ = SystemToolbox::GenerateUuid();
    multipartContentType_ = contentType;
    header += "Content-Type: multipart/" + subType + "; type=" + contentType + "; boundary=" + multipartBoundary_ + "\r\n\r\n";

    stream_.Send(true, header.c_str(), header.size());
    state_ = State_WritingMultipart;
  }


  void HttpOutput::StateMachine::SendMultipartItem(const void* item, 
                                                   size_t length,
                                                   const std::map<std::string, std::string>& headers)
  {
    if (state_ != State_WritingMultipart)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    std::string header = "--" + multipartBoundary_ + "\r\n";

    bool hasContentType = false;
    bool hasContentLength = false;
    bool hasMimeVersion = false;

    for (std::map<std::string, std::string>::const_iterator
           it = headers.begin(); it != headers.end(); ++it)
    {
      header += it->first + ": " + it->second + "\r\n";

      std::string tmp;
      Toolbox::ToLowerCase(tmp, it->first);

      if (tmp == "content-type")
      {
        hasContentType = true;
      }

      if (tmp == "content-length")
      {
        hasContentLength = true;
      }

      if (tmp == "mime-version")
      {
        hasMimeVersion = true;
      }
    }

    if (!hasContentType)
    {
      header += "Content-Type: " + multipartContentType_ + "\r\n";
    }

    if (!hasContentLength)
    {
      header += "Content-Length: " + boost::lexical_cast<std::string>(length) + "\r\n";
    }

    if (!hasMimeVersion)
    {
      header += "MIME-Version: 1.0\r\n\r\n";
    }

    stream_.Send(false, header.c_str(), header.size());

    if (length > 0)
    {
      stream_.Send(false, item, length);
    }

    stream_.Send(false, "\r\n", 2);    
  }


  void HttpOutput::StateMachine::CloseMultipart()
  {
    if (state_ != State_WritingMultipart)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    // The two lines below might throw an exception, if the client has
    // closed the connection. Such an error is ignored.
    try
    {
      std::string header = "--" + multipartBoundary_ + "--\r\n";
      stream_.Send(false, header.c_str(), header.size());
    }
    catch (OrthancException&)
    {
    }

    state_ = State_Done;
  }


  void HttpOutput::Answer(IHttpStreamAnswer& stream)
  {
    HttpCompression compression = stream.SetupHttpCompression(isGzipAllowed_, isDeflateAllowed_);

    switch (compression)
    {
      case HttpCompression_None:
        break;

      case HttpCompression_Gzip:
        stateMachine_.AddHeader("Content-Encoding", "gzip");
        break;

      case HttpCompression_Deflate:
        stateMachine_.AddHeader("Content-Encoding", "deflate");
        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    stateMachine_.SetContentLength(stream.GetContentLength());

    std::string contentType = stream.GetContentType();
    if (contentType.empty())
    {
      contentType = "application/octet-stream";
    }

    stateMachine_.SetContentType(contentType.c_str());

    std::string filename;
    if (stream.HasContentFilename(filename))
    {
      SetContentFilename(filename.c_str());
    }

    while (stream.ReadNextChunk())
    {
      stateMachine_.SendBody(stream.GetChunkContent(),
                             stream.GetChunkSize());
    }

    stateMachine_.CloseBody();
  }

}
