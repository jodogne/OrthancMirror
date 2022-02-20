/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

namespace Orthanc
{
  class ORTHANC_PUBLIC BufferHttpSender : public HttpFileSender
  {
  private:
    std::string  buffer_;
    size_t       position_;
    size_t       chunkSize_;
    size_t       currentChunkSize_;

  public:
    BufferHttpSender();

    std::string& GetBuffer();

    const std::string& GetBuffer() const;

    // This is for test purpose. If "chunkSize" is set to "0" (the
    // default), the entire buffer is consumed at once.
    void SetChunkSize(size_t chunkSize);


    /**
     * Implementation of the IHttpStreamAnswer interface.
     **/

    virtual uint64_t GetContentLength() ORTHANC_OVERRIDE;

    virtual bool ReadNextChunk() ORTHANC_OVERRIDE;

    virtual const char* GetChunkContent() ORTHANC_OVERRIDE;

    virtual size_t GetChunkSize() ORTHANC_OVERRIDE;
  };
}
