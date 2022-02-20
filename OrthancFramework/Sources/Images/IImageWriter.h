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

#include <boost/noncopyable.hpp>

#if !defined(ORTHANC_SANDBOXED)
#  error The macro ORTHANC_SANDBOXED must be defined
#endif

namespace Orthanc
{
  class ORTHANC_PUBLIC IImageWriter : public boost::noncopyable
  {
  protected:
    virtual void WriteToMemoryInternal(std::string& compressed,
                                       unsigned int width,
                                       unsigned int height,
                                       unsigned int pitch,
                                       PixelFormat format,
                                       const void* buffer) = 0;

#if ORTHANC_SANDBOXED == 0
    virtual void WriteToFileInternal(const std::string& path,
                                     unsigned int width,
                                     unsigned int height,
                                     unsigned int pitch,
                                     PixelFormat format,
                                     const void* buffer);
#endif

  public:
    virtual ~IImageWriter()
    {
    }

    static void WriteToMemory(IImageWriter& writer,
                              std::string& compressed,
                              const ImageAccessor& accessor);

#if ORTHANC_SANDBOXED == 0
    static void WriteToFile(IImageWriter& writer,
                            const std::string& path,
                            const ImageAccessor& accessor);
#endif
  };
}
