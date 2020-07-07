/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
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

#include "DicomMap.h"

#include <stdint.h>

namespace Orthanc
{
  class ORTHANC_PUBLIC DicomImageInformation
  {
  private:
    unsigned int width_;
    unsigned int height_;
    unsigned int samplesPerPixel_;
    uint32_t numberOfFrames_;

    bool isPlanar_;
    bool isSigned_;
    size_t bytesPerValue_;

    uint32_t bitsAllocated_;
    uint32_t bitsStored_;
    uint32_t highBit_;

    PhotometricInterpretation  photometric_;

  protected:
    explicit DicomImageInformation()
    {
    }

  public:
    explicit DicomImageInformation(const DicomMap& values);

    DicomImageInformation* Clone() const;

    unsigned int GetWidth() const
    {
      return width_;
    }

    unsigned int GetHeight() const
    {
      return height_;
    }

    unsigned int GetNumberOfFrames() const
    {
      return numberOfFrames_;
    }

    unsigned int GetChannelCount() const
    {
      return samplesPerPixel_;
    }

    unsigned int GetBitsStored() const
    {
      return bitsStored_;
    }

    size_t GetBytesPerValue() const
    {
      return bytesPerValue_;
    }

    bool IsSigned() const
    {
      return isSigned_;
    }

    unsigned int GetBitsAllocated() const
    {
      return bitsAllocated_;
    }

    unsigned int GetHighBit() const
    {
      return highBit_;
    }

    bool IsPlanar() const
    {
      return isPlanar_;
    }

    unsigned int GetShift() const
    {
      return highBit_ + 1 - bitsStored_;
    }

    PhotometricInterpretation GetPhotometricInterpretation() const
    {
      return photometric_;
    }

    bool ExtractPixelFormat(PixelFormat& format,
                            bool ignorePhotometricInterpretation) const;

    size_t GetFrameSize() const;
  };
}
