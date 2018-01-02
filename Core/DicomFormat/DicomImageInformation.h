/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2018 Osimis S.A., Belgium
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

#include "DicomMap.h"

#include <stdint.h>

namespace Orthanc
{
  class DicomImageInformation
  {
  private:
    unsigned int width_;
    unsigned int height_;
    unsigned int samplesPerPixel_;
    unsigned int numberOfFrames_;

    bool isPlanar_;
    bool isSigned_;
    size_t bytesPerValue_;

    unsigned int bitsAllocated_;
    unsigned int bitsStored_;
    unsigned int highBit_;

    PhotometricInterpretation  photometric_;

  public:
    explicit DicomImageInformation(const DicomMap& values);

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
