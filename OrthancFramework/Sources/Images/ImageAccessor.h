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

#include "../Compatibility.h"
#include "../Enumerations.h"

#include <string>
#include <stdint.h>
#include <boost/noncopyable.hpp>

namespace Orthanc
{
  class ORTHANC_PUBLIC ImageAccessor : public boost::noncopyable
  {
  private:
    template <PixelFormat Format>
    friend struct ImageTraits;
    
    bool readOnly_;
    PixelFormat format_;
    unsigned int width_;
    unsigned int height_;
    unsigned int pitch_;
    uint8_t *buffer_;

    template <typename T>
    const T& GetPixelUnchecked(unsigned int x,
                               unsigned int y) const
    {
      const uint8_t* row = reinterpret_cast<const uint8_t*>(buffer_) + y * pitch_;
      return reinterpret_cast<const T*>(row) [x];
    }


    template <typename T>
    T& GetPixelUnchecked(unsigned int x,
                         unsigned int y)
    {
      uint8_t* row = reinterpret_cast<uint8_t*>(buffer_) + y * pitch_;
      return reinterpret_cast<T*>(row) [x];
    }

#if ORTHANC_BUILDING_FRAMEWORK_LIBRARY == 1
    // Alias for binary compatibility with Orthanc Framework 1.7.2 => don't use it anymore
    void* GetBuffer() const;
    void* GetRow(unsigned int y) const;
#endif

  public:
    ImageAccessor();

    virtual ~ImageAccessor();

    bool IsReadOnly() const;

    PixelFormat GetFormat() const;

    unsigned int GetBytesPerPixel() const;

    unsigned int GetWidth() const;

    unsigned int GetHeight() const;

    unsigned int GetPitch() const;

    unsigned int GetSize() const;

    const void* GetConstBuffer() const;

    void* GetBuffer();

    const void* GetConstRow(unsigned int y) const;

    void* GetRow(unsigned int y);

    void AssignEmpty(PixelFormat format);

    void AssignReadOnly(PixelFormat format,
                        unsigned int width,
                        unsigned int height,
                        unsigned int pitch,
                        const void *buffer);

    void GetReadOnlyAccessor(ImageAccessor& target) const;

    void AssignWritable(PixelFormat format,
                        unsigned int width,
                        unsigned int height,
                        unsigned int pitch,
                        void *buffer);

    void GetWriteableAccessor(ImageAccessor& target) const;

    void ToMatlabString(std::string& target) const; 

    void GetRegion(ImageAccessor& accessor,
                   unsigned int x,
                   unsigned int y,
                   unsigned int width,
                   unsigned int height) const;

    void SetFormat(PixelFormat format);
  };
}
