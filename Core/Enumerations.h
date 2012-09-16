/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012 Medical Physics Department, CHU of Liege,
 * Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#pragma once

#include "../OrthancCppClient/HttpEnumerations.h"

namespace Orthanc
{
  enum ErrorCode
  {
    // Generic error codes
    ErrorCode_Success,
    ErrorCode_Custom,
    ErrorCode_InternalError,
    ErrorCode_NotImplemented,
    ErrorCode_ParameterOutOfRange,
    ErrorCode_NotEnoughMemory,
    ErrorCode_BadParameterType,

    // Specific error codes
    ErrorCode_UriSyntax,
    ErrorCode_InexistentFile,
    ErrorCode_CannotWriteFile,
    ErrorCode_BadFileFormat
  };

  enum PixelFormat
  {
    PixelFormat_RGB,
    PixelFormat_Grayscale8,
    PixelFormat_Grayscale16
  };
}
