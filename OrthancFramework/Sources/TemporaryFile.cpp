/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2021-2023 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#include "PrecompiledHeaders.h"
#include "TemporaryFile.h"

#include "OrthancException.h"
#include "SystemToolbox.h"
#include "Toolbox.h"

#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>

namespace Orthanc
{
  static std::string CreateTemporaryPath(const char* temporaryDirectory,
                                         const char* extension)
  {
    boost::filesystem::path dir;

    if (temporaryDirectory == NULL)
    {
#if BOOST_HAS_FILESYSTEM_V3 == 1
      dir = boost::filesystem::temp_directory_path();
#elif defined(__linux__)
      dir = "/tmp";
#else
#  error Support your platform here
#endif
    }
    else
    {
      dir = temporaryDirectory;
    }

    // We use UUID to create unique path to temporary files
    const std::string uuid = Toolbox::GenerateUuid();

    // New in Orthanc 1.5.8: Prefix the process ID to the name of the
    // temporary files, in order to locate orphan temporary files that
    // were left by instances of Orthanc that exited in non-clean way
    // https://groups.google.com/d/msg/orthanc-users/MSJX53bw6Lw/d3S3lRRLAwAJ
    std::string filename = "Orthanc-" + boost::lexical_cast<std::string>(SystemToolbox::GetProcessId()) + "-" + uuid;

    if (extension != NULL)
    {
      filename.append(extension);
    }

    dir /= filename;
    return dir.string();
  }


  TemporaryFile::TemporaryFile() : 
    path_(CreateTemporaryPath(NULL, NULL))
  {
  }


  TemporaryFile::TemporaryFile(const std::string& temporaryDirectory,
                               const std::string& extension) :
    path_(CreateTemporaryPath(temporaryDirectory.c_str(), extension.c_str()))
  {
  }


  TemporaryFile::~TemporaryFile()
  {
    boost::filesystem::remove(path_);
  }

  const std::string &TemporaryFile::GetPath() const
  {
    return path_;
  }


  void TemporaryFile::Write(const std::string& content)
  {
    try
    {
      SystemToolbox::WriteFile(content, path_);
    }
    catch (OrthancException& e)
    {
      throw OrthancException(e.GetErrorCode(),
                             "Can't create temporary file \"" + path_ +
                             "\" with " + boost::lexical_cast<std::string>(content.size()) +
                             " bytes: Check you have write access to the "
                             "temporary directory and that it is not full");
    }
  }


  void TemporaryFile::Read(std::string& content) const
  {
    try
    {
      SystemToolbox::ReadFile(content, path_);
    }
    catch (OrthancException& e)
    {
      throw OrthancException(e.GetErrorCode(),
                             "Can't read temporary file \"" + path_ +
                             "\": Another process has corrupted the temporary directory");
    }
  }


  void TemporaryFile::Touch()
  {
    std::string empty;
    Write(empty);
  }


  uint64_t TemporaryFile::GetFileSize() const
  {
    return SystemToolbox::GetFileSize(path_);
  }


  void TemporaryFile::ReadRange(std::string& content,
                                uint64_t start,
                                uint64_t end,
                                bool throwIfOverflow) const
  {
    SystemToolbox::ReadFileRange(content, path_, start, end, throwIfOverflow);
  }
}
