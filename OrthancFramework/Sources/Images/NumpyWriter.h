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

#if !defined(ORTHANC_ENABLE_ZLIB)
#  error The macro ORTHANC_ENABLE_ZLIB must be defined
#endif

#include "IImageWriter.h"
#include "../ChunkedBuffer.h"
#include "../Compatibility.h"  // For ORTHANC_OVERRIDE

namespace Orthanc
{
  class ORTHANC_PUBLIC NumpyWriter : public IImageWriter
  {
  protected:
#if ORTHANC_SANDBOXED == 0
    virtual void WriteToFileInternal(const std::string& filename,
                                     unsigned int width,
                                     unsigned int height,
                                     unsigned int pitch,
                                     PixelFormat format,
                                     const void* buffer) ORTHANC_OVERRIDE;
#endif

    virtual void WriteToMemoryInternal(std::string& content,
                                       unsigned int width,
                                       unsigned int height,
                                       unsigned int pitch,
                                       PixelFormat format,
                                       const void* buffer) ORTHANC_OVERRIDE;

  private:
    bool  compressed_;

  public:
    NumpyWriter();

    void SetCompressed(bool compressed);

    bool IsCompressed() const;

    static void WriteHeader(ChunkedBuffer& target,
                            unsigned int depth,  // Must be "0" for 2D images
                            unsigned int width,
                            unsigned int height,
                            PixelFormat format);

    static void WritePixels(ChunkedBuffer& target,
                            const ImageAccessor& image);

    static void Finalize(std::string& target,
                         ChunkedBuffer& source,
                         bool compress);
  };
}
