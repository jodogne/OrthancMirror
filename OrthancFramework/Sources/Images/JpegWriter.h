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

#if !defined(ORTHANC_ENABLE_JPEG)
#  error The macro ORTHANC_ENABLE_JPEG must be defined
#endif

#if ORTHANC_ENABLE_JPEG != 1
#  error JPEG support must be enabled to include this file
#endif

#include "IImageWriter.h"
#include "../Compatibility.h"  // For ORTHANC_OVERRIDE

namespace Orthanc
{
  class ORTHANC_PUBLIC JpegWriter : public IImageWriter
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

    virtual void WriteToMemoryInternal(std::string& jpeg,
                                       unsigned int width,
                                       unsigned int height,
                                       unsigned int pitch,
                                       PixelFormat format,
                                       const void* buffer) ORTHANC_OVERRIDE;

  private:
    uint8_t  quality_;

  public:
    JpegWriter();

    void SetQuality(uint8_t quality);

    uint8_t GetQuality() const;
  };
}
