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
  HttpStatus OrthancException::ConvertToHttpStatus(ErrorCode code)
  {
    switch (code)
    {
      case ErrorCode_Success:
      {
        return HttpStatus_200_Ok;
      }

      case ErrorCode_InexistentFile:
      case ErrorCode_InexistentItem:
      case ErrorCode_InexistentTag:
      case ErrorCode_UnknownResource:
      {
        return HttpStatus_404_NotFound;
      }

      case ErrorCode_BadFileFormat:
      case ErrorCode_BadParameterType:
      case ErrorCode_BadRequest:
      case ErrorCode_ParameterOutOfRange:
      case ErrorCode_UriSyntax:
      {
        return HttpStatus_400_BadRequest;
        break;
      }

      default:
      {
        return HttpStatus_500_InternalServerError;
      }
    }
  }

  const char* OrthancException::What() const
  {
    if (errorCode_ == ErrorCode_Custom)
    {
      return custom_.c_str();
    }
    else
    {
      return EnumerationToString(errorCode_);
    }
  }


  OrthancException::OrthancException(ErrorCode errorCode) : 
    errorCode_(errorCode),
    httpStatus_(ConvertToHttpStatus(errorCode))
  {
  }
}
