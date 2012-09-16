/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012 Medical Physics Department, CHU of Liege,
 * Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#pragma once

#include "Enumerations.h"

#include <boost/shared_ptr.hpp>
#include <string>

namespace Orthanc
{
  class PngWriter
  {
  private:
    struct PImpl;
    boost::shared_ptr<PImpl> pimpl_;

    void Compress(unsigned int width,
                  unsigned int height,
                  unsigned int pitch,
                  PixelFormat format);

    void Prepare(unsigned int width,
                 unsigned int height,
                 unsigned int pitch,
                 PixelFormat format,
                 const void* buffer);

  public:
    PngWriter();

    ~PngWriter();

    void WriteToFile(const char* filename,
                     unsigned int width,
                     unsigned int height,
                     unsigned int pitch,
                     PixelFormat format,
                     const void* buffer);

    void WriteToMemory(std::string& png,
                       unsigned int width,
                       unsigned int height,
                       unsigned int pitch,
                       PixelFormat format,
                       const void* buffer);
  };
}
