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


#include "../PrecompiledHeaders.h"
#include "MultipartStreamReader.h"

#include "../Logging.h"
#include "../OrthancException.h"
#include "../Toolbox.h"

#include <boost/algorithm/string/predicate.hpp>
#include <boost/lexical_cast.hpp>

#if defined(_MSC_VER)
#  include <BaseTsd.h>   // Definition of ssize_t
#endif

namespace Orthanc
{
  static void ParseHeaders(MultipartStreamReader::HttpHeaders& headers,
                           const char* start,
                           const char* end /* exclusive */)
  {
    assert(start <= end);
    std::string tmp(start, end - start);

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


  void MultipartStreamReader::ParseBlock(const void* data,
                                         size_t size)
  {
    if (handler_ == NULL ||
        state_ == State_Done ||
        size == 0)
    {
      return;
    }
    else
    {
      const char* current = reinterpret_cast<const char*>(data);
      const char* corpusEnd = current + size;

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
          assert(current <= corpusEnd);
          buffer_.AddChunk(current, corpusEnd - current);
          return;
        }          
      } 
      
      for (;;)
      {
        assert(current <= corpusEnd);
      
        size_t patternSize = boundaryMatcher_.GetPattern().size();
        size_t remainingSize = corpusEnd - current;
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

        const char* start = current + patternSize + 2;
        
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
            assert(headersMatcher_.GetMatchEnd() <= boundaryMatcher_.GetMatchBegin());
            size_t d = boundaryMatcher_.GetMatchBegin() - headersMatcher_.GetMatchEnd();
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

        // "static_cast<>" to avoid warning about signed vs. unsigned comparison
        assert(headersMatcher_.GetMatchEnd() <= corpusEnd);
        if (contentLength + 2 > static_cast<size_t>(corpusEnd - headersMatcher_.GetMatchEnd()))
        {
          break;  // Not enough data available to have a full part
        }

        const char* p = headersMatcher_.GetMatchEnd() + contentLength;
        if (p[0] != '\r' ||
            p[1] != '\n')
        {
          throw OrthancException(ErrorCode_NetworkProtocol,
                                 "No endline at the end of a part");
        }
          
        handler_->HandlePart(headers, headersMatcher_.GetMatchEnd(), contentLength);
        current = headersMatcher_.GetMatchEnd() + contentLength + 2;
      }

      if (current != corpusEnd)
      {
        assert(current < corpusEnd);
        buffer_.AddChunk(current, corpusEnd - current);
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
    else
    {
      std::string corpus;
      buffer_.Flatten(corpus);

      if (!corpus.empty())
      {
        ParseBlock(corpus.c_str(), corpus.size());
      }
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

  size_t MultipartStreamReader::GetBlockSize() const
  {
    return blockSize_;
  }

  void MultipartStreamReader::SetHandler(MultipartStreamReader::IHandler &handler)
  {
    handler_ = &handler;
  }


  void MultipartStreamReader::AddChunk(const void* chunk,
                                       size_t size)
  {
    if (state_ != State_Done &&
        size != 0)
    {
      size_t oldSize = buffer_.GetNumBytes();
      if (oldSize == 0)
      {
        /**
         * Optimization in Orthanc 1.9.3: Directly parse the input
         * buffer instead of going through the ChunkedBuffer if the
         * latter is still empty. This notably avoids one memcpy() in
         * STOW-RS server if chunked transfers is disabled.
         **/
        ParseBlock(chunk, size);
      }
      else
      {
        buffer_.AddChunk(chunk, size);

        if (oldSize / blockSize_ != buffer_.GetNumBytes() / blockSize_)
        {
          ParseStream();
        }
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


  static void RemoveSurroundingQuotes(std::string& value)
  {
    if (value.size() >= 2 &&
        value[0] == '"' &&
        value[value.size() - 1] == '"')
    {
      value = value.substr(1, value.size() - 2);
    }
  }
  

  bool MultipartStreamReader::ParseMultipartContentType(std::string& contentType,
                                                        std::string& subType,
                                                        std::string& boundary,
                                                        const std::string& contentTypeHeader)
  {
    std::vector<std::string> tokens;
    Toolbox::TokenizeString(tokens, contentTypeHeader, ';');

    if (tokens.empty())
    {
      return false;
    }

    contentType = Toolbox::StripSpaces(tokens[0]);
    Toolbox::ToLowerCase(contentType);

    if (contentType.empty())
    {
      return false;
    }

    bool valid = false;
    subType.clear();

    for (size_t i = 1; i < tokens.size(); i++)
    {
      std::vector<std::string> items;
      Toolbox::TokenizeString(items, tokens[i], '=');

      if (items.size() == 2)
      {
        if (boost::iequals("boundary", Toolbox::StripSpaces(items[0])))
        {
          boundary = Toolbox::StripSpaces(items[1]);

          // https://bugs.orthanc-server.com/show_bug.cgi?id=190
          RemoveSurroundingQuotes(boundary);
          
          valid = !boundary.empty();
        }
        else if (boost::iequals("type", Toolbox::StripSpaces(items[0])))
        {
          subType = Toolbox::StripSpaces(items[1]);
          Toolbox::ToLowerCase(subType);

          // https://bugs.orthanc-server.com/show_bug.cgi?id=54
          // https://tools.ietf.org/html/rfc7231#section-3.1.1.1
          RemoveSurroundingQuotes(subType);
        }
      }
    }

    return valid;
  }

  
  bool MultipartStreamReader::ParseHeaderArguments(std::string& main,
                                                   std::map<std::string, std::string>& arguments,
                                                   const std::string& header)
  {
    std::vector<std::string> tokens;
    Toolbox::TokenizeString(tokens, header, ';');

    if (tokens.empty())
    {
      return false;
    }

    main = Toolbox::StripSpaces(tokens[0]);
    Toolbox::ToLowerCase(main);
    if (main.empty())
    {
      return false;
    }

    arguments.clear();
    
    for (size_t i = 1; i < tokens.size(); i++)
    {
      std::vector<std::string> items;
      Toolbox::TokenizeString(items, tokens[i], '=');

      if (items.size() > 2)
      {
        return false;
      }
      else if (!items.empty())
      {
        std::string key = Toolbox::StripSpaces(items[0]);
        Toolbox::ToLowerCase(key);
        
        if (arguments.find(key) != arguments.end())
        {
          LOG(ERROR) << "The same argument was provided twice in an HTTP header: \""
                     << key << "\" in \"" << header << "\"";
          return false;
        }
        else if (!key.empty())
        {
          if (items.size() == 1)
          {
            arguments[key] = "";
          }
          else
          {
            assert(items.size() == 2);
            std::string value = Toolbox::StripSpaces(items[1]);
            RemoveSurroundingQuotes(value);
            arguments[key] = value;
          }
        }
      }
    }

    return true;
  }
}
