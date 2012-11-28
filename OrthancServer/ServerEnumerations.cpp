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

#include "ServerEnumerations.h"

#include "../Core/OrthancException.h"

namespace Orthanc
{
  const char* ToString(ResourceType type)
  {
    switch (type)
    {
    case ResourceType_Patient:
      return "Patient";

    case ResourceType_Study:
      return "Study";

    case ResourceType_Series:
      return "Series";

    case ResourceType_Instance:
      return "Instance";
      
    default:
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }

  std::string GetBasePath(ResourceType type,
                          const std::string& publicId)
  {
    switch (type)
    {
    case ResourceType_Patient:
      return "/patients/" + publicId;

    case ResourceType_Study:
      return "/studies/" + publicId;

    case ResourceType_Series:
      return "/series/" + publicId;

    case ResourceType_Instance:
      return "/instances/" + publicId;
      
    default:
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }

  const char* ToString(SeriesStatus status)
  {
    switch (status)
    {
    case SeriesStatus_Complete:
      return "Complete";

    case SeriesStatus_Missing:
      return "Missing";

    case SeriesStatus_Inconsistent:
      return "Inconsistent";

    case SeriesStatus_Unknown:
      return "Unknown";

    default:
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }

  const char* ToString(ChangeType type)
  {
    switch (type)
    {
    case ChangeType_CompletedSeries:
      return "CompletedSeries";

    case ChangeType_NewInstance:
      return "NewInstance";

    case ChangeType_NewPatient:
      return "NewPatient";

    case ChangeType_NewSeries:
      return "NewSeries";

    case ChangeType_NewStudy:
      return "NewStudy";

    default:
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }
}
