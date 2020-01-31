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


#pragma once

#include "StringMatcher.h"
#include "../ChunkedBuffer.h"

#include <map>

namespace Orthanc
{
  class MultipartStreamReader : public boost::noncopyable
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
    MultipartStreamReader(const std::string& boundary);

    void SetBlockSize(size_t size);

    size_t GetBlockSize() const
    {
      return blockSize_;
    }

    void SetHandler(IHandler& handler)
    {
      handler_ = &handler;
    }
    
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
  };
}
