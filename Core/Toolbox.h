/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
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

#include "Enumerations.h"

#include <stdint.h>
#include <vector>
#include <string>
#include <json/json.h>

namespace Orthanc
{
  typedef std::vector<std::string> UriComponents;

  class NullType
  {
  };

  namespace Toolbox
  {
    void USleep(uint64_t microSeconds);

#if !defined(ORTHANC_SANDBOXED) || ORTHANC_SANDBOXED != 1
    ServerBarrierEvent ServerBarrier(const bool& stopFlag);

    ServerBarrierEvent ServerBarrier();
#endif

    void ToUpperCase(std::string& s);  // Inplace version

    void ToLowerCase(std::string& s);  // Inplace version

    void ToUpperCase(std::string& result,
                     const std::string& source);

    void ToLowerCase(std::string& result,
                     const std::string& source);

#if !defined(ORTHANC_SANDBOXED) || ORTHANC_SANDBOXED != 1
    void ReadFile(std::string& content,
                  const std::string& path);
#endif

#if !defined(ORTHANC_SANDBOXED) || ORTHANC_SANDBOXED != 1
    bool ReadHeader(std::string& header,
                    const std::string& path,
                    size_t headerSize);
#endif

#if !defined(ORTHANC_SANDBOXED) || ORTHANC_SANDBOXED != 1
    void WriteFile(const std::string& content,
                   const std::string& path);
#endif

#if !defined(ORTHANC_SANDBOXED) || ORTHANC_SANDBOXED != 1
    void WriteFile(const void* content,
                   size_t size,
                   const std::string& path);
#endif

#if !defined(ORTHANC_SANDBOXED) || ORTHANC_SANDBOXED != 1
    void RemoveFile(const std::string& path);
#endif

    void SplitUriComponents(UriComponents& components,
                            const std::string& uri);
  
    void TruncateUri(UriComponents& target,
                     const UriComponents& source,
                     size_t fromLevel);
  
    bool IsChildUri(const UriComponents& baseUri,
                    const UriComponents& testedUri);

    std::string AutodetectMimeType(const std::string& path);

    std::string FlattenUri(const UriComponents& components,
                           size_t fromLevel = 0);

#if !defined(ORTHANC_SANDBOXED) || ORTHANC_SANDBOXED != 1
    uint64_t GetFileSize(const std::string& path);
#endif

#if !defined(ORTHANC_ENABLE_MD5) || ORTHANC_ENABLE_MD5 == 1
    void ComputeMD5(std::string& result,
                    const std::string& data);

    void ComputeMD5(std::string& result,
                    const void* data,
                    size_t size);
#endif

    void ComputeSHA1(std::string& result,
                     const std::string& data);

    void ComputeSHA1(std::string& result,
                     const void* data,
                     size_t size);

    bool IsSHA1(const char* str,
                size_t size);

    bool IsSHA1(const std::string& s);

#if !defined(ORTHANC_ENABLE_BASE64) || ORTHANC_ENABLE_BASE64 == 1
    void DecodeBase64(std::string& result, 
                      const std::string& data);

    void EncodeBase64(std::string& result, 
                      const std::string& data);

#  if BOOST_HAS_REGEX == 1
    bool DecodeDataUriScheme(std::string& mime,
                             std::string& content,
                             const std::string& source);
#  endif

    void EncodeDataUriScheme(std::string& result,
                             const std::string& mime,
                             const std::string& content);
#endif

#if !defined(ORTHANC_SANDBOXED) || ORTHANC_SANDBOXED != 1
    std::string GetPathToExecutable();

    std::string GetDirectoryOfExecutable();
#endif

    std::string ConvertToUtf8(const std::string& source,
                              Encoding sourceEncoding);

    std::string ConvertFromUtf8(const std::string& source,
                                Encoding targetEncoding);

    bool IsAsciiString(const void* data,
                       size_t size);

    std::string ConvertToAscii(const std::string& source);

    std::string StripSpaces(const std::string& source);

#if BOOST_HAS_DATE_TIME == 1
    std::string GetNowIsoString();

    void GetNowDicom(std::string& date,
                     std::string& time);
#endif

    // In-place percent-decoding for URL
    void UrlDecode(std::string& s);

    Endianness DetectEndianness();

#if BOOST_HAS_REGEX == 1
    std::string WildcardToRegularExpression(const std::string& s);
#endif

    void TokenizeString(std::vector<std::string>& result,
                        const std::string& source,
                        char separator);

#if !defined(ORTHANC_SANDBOXED) || ORTHANC_SANDBOXED != 1
    void MakeDirectory(const std::string& path);
#endif

#if !defined(ORTHANC_SANDBOXED) || ORTHANC_SANDBOXED != 1
    bool IsExistingFile(const std::string& path);
#endif

#if ORTHANC_PUGIXML_ENABLED == 1
    void JsonToXml(std::string& target,
                   const Json::Value& source,
                   const std::string& rootElement = "root",
                   const std::string& arrayElement = "item");
#endif

#if !defined(ORTHANC_SANDBOXED) || ORTHANC_SANDBOXED != 1
    void ExecuteSystemCommand(const std::string& command,
                              const std::vector<std::string>& arguments);
#endif

    bool IsInteger(const std::string& str);

    void CopyJsonWithoutComments(Json::Value& target,
                                 const Json::Value& source);

    bool StartsWith(const std::string& str,
                    const std::string& prefix);

    int GetProcessId();

#if !defined(ORTHANC_SANDBOXED) || ORTHANC_SANDBOXED != 1
    bool IsRegularFile(const std::string& path);
#endif

    FILE* OpenFile(const std::string& path,
                   FileMode mode);

    void UriEncode(std::string& target,
                   const std::string& source);

    std::string GetJsonStringField(const ::Json::Value& json,
                                   const std::string& key,
                                   const std::string& defaultValue);

    bool GetJsonBooleanField(const ::Json::Value& json,
                             const std::string& key,
                             bool defaultValue);

    int GetJsonIntegerField(const ::Json::Value& json,
                            const std::string& key,
                            int defaultValue);

    unsigned int GetJsonUnsignedIntegerField(const ::Json::Value& json,
                                             const std::string& key,
                                             unsigned int defaultValue);
  }
}
