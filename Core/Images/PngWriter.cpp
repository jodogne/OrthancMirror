/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2015 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
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
#include "PngWriter.h"

#include <vector>
#include <stdint.h>
#include <png.h>
#include "../OrthancException.h"
#include "../ChunkedBuffer.h"
#include "../Toolbox.h"


// http://www.libpng.org/pub/png/libpng-1.2.5-manual.html#section-4
// http://zarb.org/~gc/html/libpng.html
/*
  void write_row_callback(png_ptr, png_uint_32 row, int pass)
  {
  }*/




/*  bool isError_;

// http://www.libpng.org/pub/png/book/chapter14.html#png.ch14.div.2

static void ErrorHandler(png_structp png, png_const_charp message)
{
printf("** [%s]\n", message);

PngWriter* that = (PngWriter*) png_get_error_ptr(png);
that->isError_ = true;
printf("** %d\n", (int)that);

//((PngWriter*) payload)->isError_ = true;
}

static void WarningHandler(png_structp png, png_const_charp message)
{
  printf("++ %d\n", (int)message);
}*/


namespace Orthanc
{
  struct PngWriter::PImpl
  {
    png_structp png_;
    png_infop info_;

    // Filled by Prepare()
    std::vector<uint8_t*> rows_;
    int bitDepth_;
    int colorType_;
  };



  PngWriter::PngWriter() : pimpl_(new PImpl)
  {
    pimpl_->png_ = NULL;
    pimpl_->info_ = NULL;

    pimpl_->png_ = png_create_write_struct
      (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL); //this, ErrorHandler, WarningHandler);
    if (!pimpl_->png_)
    {
      throw OrthancException(ErrorCode_NotEnoughMemory);
    }

    pimpl_->info_ = png_create_info_struct(pimpl_->png_);
    if (!pimpl_->info_)
    {
      png_destroy_write_struct(&pimpl_->png_, NULL);
      throw OrthancException(ErrorCode_NotEnoughMemory);
    }
  }

  PngWriter::~PngWriter()
  {
    if (pimpl_->info_)
    {
      png_destroy_info_struct(pimpl_->png_, &pimpl_->info_);
    }

    if (pimpl_->png_)
    {
      png_destroy_write_struct(&pimpl_->png_, NULL);
    }
  }



  void PngWriter::Prepare(unsigned int width,
                          unsigned int height,
                          unsigned int pitch,
                          PixelFormat format,
                          const void* buffer)
  {
    pimpl_->rows_.resize(height);
    for (unsigned int y = 0; y < height; y++)
    {
      pimpl_->rows_[y] = const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(buffer)) + y * pitch;
    }

    switch (format)
    {
    case PixelFormat_RGB24:
      pimpl_->bitDepth_ = 8;
      pimpl_->colorType_ = PNG_COLOR_TYPE_RGB;
      break;

    case PixelFormat_RGBA32:
      pimpl_->bitDepth_ = 8;
      pimpl_->colorType_ = PNG_COLOR_TYPE_RGBA;
      break;

    case PixelFormat_Grayscale8:
      pimpl_->bitDepth_ = 8;
      pimpl_->colorType_ = PNG_COLOR_TYPE_GRAY;
      break;

    case PixelFormat_Grayscale16:
    case PixelFormat_SignedGrayscale16:
      pimpl_->bitDepth_ = 16;
      pimpl_->colorType_ = PNG_COLOR_TYPE_GRAY;
      break;

    default:
      throw OrthancException(ErrorCode_NotImplemented);
    }
  }


  void PngWriter::Compress(unsigned int width,
                           unsigned int height,
                           unsigned int pitch,
                           PixelFormat format)
  {
    png_set_IHDR(pimpl_->png_, pimpl_->info_, width, height,
                 pimpl_->bitDepth_, pimpl_->colorType_, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    png_write_info(pimpl_->png_, pimpl_->info_);

    if (height > 0)
    {
      switch (format)
      {
      case PixelFormat_Grayscale16:
      case PixelFormat_SignedGrayscale16:
      {
        int transforms = 0;
        if (Toolbox::DetectEndianness() == Endianness_Little)
        {
          transforms = PNG_TRANSFORM_SWAP_ENDIAN;
        }

        png_set_rows(pimpl_->png_, pimpl_->info_, &pimpl_->rows_[0]);
        png_write_png(pimpl_->png_, pimpl_->info_, transforms, NULL);

        break;
      }

      default:
        png_write_image(pimpl_->png_, &pimpl_->rows_[0]);
      }
    }

    png_write_end(pimpl_->png_, NULL);
  }


  void PngWriter::WriteToFile(const char* filename,
                              unsigned int width,
                              unsigned int height,
                              unsigned int pitch,
                              PixelFormat format,
                              const void* buffer)
  {
    Prepare(width, height, pitch, format, buffer);

    FILE* fp = fopen(filename, "wb");
    if (!fp)
    {
      throw OrthancException(ErrorCode_CannotWriteFile);
    }    

    png_init_io(pimpl_->png_, fp);

    if (setjmp(png_jmpbuf(pimpl_->png_)))
    {
      // Error during writing PNG
      throw OrthancException(ErrorCode_CannotWriteFile);      
    }

    Compress(width, height, pitch, format);

    fclose(fp);
  }




  static void MemoryCallback(png_structp png_ptr, 
                             png_bytep data, 
                             png_size_t size)
  {
    ChunkedBuffer* buffer = reinterpret_cast<ChunkedBuffer*>(png_get_io_ptr(png_ptr));
    buffer->AddChunk(reinterpret_cast<const char*>(data), size);
  }



  void PngWriter::WriteToMemory(std::string& png,
                                unsigned int width,
                                unsigned int height,
                                unsigned int pitch,
                                PixelFormat format,
                                const void* buffer)
  {
    ChunkedBuffer chunks;

    Prepare(width, height, pitch, format, buffer);

    if (setjmp(png_jmpbuf(pimpl_->png_)))
    {
      // Error during writing PNG
      throw OrthancException(ErrorCode_InternalError);      
    }

    png_set_write_fn(pimpl_->png_, &chunks, MemoryCallback, NULL);

    Compress(width, height, pitch, format);

    chunks.Flatten(png);
  }
}
