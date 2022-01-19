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


#include "NumpyWriter.h"

#if ORTHANC_ENABLE_ZLIB == 1
#  include "../Compression/ZipWriter.h"
#endif

#if ORTHANC_SANDBOXED == 0
#  include "../SystemToolbox.h"
#endif

#include "../OrthancException.h"
#include "../Toolbox.h"

#include <boost/lexical_cast.hpp>

namespace Orthanc
{
  void NumpyWriter::WriteHeader(ChunkedBuffer& target,
                                unsigned int depth,
                                unsigned int width,
                                unsigned int height,
                                PixelFormat format)
  {
    // https://numpy.org/devdocs/reference/generated/numpy.lib.format.html
    static const unsigned char VERSION[] = {
      0x93, 'N', 'U', 'M', 'P', 'Y',
      0x01 /* major version: 1 */,
      0x00 /* minor version: 0 */
    };

    std::string datatype;

    switch (Toolbox::DetectEndianness())
    {
      case Endianness_Little:
        datatype = "<";
        break;
        
      case Endianness_Big:
        datatype = ">";
        break;

      default:
        throw OrthancException(ErrorCode_InternalError);
    }
    
    unsigned int channels;

    switch (format)
    {
      case PixelFormat_Grayscale8:
        datatype += "u1";
        channels = 1;
        break;

      case PixelFormat_Grayscale16:
        datatype += "u2";
        channels = 1;
        break;

      case PixelFormat_SignedGrayscale16:
        datatype += "i2";
        channels = 1;
        break;

      case PixelFormat_RGB24:
        datatype += "u1";
        channels = 3;
        break;

      case PixelFormat_Float32:
        datatype += "f4";
        channels = 1;
        break;

      default:
        throw OrthancException(ErrorCode_NotImplemented);
    }

    std::string depthString;
    if (depth != 0)
    {
      depthString = boost::lexical_cast<std::string>(depth) + ", ";
    }
    
    const std::string info = ("{'descr': '" + datatype + "', 'fortran_order': False, " +
                              "'shape': (" + depthString + boost::lexical_cast<std::string>(height) +
                              "," + boost::lexical_cast<std::string>(width) +
                              "," + boost::lexical_cast<std::string>(channels) + "), }");

    const uint16_t minimumLength = sizeof(VERSION) + sizeof(uint16_t) + info.size() + 1 /* trailing '\n' */;

    // The length of the header must be evenly divisible by 64. This
    // loop could be optimized by a "ceil()" operation, but we keep
    // the code as simple as possible
    uint16_t length = 64;
    while (length < minimumLength)
    {
      length += 64;
    }

    uint16_t countZeros = length - minimumLength;
    uint16_t headerLength = info.size() + countZeros + 1 /* trailing '\n' */;
    uint8_t highByte = headerLength / 256;
    uint8_t lowByte = headerLength % 256;
    
    target.AddChunk(VERSION, sizeof(VERSION));
    target.AddChunk(&lowByte, 1);
    target.AddChunk(&highByte, 1);
    target.AddChunk(info);
    target.AddChunk(std::string(countZeros, ' '));
    target.AddChunk("\n");
  }


  void NumpyWriter::WritePixels(ChunkedBuffer& target,
                                const ImageAccessor& image)
  {
    size_t rowSize = image.GetBytesPerPixel() * image.GetWidth();

    for (unsigned int y = 0; y < image.GetHeight(); y++)
    {
      target.AddChunk(image.GetConstRow(y), rowSize);
    }
  }
  

  void NumpyWriter::Finalize(std::string& target,
                             ChunkedBuffer& source,
                             bool compress)
  {
    if (compress)
    {
#if ORTHANC_ENABLE_ZLIB == 1
      // This is the default name of the first array if arrays are
      // specified as positional arguments in "numpy.savez()"
      // https://numpy.org/doc/stable/reference/generated/numpy.savez.html
      const char* ARRAY_NAME = "arr_0";
      
      std::string uncompressed;
      source.Flatten(uncompressed);

      const bool isZip64 = (uncompressed.size() >= 1lu * 1024lu * 1024lu * 1024lu);

      ZipWriter writer;
      writer.SetMemoryOutput(target, isZip64);
      writer.Open();
      writer.OpenFile(ARRAY_NAME);
      writer.Write(uncompressed);
      writer.Close();
#else
      throw OrthancException(ErrorCode_InternalError, "Orthanc was compiled without support for zlib");
#endif
    }
    else
    {
      source.Flatten(target);
    }
  }


#if ORTHANC_SANDBOXED == 0
  void NumpyWriter::WriteToFileInternal(const std::string& filename,
                                        unsigned int width,
                                        unsigned int height,
                                        unsigned int pitch,
                                        PixelFormat format,
                                        const void* buffer)
  {
    std::string content;
    WriteToMemoryInternal(content, width, height, pitch, format, buffer);
    
    SystemToolbox::WriteFile(content, filename);
  }
#endif

  
  void NumpyWriter::WriteToMemoryInternal(std::string& content,
                                          unsigned int width,
                                          unsigned int height,
                                          unsigned int pitch,
                                          PixelFormat format,
                                          const void* buffer)
  {
    ChunkedBuffer chunks;
    WriteHeader(chunks, 0 /* no depth */, width, height, format);

    ImageAccessor image;
    image.AssignReadOnly(format, width, height, pitch, buffer);
    WritePixels(chunks, image);

    Finalize(content, chunks, compressed_);
  }


  NumpyWriter::NumpyWriter()
  {
    compressed_ = false;
  }

  
  void NumpyWriter::SetCompressed(bool compressed)
  {
#if ORTHANC_ENABLE_ZLIB == 1
    compressed_ = compressed;
#else
    if (compressed)
    {
      throw OrthancException(ErrorCode_InternalError, "Orthanc was compiled without support for zlib");
    }
#endif
  }


  bool NumpyWriter::IsCompressed() const
  {
    return compressed_;
  }
}
