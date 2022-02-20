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

#include "ImageAccessor.h"

#include <vector>
#include <stdint.h>
#include <boost/noncopyable.hpp>

namespace Orthanc
{
  class ORTHANC_PUBLIC ImageBuffer : public boost::noncopyable
  {
  private:
    bool changed_;

    bool forceMinimalPitch_;  // Currently unused
    PixelFormat format_;
    unsigned int width_;
    unsigned int height_;
    unsigned int pitch_;
    void *buffer_;

    void Initialize();
    
    void Allocate();

    void Deallocate();

  public:
    ImageBuffer(PixelFormat format,
                unsigned int width,
                unsigned int height,
                bool forceMinimalPitch);

    ImageBuffer();

    ~ImageBuffer();

    PixelFormat GetFormat() const;

    void SetFormat(PixelFormat format);

    unsigned int GetWidth() const;

    void SetWidth(unsigned int width);

    unsigned int GetHeight() const;

    void SetHeight(unsigned int height);

    unsigned int GetBytesPerPixel() const;

    void GetReadOnlyAccessor(ImageAccessor& accessor);

    void GetWriteableAccessor(ImageAccessor& accessor);

    bool IsMinimalPitchForced() const;

    void AcquireOwnership(ImageBuffer& other);
  };
}
