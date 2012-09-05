/**
 * Palanthir - A Lightweight, RESTful DICOM Store
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

#include <boost/filesystem.hpp>
#include <set>

#include "Compression/BufferCompressor.h"

namespace Palanthir
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
