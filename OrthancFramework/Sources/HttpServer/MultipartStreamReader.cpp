/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
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
#include "MultipartStreamReader.h"

#include "../OrthancException.h"
#include "../Toolbox.h"

#include <boost/algorithm/string/predicate.hpp>

#if defined(_MSC_VER)
#  include <BaseTsd.h>   // Definition of ssize_t
#endif

namespace Orthanc
{
  static void ParseHeaders(MultipartStreamReader::HttpHeaders& headers,
                           StringMatcher::Iterator start,
                           StringMatcher::Iterator end)
  {
    std::string tmp(start, end);

    std::vector<std::string> lines;
    Toolbox::TokenizeString(lines, tmp, '\n');

    headers.clear();

    for (size_t i = 0; i < lines.size(); i++)
    {
      size_t separator = lines[i].find(':');
      if (separator != std::string::npos)
      {
        std::string key = Toolbox::StripSpaces(lines[i].substr(0, separator));
        std::string value = Toolbox::StripSpaces(lines[i].substr(separator + 1));

        Toolbox::ToLowerCase(key);
        headers[key] = value;
      }
    }
  }


  static bool LookupHeaderSizeValue(size_t& target,
                                    const MultipartStreamReader::HttpHeaders& headers,
                                    const std::string& key)
  {
    MultipartStreamReader::HttpHeaders::const_iterator it = headers.find(key);
    if (it == headers.end())
    {
      return false;
    }
    else
    {
      int64_t value;
        
      try
      {
        value = boost::lexical_cast<int64_t>(it->second);
      }
      catch (boost::bad_lexical_cast&)
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange);
      }

