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

#include "../Core/DicomFormat/DicomMap.h"

#include <stdint.h>

namespace Orthanc
{
  class DicomIntegerPixelAccessor
  {
  private:
    unsigned int width_;
    unsigned int height_;
    unsigned int samplesPerPixel_;
    unsigned int numberOfFrames_;
    const void* pixelData_;
    size_t size_;

    uint8_t shift_;
    uint32_t signMask_;
    uint32_t mask_;
    size_t bytesPerPixel_;
    unsigned int frame_;

    size_t frameOffset_;
    size_t rowOffset_;

  public:
    DicomIntegerPixelAccessor(const DicomMap& values,
                              const void* pixelData,
                              size_t size);

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

    unsigned int GetCurrentFrame() const
    {
      return frame_;
    }

    void SetCurrentFrame(unsigned int frame);

    void GetExtremeValues(int32_t& min, 
                          int32_t& max) const;

    int32_t GetValue(unsigned int x, unsigned int y) const;
  };
}
