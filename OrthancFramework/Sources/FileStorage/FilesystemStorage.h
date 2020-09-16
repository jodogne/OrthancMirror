/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
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

#include "../OrthancFramework.h"

#if !defined(ORTHANC_SANDBOXED)
#  error The macro ORTHANC_SANDBOXED must be defined
#endif

#if ORTHANC_SANDBOXED == 1
#  error The class FilesystemStorage cannot be used in sandboxed environments
#endif

#include "IStorageArea.h"

#include <stdint.h>
#include <boost/filesystem.hpp>
#include <set>

namespace Orthanc
{
  class ORTHANC_PUBLIC FilesystemStorage : public IStorageArea
  {
    // TODO REMOVE THIS
    friend class FilesystemHttpSender;
    friend class FileStorageAccessor;

  private:
    boost::filesystem::path root_;
    bool                    fsyncOnWrite_;

    boost::filesystem::path GetPath(const std::string& uuid) const;

    void Setup(const std::string& root);
    
  public:
    explicit FilesystemStorage(const std::string& root) :
      fsyncOnWrite_(false)
    {
      Setup(root);
    }

    FilesystemStorage(const std::string& root,
                      bool fsyncOnWrite) :
      fsyncOnWrite_(fsyncOnWrite)
    {
      Setup(root);
    }

    virtual void Create(const std::string& uuid,
                        const void* content, 
                        size_t size,
                        FileContentType type) ORTHANC_OVERRIDE;

    virtual void Read(std::string& content,
                      const std::string& uuid,
                      FileContentType type) ORTHANC_OVERRIDE;

    virtual void Remove(const std::string& uuid,
                        FileContentType type) ORTHANC_OVERRIDE;

    void ListAllFiles(std::set<std::string>& result) const;

    uintmax_t GetSize(const std::string& uuid) const;

    void Clear();

    uintmax_t GetCapacity() const;

    uintmax_t GetAvailableSpace() const;
  };
}
