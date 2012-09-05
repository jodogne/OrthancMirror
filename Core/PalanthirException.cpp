/**
 * Palanthir - A Lightweight, RESTful DICOM Store
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


#include "PalanthirException.h"

namespace Palanthir
{
  const char* PalanthirException::What() const
  {
    if (error_ == ErrorCode_Custom)
    {
      return custom_.c_str();
    }
    else
    {
      return GetDescription(error_);
    }
  }


  const char* PalanthirException::GetDescription(ErrorCode error)
  {
    switch (error)
    {
    case ErrorCode_Success:
      return "Success";

    case ErrorCode_ParameterOutOfRange:
      return "Parameter out of range";

    case ErrorCode_NotImplemented:
      return "Not implemented yet";

    case ErrorCode_InternalError:
      return "Internal error";

    case ErrorCode_NotEnoughMemory:
      return "Not enough memory";

    case ErrorCode_UriSyntax:
      return "Badly formatted URI";

    case ErrorCode_BadParameterType:
      return "Bad type for a parameter";

    case ErrorCode_InexistentFile:
      return "Inexistent file";

    case ErrorCode_BadFileFormat:
      return "Bad file format";

    case ErrorCode_CannotWriteFile:
      return "Cannot write to file";

    case ErrorCode_Custom:
    default:
      return "???";
    }
  }
}
