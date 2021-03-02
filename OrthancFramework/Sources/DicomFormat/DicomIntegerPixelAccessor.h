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

#include "DicomImageInformation.h"

#include <stdint.h>

namespace Orthanc
{
  class DicomIntegerPixelAccessor
  {
  private:
    DicomImageInformation information_;

    uint32_t signMask_;
    uint32_t mask_;

    const void* pixelData_;
    size_t size_;
    unsigned int frame_;
    size_t frameOffset_;
    size_t rowOffset_;

  public:
    DicomIntegerPixelAccessor(const DicomMap& values,
                              const void* pixelData,
                              size_t size);

    const DicomImageInformation GetInformation() const
    {
      return information_;
    }

    unsigned int GetCurrentFrame() const
    {
      return frame_;
    }

    void SetCurrentFrame(unsigned int frame);

    void GetExtremeValues(int32_t& min, 
                          int32_t& max) const;

    int32_t GetValue(unsigned int x, unsigned int y, unsigned int channel = 0) const;

    const void* GetPixelData() const
    {
      return pixelData_;
    }

    size_t GetSize() const
    {
      return size_;
    }
  };
}
