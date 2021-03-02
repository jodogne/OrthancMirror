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

#include "OrthancFramework.h"

#if !defined(ORTHANC_SANDBOXED)
#  error The macro ORTHANC_SANDBOXED must be defined
#endif

#if ORTHANC_SANDBOXED == 1
#  error The class TemporaryFile cannot be used in sandboxed environments
#endif

#include <boost/noncopyable.hpp>
#include <stdint.h>
#include <string>

namespace Orthanc
{
  class ORTHANC_PUBLIC TemporaryFile : public boost::noncopyable
  {
  private:
    std::string path_;

  public:
    TemporaryFile();

    TemporaryFile(const std::string& temporaryFolder,
                  const std::string& extension);

    ~TemporaryFile();

    const std::string& GetPath() const;

    void Write(const std::string& content);

    void Read(std::string& content) const;

    void Touch();

    uint64_t GetFileSize() const;

    void ReadRange(std::string& content,
                   uint64_t start,
                   uint64_t end,
                   bool throwIfOverflow) const;
  };
}
