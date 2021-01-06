/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
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

    unsigned int GetWidth() const;

    unsigned int GetHeight() const;

    unsigned int GetNumberOfFrames() const;

    unsigned int GetChannelCount() const;

    unsigned int GetBitsStored() const;

    size_t GetBytesPerValue() const;

    bool IsSigned() const;

    unsigned int GetBitsAllocated() const;

    unsigned int GetHighBit() const;

    bool IsPlanar() const;

    unsigned int GetShift() const;

    PhotometricInterpretation GetPhotometricInterpretation() const;

    bool ExtractPixelFormat(PixelFormat& format,
                            bool ignorePhotometricInterpretation) const;

    size_t GetFrameSize() const;

    /**
     * This constant gives a bound on the maximum tag length that is
     * useful to class "DicomImageInformation", in order to avoid
     * using too much memory when copying DICOM tags from "DcmDataset"
     * to "DicomMap" using "ExtractDicomSummary()". It answers the
     * value 256, which corresponds to ORTHANC_MAXIMUM_TAG_LENGTH that
     * was implicitly used in Orthanc <= 1.7.2.
     **/
    static unsigned int GetUsefulTagLength();
  };
}
