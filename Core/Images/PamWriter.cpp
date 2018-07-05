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


#include "../PrecompiledHeaders.h"
#include "PamWriter.h"

#include <vector>
#include <stdint.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <endian.h>
#include "../OrthancException.h"
#include "../ChunkedBuffer.h"
#include "../Toolbox.h"

#if ORTHANC_SANDBOXED == 0
#  include "../SystemToolbox.h"
#endif


namespace Orthanc
{
  namespace
  {
    void GetPixelFormatInfo(const PixelFormat& format, unsigned int& maxValue, unsigned int& channelCount, unsigned int& bytesPerChannel, const char*& tupleType) {
      maxValue = 255;
      channelCount = 1;
      bytesPerChannel = 1;
      tupleType = NULL;

      switch (format) {
      case PixelFormat_Grayscale8:
        maxValue = 255;
        channelCount = 1;
        bytesPerChannel = 1;
        tupleType = "GRAYSCALE";
        break;
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

    void WriteToStream(std::ostream& output,
                       unsigned int width,
                       unsigned int height,
                       unsigned int pitch,
                       PixelFormat format,
                       const void* buffer)
    {
      unsigned int maxValue = 255;
      unsigned int channelCount = 1;
      unsigned int bytesPerChannel = 1;
      const char* tupleType = "GRAYSCALE";
      GetPixelFormatInfo(format, maxValue, channelCount, bytesPerChannel, tupleType);

      output << "P7" << "\n";
      output << "WIDTH " << width << "\n";
      output << "HEIGHT " << height << "\n";
      output << "DEPTH " << channelCount << "\n";
      output << "MAXVAL " << maxValue << "\n";
      output << "TUPLTYPE " << tupleType << "\n";
      output << "ENDHDR" << "\n";

      if (Toolbox::DetectEndianness() == Endianness_Little && bytesPerChannel == 2)
      {
        uint16_t tmp;
        const uint16_t* pixel = NULL;
        for (unsigned int h = 0; h < height; ++h)
        {
          pixel = reinterpret_cast<const uint16_t*> (reinterpret_cast<const uint8_t*>(buffer) + h * pitch);
          for (unsigned int w = 0; w < (width * channelCount); ++w, ++pixel)
          {
            tmp = htobe16(*pixel);
            output.write(reinterpret_cast<const char*>(&tmp), 2);
          }
        }
      }
      else
      {
        for (unsigned int h = 0; h < height; ++h)
        {
          output.write(reinterpret_cast<const char*>(reinterpret_cast<const uint8_t*>(buffer) + h * pitch), channelCount * bytesPerChannel * width);
        }
      }

    }

  }

#if ORTHANC_SANDBOXED == 0
  void PamWriter::WriteToFileInternal(const std::string& filename,
                                      unsigned int width,
                                      unsigned int height,
                                      unsigned int pitch,
                                      PixelFormat format,
                                      const void* buffer)
  {
    std::ofstream outfile (filename, std::ofstream::binary);

    WriteToStream(outfile, width, height, pitch, format, buffer);
    outfile.close();
  }
#endif


  void PamWriter::WriteToMemoryInternal(std::string& output,
                                        unsigned int width,
                                        unsigned int height,
                                        unsigned int pitch,
                                        PixelFormat format,
                                        const void* buffer)
  {
    std::ostringstream outStream;  // todo: try to write directly in output and avoid copy

    WriteToStream(outStream, width, height, pitch, format, buffer);
    output = outStream.str();
  }
}
