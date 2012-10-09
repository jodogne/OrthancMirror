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

#include <boost/filesystem.hpp>
#include <set>

#include "Compression/BufferCompressor.h"

namespace Orthanc
{
  class FileStorage : public boost::noncopyable
  {
    friend class HttpOutput;

  private:
    std::auto_ptr<BufferCompressor> compressor_;

    boost::filesystem::path root_;

    boost::filesystem::path GetPath(const std::string& uuid) const;

    std::string CreateFileWithoutCompression(const void* content, size_t size);

  public:
    FileStorage(std::string root);

    void SetBufferCompressor(BufferCompressor* compressor)  // Takes the ownership
    {
      compressor_.reset(compressor);
    }

    bool HasBufferCompressor() const
    {
      return compressor_.get() != NULL;
    }

    std::string Create(const void* content, size_t size);

    std::string Create(const std::vector<uint8_t>& content);

    std::string Create(const std::string& content);

    void ReadFile(std::string& content,
                  const std::string& uuid) const;

    void ListAllFiles(std::set<std::string>& result) const;

    uintmax_t GetCompressedSize(const std::string& uuid) const;

    void Clear();

    void Remove(const std::string& uuid);

    uintmax_t GetCapacity() const;

    uintmax_t GetAvailableSpace() const;

    std::string GetPath() const
    {
      return root_.string();
    }
  };
}
