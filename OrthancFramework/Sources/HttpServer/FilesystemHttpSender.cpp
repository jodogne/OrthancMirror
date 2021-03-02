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

#include "../PrecompiledHeaders.h"
#include "FilesystemHttpSender.h"

#include "../OrthancException.h"

static const size_t  CHUNK_SIZE = 64 * 1024;   // Use 64KB chunks

namespace Orthanc
{
  void FilesystemHttpSender::Initialize(const boost::filesystem::path& path)
  {
    SetContentFilename(path.filename().string());
    file_.open(path.string().c_str(), std::ifstream::binary);

    if (!file_.is_open())
    {
      throw OrthancException(ErrorCode_InexistentFile);
    }

    file_.seekg(0, file_.end);
    size_ = file_.tellg();
    file_.seekg(0, file_.beg);
  }

  FilesystemHttpSender::FilesystemHttpSender(const std::string& path)
  {
    Initialize(path);
  }

  FilesystemHttpSender::FilesystemHttpSender(const boost::filesystem::path& path)
  {
    Initialize(path);
  }

  FilesystemHttpSender::FilesystemHttpSender(const std::string& path,
                                             MimeType contentType)
  {
    SetContentType(contentType);
    Initialize(path);
  }

  FilesystemHttpSender::FilesystemHttpSender(const FilesystemStorage& storage,
                                             const std::string& uuid)
  {
    Initialize(storage.GetPath(uuid));
  }

  uint64_t FilesystemHttpSender::GetContentLength()
  {
    return size_;
  }


  bool FilesystemHttpSender::ReadNextChunk()
  {
    if (chunk_.size() == 0)
    {
      chunk_.resize(CHUNK_SIZE);
    }

    file_.read(&chunk_[0], chunk_.size());

    if ((file_.flags() & std::istream::failbit) ||
        file_.gcount() < 0)
    {
      throw OrthancException(ErrorCode_CorruptedFile);
    }

    chunkSize_ = static_cast<size_t>(file_.gcount());

    return chunkSize_ > 0;
  }

  const char *FilesystemHttpSender::GetChunkContent()
  {
    return chunk_.c_str();
  }

  size_t FilesystemHttpSender::GetChunkSize()
  {
    return chunkSize_;
  }
}
