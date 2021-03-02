/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
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

#include "StringMatcher.h"
#include "../ChunkedBuffer.h"

#include <map>

namespace Orthanc
{
  class ORTHANC_PUBLIC MultipartStreamReader : public boost::noncopyable
  {
  public:
    typedef std::map<std::string, std::string>  HttpHeaders;
    
    class IHandler : public boost::noncopyable
    {
    public:
      virtual ~IHandler()
      {
      }
      
      virtual void HandlePart(const HttpHeaders& headers,
                              const void* part,
                              size_t size) = 0;
    };
    
  private:
    enum State
    {
      State_UnusedArea,
      State_Content,
      State_Done
    };
    
    State          state_;
    IHandler*      handler_;
    StringMatcher  headersMatcher_;
    StringMatcher  boundaryMatcher_;
    ChunkedBuffer  buffer_;
    size_t         blockSize_;

    void ParseStream();

  public:
    explicit MultipartStreamReader(const std::string& boundary);

    void SetBlockSize(size_t size);

    size_t GetBlockSize() const;

    void SetHandler(IHandler& handler);
    
    void AddChunk(const void* chunk,
                  size_t size);

    void AddChunk(const std::string& chunk);

    void CloseStream();

    static bool GetMainContentType(std::string& contentType,
                                   const HttpHeaders& headers);

    static bool ParseMultipartContentType(std::string& contentType,
                                          std::string& subType,  // Possibly empty
                                          std::string& boundary,
                                          const std::string& contentTypeHeader);

    static bool ParseHeaderArguments(std::string& main,
                                     std::map<std::string, std::string>& arguments,
                                     const std::string& header);
  };
}
