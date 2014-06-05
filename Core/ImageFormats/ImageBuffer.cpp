/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2014 Medical Physics Department, CHU of Liege,
 * Belgium
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


#include "../PrecompiledHeaders.h"
#include "ImageBuffer.h"

namespace Orthanc
{
  void ImageBuffer::Allocate()
  {
    if (changed_)
    {
      /*
        if (forceMinimalPitch_)
        {
        TODO: Align pitch and memory buffer to optimal size for SIMD.
        }
      */

      pitch_ = GetBytesPerPixel() * width_;

      data_.resize(pitch_ * height_);
      if (data_.size() > 0)
      {
        buffer_ = &data_[0];
      }
      else
      {
        buffer_ = 0;
      }

      changed_ = false;
    }
  }


  ImageBuffer::ImageBuffer() : changed_(false)
  {
    changed_ = false;
    forceMinimalPitch_ = true;
    format_ = PixelFormat_Grayscale8;
    width_ = 0;
    height_ = 0;
    pitch_ = 0;
    buffer_ = NULL;
  }


  void ImageBuffer::SetFormat(PixelFormat format)
  {
    changed_ = true;
    format_ = format;
  }


  void ImageBuffer::SetWidth(unsigned int width)
  {
    changed_ = true;
    width_ = width;     
  }


  void ImageBuffer::SetHeight(unsigned int height)
  {
    changed_ = true;
    height_ = height;     
  }


  ImageAccessor ImageBuffer::GetAccessor()
  {
    Allocate();

    ImageAccessor accessor;
    accessor.AssignWritable(format_, width_, height_, pitch_, buffer_);
    return accessor;
  }


  ImageAccessor ImageBuffer::GetConstAccessor()
  {
    Allocate();

    ImageAccessor accessor;
    accessor.AssignReadOnly(format_, width_, height_, pitch_, buffer_);
    return accessor;
  }


  void ImageBuffer::SetMinimalPitchForced(bool force)
  {
    changed_ = true;
    forceMinimalPitch_ = force;
  }
}
