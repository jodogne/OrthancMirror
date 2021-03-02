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

#if !defined(ORTHANC_ENABLE_ZLIB)
#  error The macro ORTHANC_ENABLE_ZLIB must be defined
#endif

#if ORTHANC_ENABLE_ZLIB != 1
#  error ZLIB support must be enabled to include this file
#endif


#include <stdint.h>
#include <string>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>


namespace Orthanc
{
  class ORTHANC_PUBLIC ZipReader : public boost::noncopyable
  {
  private:
    class MemoryBuffer;
    
    struct PImpl;
    boost::shared_ptr<PImpl>   pimpl_;

    ZipReader();

    void SeekFirst();

  public:
    ~ZipReader();

    uint64_t GetFilesCount() const;

    bool ReadNextFile(std::string& filename,
                      std::string& content);
    
    static ZipReader* CreateFromMemory(const void* buffer,
                                       size_t size);

    static ZipReader* CreateFromMemory(const std::string& buffer);

#if ORTHANC_SANDBOXED != 1
    static ZipReader* CreateFromFile(const std::string& path);    
#endif

    static bool IsZipMemoryBuffer(const void* buffer,
                                  size_t size);

    static bool IsZipMemoryBuffer(const std::string& content);

#if ORTHANC_SANDBOXED != 1
    static bool IsZipFile(const std::string& path);
#endif
  };
}
