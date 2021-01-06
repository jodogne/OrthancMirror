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


#include "Image.h"

#include "../Compatibility.h"
#include "ImageProcessing.h"

#include <memory>

namespace Orthanc
{
  Image::Image(PixelFormat format,
               unsigned int width,
               unsigned int height,
               bool forceMinimalPitch) :
    image_(format, width, height, forceMinimalPitch)
  {
    ImageAccessor accessor;
    image_.GetWriteableAccessor(accessor);
    
    AssignWritable(format, width, height, accessor.GetPitch(), accessor.GetBuffer());
  }


  Image* Image::Clone(const ImageAccessor& source)
  {
    std::unique_ptr<Image> target(new Image(source.GetFormat(), source.GetWidth(), source.GetHeight(), false));
    ImageProcessing::Copy(*target, source);
    return target.release();
  }
}
