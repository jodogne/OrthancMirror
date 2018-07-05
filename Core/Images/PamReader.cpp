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
#include "PamReader.h"

#include "../OrthancException.h"
#include "../Toolbox.h"
#include <istream>
#include <sstream>
#include <fstream>
#include <endian.h>
#if ORTHANC_SANDBOXED == 0
#  include "../SystemToolbox.h"
#endif

#include <string.h>

namespace Orthanc
{
  namespace
  {
    void GetPixelFormat(PixelFormat& format, unsigned int& bytesPerChannel, const unsigned int& maxValue, const unsigned int& channelCount, const char* tupleType)
    {
      if (strcmp(tupleType, "GRAYSCALE") == 0 && channelCount == 1)
      {
        if (maxValue == 255)
        {
          format = PixelFormat_Grayscale8;
          bytesPerChannel = 1;
          return;
        }
        else if (maxValue == 65535)
        {
          format = PixelFormat_Grayscale16;
          bytesPerChannel = 2;
          return;
        }
      }
      else if (strcmp(tupleType, "RGB") == 0 && channelCount == 3)
      {
        if (maxValue == 255)
        {
          format = PixelFormat_RGB24;
          bytesPerChannel = 1;
          return;
        }
        else if (maxValue == 65535)
        {
          format = PixelFormat_RGB48;
          bytesPerChannel = 2;
          return;
        }
      }
      throw OrthancException(ErrorCode_NotImplemented);
    }

    void ReadDelimiter(std::istream& input, const char* expectedDelimiter)
    {
      std::string delimiter;
      input >> delimiter;
      if (delimiter != expectedDelimiter)
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }
    }

    unsigned int ReadKeyValueUint(std::istream& input, const char* expectedKey)
    {
      std::string key;
      unsigned int value;
      input >> key >> value;
      if (key != expectedKey)
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }
      return value;
    }

    std::string ReadKeyValueString(std::istream& input, const char* expectedKey)
    {
      std::string key;
      std::string value;
      input >> key >> value;
      if (key != expectedKey)
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }
      return value;
    }
  }

  void PamReader::ReadFromStream(std::istream& input)
  {
    ReadDelimiter(input, "P7");
    unsigned int width = ReadKeyValueUint(input, "WIDTH");
    unsigned int height = ReadKeyValueUint(input, "HEIGHT");
    unsigned int channelCount = ReadKeyValueUint(input, "DEPTH");
    unsigned int maxValue = ReadKeyValueUint(input, "MAXVAL");
    std::string tupleType = ReadKeyValueString(input, "TUPLTYPE");
    ReadDelimiter(input, "ENDHDR");
    // skip last EOL
    char tmp[16];
    input.getline(tmp, 16);

    unsigned int bytesPerChannel;
    PixelFormat format;
    GetPixelFormat(format, bytesPerChannel, maxValue, channelCount, tupleType.c_str());

    // read the pixels data
    unsigned int sizeInBytes = width * height * channelCount * bytesPerChannel;
    data_.reserve(sizeInBytes);
    input.read(data_.data(), sizeInBytes);

    AssignWritable(format, width, height, width * channelCount * bytesPerChannel, data_.data());

    // swap bytes
    if (Toolbox::DetectEndianness() == Endianness_Little && bytesPerChannel == 2)
    {
      uint16_t* pixel = NULL;
      for (unsigned int h = 0; h < height; ++h)
      {
        pixel = reinterpret_cast<uint16_t*> (data_.data() + h * width * channelCount * bytesPerChannel);
        for (unsigned int w = 0; w < (width * channelCount); ++w, ++pixel)
        {
          *pixel = htobe16(*pixel);
        }
      }
    }

  }

#if ORTHANC_SANDBOXED == 0
  void PamReader::ReadFromFile(const std::string& filename)
  {
    std::ifstream inputStream(filename, std::ofstream::binary);
    ReadFromStream(inputStream);
  }
#endif

  void PamReader::ReadFromMemory(const void* buffer,
                                 size_t size)
  {
    std::istringstream inputStream(std::string(reinterpret_cast<const char*>(buffer), size));
    ReadFromStream(inputStream);
  }

  void PamReader::ReadFromMemory(const std::string& buffer)
  {
    std::istringstream inputStream(buffer);
    ReadFromStream(inputStream);
  }

}
