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
#include "JpegReader.h"

#include "JpegErrorManager.h"
#include "../OrthancException.h"
#include "../Logging.h"

#if ORTHANC_SANDBOXED == 0
#  include "../SystemToolbox.h"
#endif


namespace Orthanc
{
  static void Uncompress(struct jpeg_decompress_struct& cinfo,
                         std::string& content,
                         ImageAccessor& accessor)
  {
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);

    PixelFormat format;
    if (cinfo.output_components == 1 &&
        cinfo.out_color_space == JCS_GRAYSCALE)
    {
      format = PixelFormat_Grayscale8;
    }
    else if (cinfo.output_components == 3 &&
             cinfo.out_color_space == JCS_RGB)
    {
      format = PixelFormat_RGB24;
    }
    else
    {
      throw OrthancException(ErrorCode_NotImplemented);
    }

    unsigned int pitch = cinfo.output_width * cinfo.output_components;

    /* Make a one-row-high sample array that will go away when done with image */
    JSAMPARRAY buffer = (*cinfo.mem->alloc_sarray) ((j_common_ptr) &cinfo, JPOOL_IMAGE, pitch, 1);

    try
    {
      content.resize(pitch * cinfo.output_height);
    }
    catch (...)
    {
      throw OrthancException(ErrorCode_NotEnoughMemory);
    }

    accessor.AssignWritable(format, cinfo.output_width, cinfo.output_height, pitch, 
                            content.empty() ? NULL : &content[0]);

    uint8_t* target = reinterpret_cast<uint8_t*>(&content[0]);
    while (cinfo.output_scanline < cinfo.output_height) 
    {
      jpeg_read_scanlines(&cinfo, buffer, 1);
      memcpy(target, buffer[0], pitch);
      target += pitch;
    }

    // Everything went fine, "setjmp()" didn't get called

    jpeg_finish_decompress(&cinfo);
  }


#if ORTHANC_SANDBOXED == 0
  void JpegReader::ReadFromFile(const std::string& filename)
  {
    FILE* fp = SystemToolbox::OpenFile(filename, FileMode_ReadBinary);
    if (!fp)
    {
      throw OrthancException(ErrorCode_InexistentFile);
    }

    struct jpeg_decompress_struct cinfo;
    memset(&cinfo, 0, sizeof(struct jpeg_decompress_struct));

    Internals::JpegErrorManager jerr;
    cinfo.err = jerr.GetPublic();
    
    if (setjmp(jerr.GetJumpBuffer())) 
    {
      jpeg_destroy_decompress(&cinfo);
      fclose(fp);
      LOG(ERROR) << "Error during JPEG decoding: " << jerr.GetMessage();
      throw OrthancException(ErrorCode_InternalError);
    }

    // Below this line, we are under the scope of a "setjmp"

    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, fp);

    try
    {
      Uncompress(cinfo, content_, *this);
    }
    catch (OrthancException&)
    {
      jpeg_destroy_decompress(&cinfo);
      fclose(fp);
      throw;
    }

    jpeg_destroy_decompress(&cinfo);
    fclose(fp);
  }
#endif


  void JpegReader::ReadFromMemory(const void* buffer,
                                  size_t size)
  {
    struct jpeg_decompress_struct cinfo;
    memset(&cinfo, 0, sizeof(struct jpeg_decompress_struct));

    Internals::JpegErrorManager jerr;
    cinfo.err = jerr.GetPublic();
    
    if (setjmp(jerr.GetJumpBuffer())) 
    {
      jpeg_destroy_decompress(&cinfo);
      LOG(ERROR) << "Error during JPEG decoding: " << jerr.GetMessage();
      throw OrthancException(ErrorCode_InternalError);
    }

    // Below this line, we are under the scope of a "setjmp"
    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(buffer)), size);

    try
    {
      Uncompress(cinfo, content_, *this);
    }
    catch (OrthancException&)
    {
      jpeg_destroy_decompress(&cinfo);
      throw;
    }

    jpeg_destroy_decompress(&cinfo);
  }


  void JpegReader::ReadFromMemory(const std::string& buffer)
  {
    if (buffer.empty())
    {
      ReadFromMemory(NULL, 0);
    }
    else
    {
      ReadFromMemory(buffer.c_str(), buffer.size());
    }
  }
}
