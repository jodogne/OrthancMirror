/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2025 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2025 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

#include "OrthancFramework.h"  // Must be before "ORTHANC_SANDBOXED"

#if !defined(ORTHANC_SANDBOXED)
#  error The macro ORTHANC_SANDBOXED must be defined
#endif

#if !defined(ORTHANC_ENABLE_MD5)
#  error The macro ORTHANC_ENABLE_MD5 must be defined
#endif

#if ORTHANC_SANDBOXED == 1
#  error The namespace SystemToolbox cannot be used in sandboxed environments
#endif

#include "Enumerations.h"

#include <map>
#include <vector>
#include <string>
#include <stdint.h>
#include <boost/filesystem.hpp>

namespace Orthanc
{
  class ORTHANC_PUBLIC SystemToolbox
  {
  public:
    static void USleep(uint64_t microSeconds);

    static ServerBarrierEvent ServerBarrier(const bool& stopFlag);

    static ServerBarrierEvent ServerBarrier();

    static void ReadFile(std::string& content,
                         const std::string& path,
                         bool log);
    
    static void ReadFile(std::string &content, 
                         const boost::filesystem::path& path, 
                         bool log);

    static void ReadFile(std::string &content, 
                         const char* path);

    static void ReadFile(std::string& content,
                         const std::string& path);

    static void ReadFile(std::string &content, 
                         const boost::filesystem::path& path);

    static bool ReadHeader(std::string& header,
                           const std::string& path,
                           size_t headerSize);

   static bool ReadHeader(std::string &header, 
                          const boost::filesystem::path& path, 
                          size_t headerSize);

    static void WriteFile(const void* content,
                          size_t size,
                          const std::string& path,
                          bool callFsync);

    static void WriteFile(const void *content, 
                          size_t size, 
                          const boost::filesystem::path &path, 
                          bool callFsync);    // this variant is mandatory to handle non ASCII-only path on Windows

    static void WriteFile(const void* content,
                          size_t size,
                          const std::string& path);

    static void WriteFile(const std::string& content,
                          const std::string& path,
                          bool callFsync);

    static void WriteFile(const std::string& content,
                          const std::string& path);

    static void WriteFile(const std::string &content, 
                          const boost::filesystem::path& path);

    static void WriteFile(const std::string &content, 
                          const boost::filesystem::path& path, 
                          bool callFsync);

    static void RemoveFile(const std::string& path);

    static void RemoveFile(const boost::filesystem::path& path);

    static uint64_t GetFileSize(const std::string& path);

    static uint64_t GetFileSize(const boost::filesystem::path &path);  // this variant is mandatory to handle non ASCII-only path on Windows

#if ORTHANC_ENABLE_MD5 == 1
    static void ComputeStreamMD5(std::string& result,
                                 std::istream& stream);

    static void ComputeFileMD5(std::string& result,
                               const std::string& path);

    static void ComputeFileMD5(std::string &result, 
                               const boost::filesystem::path& path);  // this variant is mandatory to handle non ASCII-only path on Windows

    // returns true if file have the same MD5
    static bool CompareFilesMD5(const std::string& path1,
                                const std::string& path2); 

    static bool CompareFilesMD5(const boost::filesystem::path& path1, 
                                const boost::filesystem::path& path2);  // this variant is mandatory to handle non ASCII-only path on Windows
#endif

    static void MakeDirectory(const std::string& path);

    static void MakeDirectory(const boost::filesystem::path& path);

    static bool IsExistingFile(const std::string& path);

    static std::string GetPathToExecutable();

    static std::string GetDirectoryOfExecutable();

    static void ExecuteSystemCommand(const std::string& command,
                                     const std::vector<std::string>& arguments);

    static int GetProcessId();

    static bool IsRegularFile(const std::string& path);

    static bool IsRegularFile(const boost::filesystem::path& path);  // this variant is mandatory to handle non ASCII-only path on Windows

    static FILE* OpenFile(const std::string& path,
                          FileMode mode);

    static FILE* OpenFile(const boost::filesystem::path& path, 
                          FileMode mode);

    static MimeType AutodetectMimeType(const char* path)  // used only in Unit Tests
    { 
      return AutodetectMimeType(std::string(path));
    }

    static MimeType AutodetectMimeType(const std::string &path);

    static MimeType AutodetectMimeType(const boost::filesystem::path &path);

    static boost::filesystem::path PathFromUtf8(const std::string &utf8);

    static std::string PathToUtf8(const boost::filesystem::path &p);

    static std::string GetNowIsoString(bool utc);

    static void GetNowDicom(std::string& date,
                            std::string& time,
                            bool utc);

    static unsigned int GetHardwareConcurrency();

    static bool IsContentCompressible(MimeType mime);

    static bool IsContentCompressible(const std::string& contentType);

    static void GetEnvironmentVariables(std::map<std::string, std::string>& env);

    static boost::filesystem::path InterpretRelativePath(const boost::filesystem::path& baseDirectory,
                                                         const std::string& relativePath);

    static void ReadFileRange(std::string& content,                              
                              const std::string& path,
                              uint64_t start,  // Inclusive
                              uint64_t end,    // Exclusive
                              bool throwIfOverflow);

    static void ReadFileRange(std::string &content, 
                              const boost::filesystem::path& path,
                              uint64_t start, // Inclusive
                              uint64_t end, // Exclusive
                              bool throwIfOverflow);

    static void GetMacAddresses(std::set<std::string>& target);

#ifdef _WIN32
    static std::wstring Utf8ToWString(const std::string &str);

    static std::string WStringToUtf8(const std::wstring &wstr);

    static std::wstring WStringFromCharPtr(const char *wstr);
#endif

  };
}
