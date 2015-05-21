/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2015 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * In addition, as a special exception, the copyright holders of this
 * program give permission to link the code of its release with the
 * OpenSSL project's "OpenSSL" library (or with modified versions of it
 * that use the same license as the "OpenSSL" library), and distribute
 * the linked executables. You must obey the GNU General Public License
 * in all respects for all of the code used other than "OpenSSL". If you
 * modify file(s) with this exception, you may extend this exception to
 * your version of the file(s), but you are not obligated to do so. If
 * you do not wish to do so, delete this exception statement from your
 * version. If you delete this exception statement from all source files
 * in the program, then also delete it here.
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

#include "ImageAccessor.h"

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

    void WriteToFile(const char* filename,
                     const ImageAccessor& accessor)
    {
      WriteToFile(filename, accessor.GetWidth(), accessor.GetHeight(),
                  accessor.GetPitch(), accessor.GetFormat(), accessor.GetConstBuffer());
    }

    void WriteToMemory(std::string& png,
                       const ImageAccessor& accessor)
    {
      WriteToMemory(png, accessor.GetWidth(), accessor.GetHeight(),
                    accessor.GetPitch(), accessor.GetFormat(), accessor.GetConstBuffer());
    }
  };
}
