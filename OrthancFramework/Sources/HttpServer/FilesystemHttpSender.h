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

#include "HttpFileSender.h"
#include "BufferHttpSender.h"
#include "../FileStorage/FilesystemStorage.h"

#include <fstream>

namespace Orthanc
{
  class ORTHANC_PUBLIC FilesystemHttpSender : public HttpFileSender
  {
  private:
    std::ifstream    file_;
    uint64_t         size_;
    std::string      chunk_;
    size_t           chunkSize_;

    void Initialize(const boost::filesystem::path& path);

  public:
    explicit FilesystemHttpSender(const std::string& path);

    explicit FilesystemHttpSender(const boost::filesystem::path& path) ORTHANC_LOCAL;

    FilesystemHttpSender(const std::string& path,
                         MimeType contentType);

    FilesystemHttpSender(const FilesystemStorage& storage,
                         const std::string& uuid);

    /**
     * Implementation of the IHttpStreamAnswer interface.
     **/

    virtual uint64_t GetContentLength() ORTHANC_OVERRIDE;

    virtual bool ReadNextChunk() ORTHANC_OVERRIDE;

    virtual const char* GetChunkContent() ORTHANC_OVERRIDE;

    virtual size_t GetChunkSize() ORTHANC_OVERRIDE;
  };
}
