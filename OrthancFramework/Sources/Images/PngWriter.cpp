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


#include "../PrecompiledHeaders.h"
#include "PngWriter.h"

#include <vector>
#include <stdint.h>
#include <png.h>
#include "../OrthancException.h"
#include "../ChunkedBuffer.h"
#include "../Toolbox.h"

#if ORTHANC_SANDBOXED == 0
#  include "../SystemToolbox.h"
#endif


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
  /**
   * The "png_" structure cannot be safely reused if the bpp changes
   * between successive invocations. This can lead to "Invalid reads"
   * reported by valgrind if writing a 16bpp image, then a 8bpp image
   * using the same "PngWriter" object. Starting with Orthanc 1.9.3,
   * we recreate a new "png_" context each time a PNG image must be
   * written so as to prevent such invalid reads.
   **/
  class PngWriter::Context : public boost::noncopyable
  {
  private:
    png_structp png_;
    png_infop info_;

    // Filled by Prepare()
    std::vector<uint8_t*> rows_;
    int bitDepth_;
    int colorType_;

  public:
    Context() :
      png_(NULL),
      info_(NULL),
      bitDepth_(0),  // Dummy initialization
      colorType_(0)  // Dummy initialization
    {
      png_ = png_create_write_struct
        (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL); //this, ErrorHandler, WarningHandler);
      if (!png_)
      {
        throw OrthancException(ErrorCode_NotEnoughMemory);
      }

      info_ = png_create_info_struct(png_);
      if (!info_)
      {
        png_destroy_write_struct(&png_, NULL);
        throw OrthancException(ErrorCode_NotEnoughMemory);
      }
    }

    
    ~Context()
    {
      if (info_)
      {
        png_destroy_info_struct(png_, &info_);
      }

      if (png_)
      {
        png_destroy_write_struct(&png_, NULL);
      }
    }
    

    png_structp GetObject() const
    {
      return png_;
    }

    
    void Prepare(unsigned int width,
                 unsigned int height,
                 unsigned int pitch,
                 PixelFormat format,
                 const void* buffer)
    {
      rows_.resize(height);
      for (unsigned int y = 0; y < height; y++)
      {
        rows_[y] = const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(buffer)) + y * pitch;
      }

      switch (format)
      {
        case PixelFormat_RGB24:
          bitDepth_ = 8;
          colorType_ = PNG_COLOR_TYPE_RGB;
          break;

        case PixelFormat_RGBA32:
          bitDepth_ = 8;
          colorType_ = PNG_COLOR_TYPE_RGBA;
          break;

        case PixelFormat_Grayscale8:
          bitDepth_ = 8;
          colorType_ = PNG_COLOR_TYPE_GRAY;
          break;

        case PixelFormat_Grayscale16:
        case PixelFormat_SignedGrayscale16:
          bitDepth_ = 16;
          colorType_ = PNG_COLOR_TYPE_GRAY;
          break;
        
        case PixelFormat_RGBA64:
          bitDepth_ = 16;
          colorType_ = PNG_COLOR_TYPE_RGBA;
          break;

        default:
          throw OrthancException(ErrorCode_NotImplemented);
      }
    }

    
    void Compress(unsigned int width,
                  unsigned int height,
                  unsigned int pitch,
                  PixelFormat format)
    {
      png_set_IHDR(png_, info_, width, height,
                   bitDepth_, colorType_, PNG_INTERLACE_NONE,
                   PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

      png_write_info(png_, info_);

      if (height > 0)
      {
        switch (format)
        {
          case PixelFormat_Grayscale16:
          case PixelFormat_SignedGrayscale16:
          case PixelFormat_RGBA64:
          {
            int transforms = 0;
            if (Toolbox::DetectEndianness() == Endianness_Little)
            {
              transforms = PNG_TRANSFORM_SWAP_ENDIAN;
            }

            png_set_rows(png_, info_, &rows_[0]);
            png_write_png(png_, info_, transforms, NULL);

            break;
          }

          default:
            png_write_image(png_, &rows_[0]);
        }
      }

      png_write_end(png_, NULL);
    }
  };


#if ORTHANC_SANDBOXED == 0
  void PngWriter::WriteToFileInternal(const std::string& filename,
                                      unsigned int width,
                                      unsigned int height,
                                      unsigned int pitch,
                                      PixelFormat format,
                                      const void* buffer)
  {
    Context context;
    
    context.Prepare(width, height, pitch, format, buffer);

    FILE* fp = SystemToolbox::OpenFile(filename, FileMode_WriteBinary);
    if (!fp)
    {
      throw OrthancException(ErrorCode_CannotWriteFile);
    }    

    png_init_io(context.GetObject(), fp);

    if (setjmp(png_jmpbuf(context.GetObject())))
    {
      // Error during writing PNG
      throw OrthancException(ErrorCode_CannotWriteFile);      
    }

    context.Compress(width, height, pitch, format);

    fclose(fp);
  }
#endif


  static void MemoryCallback(png_structp png_ptr, 
                             png_bytep data, 
                             png_size_t size)
  {
    ChunkedBuffer* buffer = reinterpret_cast<ChunkedBuffer*>(png_get_io_ptr(png_ptr));
    buffer->AddChunk(data, size);
  }


  void PngWriter::WriteToMemoryInternal(std::string& png,
                                        unsigned int width,
                                        unsigned int height,
                                        unsigned int pitch,
                                        PixelFormat format,
                                        const void* buffer)
  {
    Context context;
    
    ChunkedBuffer chunks;

    context.Prepare(width, height, pitch, format, buffer);

    if (setjmp(png_jmpbuf(context.GetObject())))
    {
      // Error during writing PNG
      throw OrthancException(ErrorCode_InternalError);      
    }

    png_set_write_fn(context.GetObject(), &chunks, MemoryCallback, NULL);

    context.Compress(width, height, pitch, format);

    chunks.Flatten(png);
  }
}
