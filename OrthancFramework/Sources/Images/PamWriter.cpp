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


#include "../PrecompiledHeaders.h"
#include "PamWriter.h"

#include "../Endianness.h"
#include "../OrthancException.h"
#include "../Toolbox.h"

#include <boost/lexical_cast.hpp>


namespace Orthanc
{
  static void GetPixelFormatInfo(const PixelFormat& format,
                                 unsigned int& maxValue,
                                 unsigned int& channelCount,
                                 unsigned int& bytesPerChannel,
                                 std::string& tupleType)
  {
    switch (format)
    {
      case PixelFormat_Grayscale8:
        maxValue = 255;
        channelCount = 1;
        bytesPerChannel = 1;
        tupleType = "GRAYSCALE";
        break;
          
      case PixelFormat_SignedGrayscale16:
      case PixelFormat_Grayscale16:
        maxValue = 65535;
        channelCount = 1;
        bytesPerChannel = 2;
        tupleType = "GRAYSCALE";
        break;

      case PixelFormat_RGB24:
        maxValue = 255;
        channelCount = 3;
        bytesPerChannel = 1;
        tupleType = "RGB";
        break;

      case PixelFormat_RGB48:
        maxValue = 255;
        channelCount = 3;
        bytesPerChannel = 2;
        tupleType = "RGB";
        break;

      default:
        throw OrthancException(ErrorCode_NotImplemented);
    }
  }

      
  void PamWriter::WriteToMemoryInternal(std::string& target,
                                        unsigned int width,
                                        unsigned int height,
                                        unsigned int sourcePitch,
                                        PixelFormat format,
                                        const void* buffer)
  {
    unsigned int maxValue, channelCount, bytesPerChannel;
    std::string tupleType;
    GetPixelFormatInfo(format, maxValue, channelCount, bytesPerChannel, tupleType);

    target = (std::string("P7") +
              std::string("\nWIDTH ")  + boost::lexical_cast<std::string>(width) + 
              std::string("\nHEIGHT ") + boost::lexical_cast<std::string>(height) + 
              std::string("\nDEPTH ")  + boost::lexical_cast<std::string>(channelCount) + 
              std::string("\nMAXVAL ") + boost::lexical_cast<std::string>(maxValue) + 
              std::string("\nTUPLTYPE ") + tupleType + 
              std::string("\nENDHDR\n"));

    if (bytesPerChannel != 1 &&
        bytesPerChannel != 2)
    {
      throw OrthancException(ErrorCode_NotImplemented);
    }

    size_t targetPitch = channelCount * bytesPerChannel * width;
    size_t offset = target.size();

    target.resize(offset + targetPitch * height);

    assert(target.size() != 0);

    if (Toolbox::DetectEndianness() == Endianness_Little &&
        bytesPerChannel == 2)
    {
      // Byte swapping
      for (unsigned int h = 0; h < height; ++h)
      {
        const uint16_t* p = reinterpret_cast<const uint16_t*>
          (reinterpret_cast<const uint8_t*>(buffer) + h * sourcePitch);
        uint16_t* q = reinterpret_cast<uint16_t*>
          (reinterpret_cast<uint8_t*>(&target[offset]) + h * targetPitch);

        for (unsigned int w = 0; w < width * channelCount; ++w)
        {
          /**
           * This is Little-Endian computer, and PAM uses
           * Big-Endian. Need to do a 16-bit swap. We DON'T use
           * "htobe16()", as the latter only works if the "pixel"
           * pointer is 16-bit aligned (which is not the case if
           * "offset" is an odd number), and the trick that was used
           * in Orthanc <= 1.8.0 (i.e. make a "memcpy()" to a local
           * uint16_t variable) doesn't seem work for WebAssembly. We
           * thus use a plain old C implementation. Check out issue
           * #99: https://bugs.orthanc-server.com/show_bug.cgi?id=99
           **/
          const uint8_t* a = reinterpret_cast<const uint8_t*>(p);
          uint8_t* b = reinterpret_cast<uint8_t*>(q);
          b[0] = a[1];
          b[1] = a[0];
          
          p++;
          q++;
        }
      }
    }
    else
    {
      // Either "bytesPerChannel == 1" (and endianness is not
      // relevant), or we run on a big endian architecture (and no
      // byte swapping is necessary, as PAM uses big endian)
      
      for (unsigned int h = 0; h < height; ++h)
      {
        const void* p = reinterpret_cast<const uint8_t*>(buffer) + h * sourcePitch;
        void* q = reinterpret_cast<uint8_t*>(&target[offset]) + h * targetPitch;
        memcpy(q, p, targetPitch);
      }
    }
  }
}
