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


#include "PrecompiledHeaders.h"
#include "OrthancException.h"

namespace Orthanc
{
  const char* OrthancException::What() const
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


  const char* OrthancException::GetDescription(ErrorCode error)
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

      case ErrorCode_Timeout:
        return "Timeout";

      case ErrorCode_UnknownResource:
        return "Unknown resource";

      case ErrorCode_BadSequenceOfCalls:
        return "Bad sequence of calls";

      case ErrorCode_IncompatibleDatabaseVersion:
        return "Incompatible version of the database";

      case ErrorCode_FullStorage:
        return "The file storage is full";

      case ErrorCode_InexistentItem:
        return "Accessing an inexistent item";

      case ErrorCode_BadRequest:
        return "Bad request";

      case ErrorCode_NetworkProtocol:
        return "Error in the network protocol";

      case ErrorCode_CorruptedFile:
        return "Corrupted file (inconsistent MD5 hash)";

      case ErrorCode_InexistentTag:
        return "Inexistent tag";

      case ErrorCode_ReadOnly:
        return "Cannot modify a read-only data structure";

      case ErrorCode_IncompatibleImageSize:
        return "Incompatible size of the images";

      case ErrorCode_IncompatibleImageFormat:
        return "Incompatible format of the images";

      case ErrorCode_SharedLibrary:
        return "Error while using a shared library (plugin)";

      case ErrorCode_SystemCommand:
        return "Error while calling a system command";

      case ErrorCode_Plugin:
        return "Error encountered inside a plugin";

      case ErrorCode_Database:
        return "Error with the database engine";

      case ErrorCode_Custom:
      default:
        return "???";
    }
  }
}
