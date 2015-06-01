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

#include "../Enumerations.h"

#include <string>

namespace Orthanc
{
  class ImageAccessor
  {
  private:
    bool readOnly_;
    PixelFormat format_;
    unsigned int width_;
    unsigned int height_;
    unsigned int pitch_;
    void *buffer_;

  public:
    ImageAccessor()
    {
      AssignEmpty(PixelFormat_Grayscale8);
    }

    bool IsReadOnly() const
    {
      return readOnly_;
    }

    PixelFormat GetFormat() const
    {
      return format_;
    }

    unsigned int GetBytesPerPixel() const
    {
      return ::Orthanc::GetBytesPerPixel(format_);
    }

    unsigned int GetWidth() const
    {
      return width_;
    }

    unsigned int GetHeight() const
    {
      return height_;
    }

    unsigned int GetPitch() const
    {
      return pitch_;
    }

    unsigned int GetSize() const
    {
      return GetHeight() * GetPitch();
    }

    const void* GetConstBuffer() const
    {
      return buffer_;
    }

    void* GetBuffer() const;

    const void* GetConstRow(unsigned int y) const;

    void* GetRow(unsigned int y) const;

    void AssignEmpty(PixelFormat format);

    void AssignReadOnly(PixelFormat format,
                        unsigned int width,
                        unsigned int height,
                        unsigned int pitch,
                        const void *buffer);

    void AssignWritable(PixelFormat format,
                        unsigned int width,
                        unsigned int height,
                        unsigned int pitch,
                        void *buffer);

    void ToMatlabString(std::string& target) const; 
  };
}
