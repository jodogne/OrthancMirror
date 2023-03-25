/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2021-2023 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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
#include "JpegWriter.h"

#include "../OrthancException.h"
#include "../Logging.h"
#include "JpegErrorManager.h"

#if ORTHANC_SANDBOXED == 0
#  include "../SystemToolbox.h"
#endif

#include <stdlib.h>
#include <vector>

namespace Orthanc
{
  static void GetLines(std::vector<uint8_t*>& lines,
                       unsigned int height,
                       unsigned int pitch,
                       PixelFormat format,
                       const void* buffer)
  {
    if (format != PixelFormat_Grayscale8 &&
        format != PixelFormat_RGB24)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    lines.resize(height);

    uint8_t* base = const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(buffer));
    for (unsigned int y = 0; y < height; y++)
    {
      lines[y] = base + static_cast<intptr_t>(y) * static_cast<intptr_t>(pitch);
    }
  }


  static void Compress(struct jpeg_compress_struct& cinfo,
                       std::vector<uint8_t*>& lines,
                       unsigned int width,
                       unsigned int height,
                       PixelFormat format,
                       uint8_t quality)
  {
    cinfo.image_width = width;
    cinfo.image_height = height;

    switch (format)
    {
      case PixelFormat_Grayscale8:
        cinfo.input_components = 1;
        cinfo.in_color_space = JCS_GRAYSCALE;
        break;

      case PixelFormat_RGB24:
        cinfo.input_components = 3;
        cinfo.in_color_space = JCS_RGB;
        break;

      default:
        throw OrthancException(ErrorCode_InternalError);
    }

    jpeg_set_defaults(&cinfo);

    // The "static_cast" is necessary on OS X:
    // https://github.com/simonfuhrmann/mve/issues/371
    jpeg_set_quality(&cinfo, quality, static_cast<boolean>(true));
    jpeg_start_compress(&cinfo, static_cast<boolean>(true));
    
    jpeg_write_scanlines(&cinfo, &lines[0], height);
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
  }
                       

  JpegWriter::JpegWriter() : quality_(90)
  {
  }


  void JpegWriter::SetQuality(uint8_t quality)
  {
    if (quality == 0 || quality > 100)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    quality_ = quality;
  }


#if ORTHANC_SANDBOXED == 0
  void JpegWriter::WriteToFileInternal(const std::string& filename,
                                       unsigned int width,
                                       unsigned int height,
                                       unsigned int pitch,
                                       PixelFormat format,
                                       const void* buffer)
  {
    FILE* fp = SystemToolbox::OpenFile(filename, FileMode_WriteBinary);
    if (fp == NULL)
    {
      throw OrthancException(ErrorCode_CannotWriteFile);
    }

    std::vector<uint8_t*> lines;
    GetLines(lines, height, pitch, format, buffer);

    struct jpeg_compress_struct cinfo;
    memset(&cinfo, 0, sizeof(struct jpeg_compress_struct));

    Internals::JpegErrorManager jerr;
    cinfo.err = jerr.GetPublic();

    if (setjmp(jerr.GetJumpBuffer())) 
    {
      /* If we get here, the JPEG code has signaled an error.
       * We need to clean up the JPEG object, close the input file, and return.
       */
      jpeg_destroy_compress(&cinfo);
      fclose(fp);
      throw OrthancException(ErrorCode_InternalError,
                             "Error during JPEG encoding: " + jerr.GetMessage());
    }

    // Do not allocate data on the stack below this line!

    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, fp);
    Compress(cinfo, lines, width, height, format, quality_);

    // Everything went fine, "setjmp()" didn't get called

    fclose(fp);
  }
#endif


  void JpegWriter::WriteToMemoryInternal(std::string& jpeg,
                                         unsigned int width,
                                         unsigned int height,
                                         unsigned int pitch,
                                         PixelFormat format,
                                         const void* buffer)
  {
    std::vector<uint8_t*> lines;
    GetLines(lines, height, pitch, format, buffer);

    struct jpeg_compress_struct cinfo;
    memset(&cinfo, 0, sizeof(struct jpeg_compress_struct));

    Internals::JpegErrorManager jerr;

    unsigned char* data = NULL;
    unsigned long size;

    if (setjmp(jerr.GetJumpBuffer())) 
    {
      jpeg_destroy_compress(&cinfo);

      if (data != NULL)
      {
        free(data);
      }

      throw OrthancException(ErrorCode_InternalError,
                             "Error during JPEG encoding: " + jerr.GetMessage());
    }

    // Do not allocate data on the stack below this line!

    jpeg_create_compress(&cinfo);
    cinfo.err = jerr.GetPublic();
    jpeg_mem_dest(&cinfo, &data, &size);

    Compress(cinfo, lines, width, height, format, quality_);

    // Everything went fine, "setjmp()" didn't get called

    jpeg.assign(reinterpret_cast<const char*>(data), size);
    free(data);
  }

  uint8_t JpegWriter::GetQuality() const
  {
    return quality_;
  }
}