      if (value < 0)
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
      else
      {
        target = static_cast<size_t>(value);
        return true;
      }
    }
  }


  void MultipartStreamReader::ParseStream()
  {
    if (handler_ == NULL ||
        state_ == State_Done)
    {
      return;
    }
      
    std::string corpus;
    buffer_.Flatten(corpus);

    StringMatcher::Iterator current = corpus.begin();
    StringMatcher::Iterator corpusEnd = corpus.end();

    if (state_ == State_UnusedArea)
    {
      /**
       * "Before the first boundary is an area that is ignored by
       * MIME-compliant clients. This area is generally used to put
       * a message to users of old non-MIME clients."
       * https://en.wikipedia.org/wiki/MIME#Multipart_messages
       **/

      if (boundaryMatcher_.Apply(current, corpusEnd))
      {
        current = boundaryMatcher_.GetMatchBegin();
        state_ = State_Content;
      }
      else
      {
        // We have not seen the end of the unused area yet
        std::string reminder(current, corpusEnd);
        buffer_.AddChunkDestructive(reminder);
        return;
      }          
    } 
      
    for (;;)
    {
      size_t patternSize = boundaryMatcher_.GetPattern().size();
      size_t remainingSize = std::distance(current, corpusEnd);
      if (remainingSize < patternSize + 2)
      {
        break;  // Not enough data available
      }
        
      std::string boundary(current, current + patternSize + 2);
      if (boundary == boundaryMatcher_.GetPattern() + "--")
      {
        state_ = State_Done;
        return;
      }
        
      if (boundary != boundaryMatcher_.GetPattern() + "\r\n")
      {
        throw OrthancException(ErrorCode_NetworkProtocol,
                               "Garbage between two items in a multipart stream");
      }

      StringMatcher::Iterator start = current + patternSize + 2;
        
      if (!headersMatcher_.Apply(start, corpusEnd))
      {
        break;  // Not enough data available
      }

      HttpHeaders headers;
      ParseHeaders(headers, start, headersMatcher_.GetMatchBegin());

      size_t contentLength = 0;
      if (!LookupHeaderSizeValue(contentLength, headers, "content-length"))
      {
        if (boundaryMatcher_.Apply(headersMatcher_.GetMatchEnd(), corpusEnd))
        {
          size_t d = std::distance(headersMatcher_.GetMatchEnd(), boundaryMatcher_.GetMatchBegin());
          if (d <= 1)
          {
            throw OrthancException(ErrorCode_NetworkProtocol);
          }
          else
          {
            contentLength = d - 2;
          }
        }
        else
        {
          break;  // Not enough data available to have a full part
        }
      }

      // Explicit conversion to avoid warning about signed vs. unsigned comparison
      std::iterator_traits<StringMatcher::Iterator>::difference_type d = contentLength + 2;
      if (d > std::distance(headersMatcher_.GetMatchEnd(), corpusEnd))
      {
        break;  // Not enough data available to have a full part
      }

      const char* p = headersMatcher_.GetPointerEnd() + contentLength;
      if (p[0] != '\r' ||
          p[1] != '\n')
      {
        throw OrthancException(ErrorCode_NetworkProtocol,
                               "No endline at the end of a part");
      }
          
      handler_->HandlePart(headers, headersMatcher_.GetPointerEnd(), contentLength);
      current = headersMatcher_.GetMatchEnd() + contentLength + 2;
    }

    if (current != corpusEnd)
    {
      std::string reminder(current, corpusEnd);
      buffer_.AddChunkDestructive(reminder);
    }
  }


  MultipartStreamReader::MultipartStreamReader(const std::string& boundary) :
    state_(State_UnusedArea),
    handler_(NULL),
    headersMatcher_("\r\n\r\n"),
    boundaryMatcher_("--" + boundary),
    blockSize_(10 * 1024 * 1024)
  {
  }


  void MultipartStreamReader::SetBlockSize(size_t size)
  {
    if (size == 0)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
    else
    {
      blockSize_ = size;
    }        
  }

    
  void MultipartStreamReader::AddChunk(const void* chunk,
                                       size_t size)
  {
    if (state_ != State_Done &&
        size != 0)
    {
      size_t oldSize = buffer_.GetNumBytes();
      
      buffer_.AddChunk(chunk, size);

      if (oldSize / blockSize_ != buffer_.GetNumBytes() / blockSize_)
      {
        ParseStream();
      }
    }
  }


  void MultipartStreamReader::AddChunk(const std::string& chunk)
  {
    if (!chunk.empty())
    {
      AddChunk(chunk.c_str(), chunk.size());
    }
  }


  void MultipartStreamReader::CloseStream()
  {
    if (buffer_.GetNumBytes() != 0)
    {
      ParseStream();
    }
  }


  bool MultipartStreamReader::GetMainContentType(std::string& contentType,
                                                 const HttpHeaders& headers)
  {
    HttpHeaders::const_iterator it = headers.find("content-type");

    if (it == headers.end())
    {
      return false;
    }
    else
    {
      contentType = it->second;
      return true;
    }
  }


  bool MultipartStreamReader::ParseMultipartContentType(std::string& contentType,
                                                        std::string& subType,
                                                        std::string& boundary,
                                                        const std::string& contentTypeHeader)
  {
    std::vector<std::string> tokens;
    Orthanc::Toolbox::TokenizeString(tokens, contentTypeHeader, ';');

    if (tokens.empty())
    {
      return false;
    }

    contentType = Orthanc::Toolbox::StripSpaces(tokens[0]);
    Orthanc::Toolbox::ToLowerCase(contentType);

    if (contentType.empty())
    {
      return false;
    }

    bool valid = false;
    subType.clear();

    for (size_t i = 0; i < tokens.size(); i++)
    {
      std::vector<std::string> items;
      Orthanc::Toolbox::TokenizeString(items, tokens[i], '=');

      if (items.size() == 2)
      {
        if (boost::iequals("boundary", Orthanc::Toolbox::StripSpaces(items[0])))
        {
          boundary = Orthanc::Toolbox::StripSpaces(items[1]);
          valid = !boundary.empty();
        }
        else if (boost::iequals("type", Orthanc::Toolbox::StripSpaces(items[0])))
        {
          subType = Orthanc::Toolbox::StripSpaces(items[1]);
          Orthanc::Toolbox::ToLowerCase(subType);

          // https://bitbucket.org/sjodogne/orthanc/issues/54/decide-what-to-do-wrt-quoting-of-multipart
          // https://tools.ietf.org/html/rfc7231#section-3.1.1.1
          if (subType.size() >= 2 &&
              subType[0] == '"' &&
              subType[subType.size() - 1] == '"')
          {
            subType = subType.substr(1, subType.size() - 2);
          }
        }
      }
    }

    return valid;
  }
}
