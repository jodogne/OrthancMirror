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

#include <stdint.h>
#include <vector>
#include <string>

namespace Orthanc
{
  typedef std::vector<std::string> UriComponents;

  namespace Toolbox
  {
    void ServerBarrier();

    void ToUpperCase(std::string& s);

    void ToLowerCase(std::string& s);

    void ReadFile(std::string& content,
                  const std::string& path);

    void Sleep(uint32_t seconds);

    void USleep(uint64_t microSeconds);

    void RemoveFile(const std::string& path);

    void SplitUriComponents(UriComponents& components,
                            const std::string& uri);
  
    bool IsChildUri(const UriComponents& baseUri,
                    const UriComponents& testedUri);

    std::string AutodetectMimeType(const std::string& path);

    std::string FlattenUri(const UriComponents& components,
                           size_t fromLevel = 0);

    uint64_t GetFileSize(const std::string& path);

    void ComputeMD5(std::string& result,
                    const std::string& data);

    std::string EncodeBase64(const std::string& data);

    std::string GetPathToExecutable();

    std::string GetDirectoryOfExecutable();

    std::string ConvertToUtf8(const std::string& source,
                              const char* fromEncoding);

    std::string ConvertToAscii(const std::string& source);
  }
}
