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

#include "BufferHttpSender.h"

#include "../Compatibility.h"

#include <memory>  // For std::unique_ptr

namespace Orthanc
{
  class ORTHANC_PUBLIC HttpStreamTranscoder : public IHttpStreamAnswer
  {
  private:
    IHttpStreamAnswer& source_;
    CompressionType    sourceCompression_;
    uint64_t           bytesToSkip_;
    uint64_t           skipped_;
    uint64_t           currentChunkOffset_;
    bool               ready_;

    std::unique_ptr<BufferHttpSender>  uncompressed_;

    void ReadSource(std::string& buffer);

    HttpCompression SetupZlibCompression(bool deflateAllowed);

  public:
    HttpStreamTranscoder(IHttpStreamAnswer& source,
                         CompressionType compression);

    // This is the first method to be called
    virtual HttpCompression SetupHttpCompression(bool gzipAllowed,
                                                 bool deflateAllowed) ORTHANC_OVERRIDE;

    virtual bool HasContentFilename(std::string& filename) ORTHANC_OVERRIDE;

    virtual std::string GetContentType() ORTHANC_OVERRIDE;

    virtual uint64_t GetContentLength() ORTHANC_OVERRIDE;

    virtual bool ReadNextChunk() ORTHANC_OVERRIDE;

    virtual const char* GetChunkContent() ORTHANC_OVERRIDE;

    virtual size_t GetChunkSize() ORTHANC_OVERRIDE;
  };
}
