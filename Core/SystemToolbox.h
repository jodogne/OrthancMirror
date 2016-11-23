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

#if !defined(ORTHANC_SANDBOXED)
#  error The macro ORTHANC_SANDBOXED must be defined
#endif

#if ORTHANC_SANDBOXED == 1
#  error The namespace SystemToolbox cannot be used in sandboxed environments
#endif

#include "Enumerations.h"

#include <vector>
#include <string>
#include <stdint.h>

namespace Orthanc
{
  namespace SystemToolbox
  {
    ServerBarrierEvent ServerBarrier(const bool& stopFlag);

    ServerBarrierEvent ServerBarrier();

    void ReadFile(std::string& content,
                  const std::string& path);

    bool ReadHeader(std::string& header,
                    const std::string& path,
                    size_t headerSize);

    void WriteFile(const void* content,
                   size_t size,
                   const std::string& path);

    void WriteFile(const std::string& content,
                   const std::string& path);

    void RemoveFile(const std::string& path);

    uint64_t GetFileSize(const std::string& path);

    void MakeDirectory(const std::string& path);

    bool IsExistingFile(const std::string& path);

    std::string GetPathToExecutable();

    std::string GetDirectoryOfExecutable();

    void ExecuteSystemCommand(const std::string& command,
                              const std::vector<std::string>& arguments);

    int GetProcessId();

    bool IsRegularFile(const std::string& path);

    FILE* OpenFile(const std::string& path,
                   FileMode mode);

    std::string GenerateUuid();

#if BOOST_HAS_DATE_TIME == 1
    std::string GetNowIsoString();

    void GetNowDicom(std::string& date,
                     std::string& time);
#endif
  }
}
