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
#include "ImageProcessing.h"

#include "../OrthancException.h"

#include <cassert>
#include <string.h>
#include <limits>
#include <stdint.h>

#include <stdio.h>

namespace Orthanc
{
  template <typename TargetType, typename SourceType>
  static void ConvertInternal(ImageAccessor& target,
                              const ImageAccessor& source)
  {
    const TargetType minValue = std::numeric_limits<TargetType>::min();
    const TargetType maxValue = std::numeric_limits<TargetType>::max();

    for (unsigned int y = 0; y < source.GetHeight(); y++)
    {
      TargetType* t = reinterpret_cast<TargetType*>(target.GetRow(y));
      const SourceType* s = reinterpret_cast<const SourceType*>(source.GetConstRow(y));

      for (unsigned int x = 0; x < source.GetWidth(); x++, t++, s++)
      {
        if (static_cast<int32_t>(*s) < static_cast<int32_t>(minValue))
        {
          *t = minValue;
        }
        else if (static_cast<int32_t>(*s) > static_cast<int32_t>(maxValue))
        {
          *t = maxValue;
        }
        else
        {
          *t = static_cast<TargetType>(*s);
        }
      }
    }
  }



  void ImageProcessing::Copy(ImageAccessor& target,
                             const ImageAccessor& source)
  {
    if (target.GetWidth() != source.GetWidth() ||
        target.GetHeight() != source.GetHeight())
    {
      throw OrthancException(ErrorCode_IncompatibleImageSize);
    }

    if (target.GetFormat() != source.GetFormat())
    {
      throw OrthancException(ErrorCode_IncompatibleImageFormat);
    }

    unsigned int lineSize = GetBytesPerPixel(source.GetFormat()) * source.GetWidth();

    assert(source.GetPitch() >= lineSize && target.GetPitch() >= lineSize);

    for (unsigned int y = 0; y < source.GetHeight(); y++)
    {
      memcpy(target.GetRow(y), source.GetConstRow(y), lineSize);
    }
  }


  void ImageProcessing::Convert(ImageAccessor& target,
                                const ImageAccessor& source)
  {
    if (target.GetWidth() != source.GetWidth() ||
        target.GetHeight() != source.GetHeight())
    {
      throw OrthancException(ErrorCode_IncompatibleImageSize);
    }

    if (source.GetFormat() == target.GetFormat())
    {
      Copy(target, source);
      return;
    }

    if (target.GetFormat() == PixelFormat_Grayscale16 &&
        source.GetFormat() == PixelFormat_Grayscale8)
    {
      ConvertInternal<uint16_t, uint8_t>(target, source);
      return;
    }

    if (target.GetFormat() == PixelFormat_SignedGrayscale16 &&
        source.GetFormat() == PixelFormat_Grayscale8)
    {
      ConvertInternal<int16_t, uint8_t>(target, source);
      return;
    }

    if (target.GetFormat() == PixelFormat_Grayscale8 &&
        source.GetFormat() == PixelFormat_Grayscale16)
    {
      printf("ICI2\n");
      ConvertInternal<uint8_t, uint16_t>(target, source);

      /*for (unsigned int y = 0; y < source.GetHeight(); y++)
      {
        uint8_t* t = reinterpret_cast<uint8_t*>(target.GetRow(y));
        const uint16_t* s = reinterpret_cast<const uint16_t*>(source.GetConstRow(y));

        for (unsigned int x = 0; x < source.GetWidth(); x++, t++, s++)
        {
          if (*s < 0)
          {
            *t = 0;
          }
          else if (*s > 255)
          {
            *t = 255;
          }
          else
          {
            *t = static_cast<uint8_t>(*s);
          }
        }
        }*/
        
      return;
    }

    if (target.GetFormat() == PixelFormat_SignedGrayscale16 &&
        source.GetFormat() == PixelFormat_Grayscale16)
    {
      ConvertInternal<int16_t, uint16_t>(target, source);
      return;
    }

    if (target.GetFormat() == PixelFormat_Grayscale8 &&
        source.GetFormat() == PixelFormat_SignedGrayscale16)
    {
      ConvertInternal<uint8_t, int16_t>(target, source);
      return;
    }

    if (target.GetFormat() == PixelFormat_Grayscale16 &&
        source.GetFormat() == PixelFormat_SignedGrayscale16)
    {
      ConvertInternal<uint16_t, int16_t>(target, source);
      return;
    }

    throw OrthancException(ErrorCode_NotImplemented);
  }


  void ImageProcessing::ShiftRight(ImageAccessor& image,
                                   unsigned int shift)
  {
    if (image.GetWidth() == 0 ||
        image.GetHeight() == 0 ||
        shift == 0)
    {
      // Nothing to do
      return;
    }

    throw OrthancException(ErrorCode_NotImplemented);
  }

}
