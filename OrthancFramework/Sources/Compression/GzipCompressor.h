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

#include "DeflateBaseCompressor.h"
#include "../Compatibility.h"  // For ORTHANC_OVERRIDE

namespace Orthanc
{
  class ORTHANC_PUBLIC GzipCompressor : public DeflateBaseCompressor
  {
  private:
    uint64_t GuessUncompressedSize(const void* compressed,
                                   size_t compressedSize);

  public:
    GzipCompressor();

    virtual void Compress(std::string& compressed,
                          const void* uncompressed,
                          size_t uncompressedSize) ORTHANC_OVERRIDE;

    virtual void Uncompress(std::string& uncompressed,
                            const void* compressed,
                            size_t compressedSize) ORTHANC_OVERRIDE;
  };
}
