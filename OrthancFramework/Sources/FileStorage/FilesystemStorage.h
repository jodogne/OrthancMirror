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

#include "../OrthancFramework.h"

#if !defined(ORTHANC_SANDBOXED)
#  error The macro ORTHANC_SANDBOXED must be defined
#endif

#if ORTHANC_SANDBOXED == 1
#  error The class FilesystemStorage cannot be used in sandboxed environments
#endif

#include "IStorageArea.h"
#include "../Compatibility.h"  // For ORTHANC_OVERRIDE

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
    
#if ORTHANC_BUILDING_FRAMEWORK_LIBRARY == 1
    // Alias for binary compatibility with Orthanc Framework 1.7.2 => don't use it anymore
    explicit FilesystemStorage(std::string root);
#endif

#if ORTHANC_BUILDING_FRAMEWORK_LIBRARY == 1
    // Binary compatibility with Orthanc Framework <= 1.8.2
    void Read(std::string& content,
              const std::string& uuid,
              FileContentType type);
#endif

  public:
    explicit FilesystemStorage(const std::string& root);

    FilesystemStorage(const std::string& root,
                      bool fsyncOnWrite);

    virtual void Create(const std::string& uuid,
                        const void* content, 
                        size_t size,
                        FileContentType type) ORTHANC_OVERRIDE;

    virtual IMemoryBuffer* Read(const std::string& uuid,
                                FileContentType type) ORTHANC_OVERRIDE;

    virtual IMemoryBuffer* ReadRange(const std::string& uuid,
                                     FileContentType type,
                                     uint64_t start /* inclusive */,
                                     uint64_t end /* exclusive */) ORTHANC_OVERRIDE;

    virtual bool HasReadRange() const ORTHANC_OVERRIDE;

    virtual void Remove(const std::string& uuid,
                        FileContentType type) ORTHANC_OVERRIDE;

    void ListAllFiles(std::set<std::string>& result) const;

    uintmax_t GetSize(const std::string& uuid) const;

    void Clear();

    uintmax_t GetCapacity() const;

    uintmax_t GetAvailableSpace() const;
  };
}
