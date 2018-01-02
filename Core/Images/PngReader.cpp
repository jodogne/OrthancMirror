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
#include "PngReader.h"

#include "../OrthancException.h"
#include "../Toolbox.h"

#if ORTHANC_SANDBOXED == 0
#  include "../SystemToolbox.h"
#endif

#include <png.h>
#include <string.h>  // For memcpy()

namespace Orthanc
{
#if ORTHANC_SANDBOXED == 0
  namespace 
  {
    struct FileRabi
    {
      FILE* fp_;

      FileRabi(const char* filename)
      {
        fp_ = SystemToolbox::OpenFile(filename, FileMode_ReadBinary);
        if (!fp_)
        {
          throw OrthancException(ErrorCode_InexistentFile);
        }
      }

      ~FileRabi()
      {
        if (fp_)
        {
          fclose(fp_);
        }
      }
    };
  }
#endif


  struct PngReader::PngRabi
  {
    png_structp png_;
    png_infop info_;
    png_infop endInfo_;

    void Destruct()
    {
      if (png_)
      {
        png_destroy_read_struct(&png_, &info_, &endInfo_);

        png_ = NULL;
        info_ = NULL;
        endInfo_ = NULL;
      }
    }

    PngRabi()
    {
      png_ = NULL;
      info_ = NULL;
      endInfo_ = NULL;

      png_ = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
      if (!png_)
      {
        throw OrthancException(ErrorCode_NotEnoughMemory);
      }

      info_ = png_create_info_struct(png_);
      if (!info_)
      {
        png_destroy_read_struct(&png_, NULL, NULL);
        throw OrthancException(ErrorCode_NotEnoughMemory);
      }

      endInfo_ = png_create_info_struct(png_);
      if (!info_)
      {
        png_destroy_read_struct(&png_, &info_, NULL);
        throw OrthancException(ErrorCode_NotEnoughMemory);
      }
    }

    ~PngRabi()
    {
      Destruct();
    }

    static void MemoryCallback(png_structp png_ptr, 
                               png_bytep data, 
                               png_size_t size);
  };


  void PngReader::CheckHeader(const void* header)
  {
    int is_png = !png_sig_cmp((png_bytep) header, 0, 8);
    if (!is_png)
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }
  }

  PngReader::PngReader()
  {
  }

  void PngReader::Read(PngRabi& rabi)
  {
    png_set_sig_bytes(rabi.png_, 8);

    png_read_info(rabi.png_, rabi.info_);

    png_uint_32 width, height;
    int bit_depth, color_type, interlace_type;
    int compression_type, filter_method;
    // get size and bit-depth of the PNG-image
    png_get_IHDR(rabi.png_, rabi.info_,
                 &width, &height,
                 &bit_depth, &color_type, &interlace_type,
                 &compression_type, &filter_method);

    PixelFormat format;
    unsigned int pitch;

    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth == 8)
    {
      format = PixelFormat_Grayscale8;
      pitch = width;
    }
    else if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth == 16)
    {
      format = PixelFormat_Grayscale16;
      pitch = 2 * width;

      if (Toolbox::DetectEndianness() == Endianness_Little)
      {
        png_set_swap(rabi.png_);
      }
    }
    else if (color_type == PNG_COLOR_TYPE_RGB && bit_depth == 8)
    {
      format = PixelFormat_RGB24;
      pitch = 3 * width;
    }
    else if (color_type == PNG_COLOR_TYPE_RGBA && bit_depth == 8)
    {
      format = PixelFormat_RGBA32;
      pitch = 4 * width;
    }
    else
    {
      throw OrthancException(ErrorCode_NotImplemented);
    }

    data_.resize(height * pitch);

    if (height == 0 || width == 0)
    {
      // Empty image, we are done
      AssignEmpty(format);
      return;
    }
    
    png_read_update_info(rabi.png_, rabi.info_);

    std::vector<png_bytep> rows(height);
    for (size_t i = 0; i < height; i++)
    {
      rows[i] = &data_[0] + i * pitch;
    }

    png_read_image(rabi.png_, &rows[0]);

    AssignWritable(format, width, height, pitch, &data_[0]);
  }


#if ORTHANC_SANDBOXED == 0
  void PngReader::ReadFromFile(const std::string& filename)
  {
    FileRabi f(filename.c_str());

    char header[8];
    if (fread(header, 1, 8, f.fp_) != 8)
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    CheckHeader(header);

    PngRabi rabi;

    if (setjmp(png_jmpbuf(rabi.png_)))
    {
      rabi.Destruct();
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    png_init_io(rabi.png_, f.fp_);

    Read(rabi);
  }
#endif


  namespace
  {
    struct MemoryBuffer
    {
      const uint8_t* buffer_;
      size_t size_;
      size_t pos_;
      bool ok_;
    };
  }


  void PngReader::PngRabi::MemoryCallback(png_structp png_ptr, 
                                          png_bytep outBytes, 
                                          png_size_t byteCountToRead)
  {
    MemoryBuffer* from = reinterpret_cast<MemoryBuffer*>(png_get_io_ptr(png_ptr));

    if (!from->ok_)
    {
      return;
    }

    if (from->pos_ + byteCountToRead > from->size_)
    {
      from->ok_ = false;
      return;
    }

    memcpy(outBytes, from->buffer_ + from->pos_, byteCountToRead);

    from->pos_ += byteCountToRead;
  }


  void PngReader::ReadFromMemory(const void* buffer,
                                 size_t size)
  {
    if (size < 8)
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    CheckHeader(buffer);

    PngRabi rabi;

    if (setjmp(png_jmpbuf(rabi.png_)))
    {
      rabi.Destruct();
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    MemoryBuffer tmp;
    tmp.buffer_ = reinterpret_cast<const uint8_t*>(buffer) + 8;  // We skip the header
    tmp.size_ = size - 8;
    tmp.pos_ = 0;
    tmp.ok_ = true;

    png_set_read_fn(rabi.png_, &tmp, PngRabi::MemoryCallback);

    Read(rabi);

    if (!tmp.ok_)
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }
  }

  void PngReader::ReadFromMemory(const std::string& buffer)
  {
    if (buffer.size() != 0)
    {
      ReadFromMemory(&buffer[0], buffer.size());
    }
    else
    {
      ReadFromMemory(NULL, 0);
    }
  }
}
