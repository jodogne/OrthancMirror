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

#pragma once

#if !defined(ORTHANC_ENABLE_JPEG)
#  error The macro ORTHANC_ENABLE_JPEG must be defined
#endif

#if ORTHANC_ENABLE_JPEG != 1
#  error JPEG support must be enabled to include this file
#endif

#include <string.h>
#include <stdio.h>
#include <jpeglib.h>
#include <setjmp.h>
#include <string>

namespace Orthanc
{
  namespace Internals
  {
    class JpegErrorManager 
    {
    private:
      struct jpeg_error_mgr pub;  /* "public" fields */
      jmp_buf setjmp_buffer;      /* for return to caller */
      std::string message;

      static void OutputMessage(j_common_ptr cinfo);

      static void ErrorExit(j_common_ptr cinfo);

    public:
      JpegErrorManager();

      struct jpeg_error_mgr* GetPublic()
      {
        return &pub;
      }

      jmp_buf& GetJumpBuffer()
      {
        return setjmp_buffer;
      }

      const std::string& GetMessage() const
      {
        return message;
      }
    };
  }
}
