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
#include "PixelTraits.h"

#include <cassert>

namespace Orthanc
{
  template <PixelFormat Format>
  struct ImageTraits
  {
    typedef ::Orthanc::PixelTraits<Format>    PixelTraits;
    typedef typename PixelTraits::PixelType   PixelType;

    static PixelFormat GetPixelFormat()
    {
      return Format;
    }

    static void GetPixel(PixelType& target,
                         const ImageAccessor& image,
                         unsigned int x,
                         unsigned int y)
    {
      assert(x < image.GetWidth() && y < image.GetHeight());
      PixelTraits::Copy(target, image.GetPixelUnchecked<PixelType>(x, y));
    }

    static void SetPixel(ImageAccessor& image,
                         const PixelType& value,
                         unsigned int x,
                         unsigned int y)
    {
      assert(x < image.GetWidth() && y < image.GetHeight());
      PixelTraits::Copy(image.GetPixelUnchecked<PixelType>(x, y), value);
    }

    static float GetFloatPixel(const ImageAccessor& image,
                               unsigned int x,
                               unsigned int y)
    {
      assert(x < image.GetWidth() && y < image.GetHeight());
      return PixelTraits::PixelToFloat(image.GetPixelUnchecked<PixelType>(x, y));
    }

    static void SetFloatPixel(ImageAccessor& image,
                              float value,
                              unsigned int x,
                              unsigned int y)
    {
      assert(x < image.GetWidth() && y < image.GetHeight());
      PixelTraits::FloatToPixel(image.GetPixelUnchecked<PixelType>(x, y), value);
    }
  };
}
