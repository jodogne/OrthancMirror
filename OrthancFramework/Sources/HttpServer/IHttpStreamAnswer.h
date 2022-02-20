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

#include "../Enumerations.h"

#include <stdint.h>
#include <boost/noncopyable.hpp>
#include <string>

namespace Orthanc
{
  class IHttpStreamAnswer : public boost::noncopyable
  {
  public:
    virtual ~IHttpStreamAnswer()
    {
    }

    // This is the first method to be called
    virtual HttpCompression SetupHttpCompression(bool gzipAllowed,
                                                 bool deflateAllowed) = 0;

    virtual bool HasContentFilename(std::string& filename) = 0;

    virtual std::string GetContentType() = 0;

    virtual uint64_t GetContentLength() = 0;

    virtual bool ReadNextChunk() = 0;

    virtual const char* GetChunkContent() = 0;

    virtual size_t GetChunkSize() = 0;
  };
}
