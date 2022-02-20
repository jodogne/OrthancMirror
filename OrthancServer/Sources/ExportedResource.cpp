/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#include "PrecompiledHeadersServer.h"
#include "ExportedResource.h"

#include "../../OrthancFramework/Sources/OrthancException.h"

namespace Orthanc
{
  void ExportedResource::Format(Json::Value& item) const
  {
    item = Json::objectValue;
    item["Seq"] = static_cast<int>(seq_);
    item["ResourceType"] = EnumerationToString(resourceType_);
    item["ID"] = publicId_;
    item["Path"] = GetBasePath(resourceType_, publicId_);
    item["RemoteModality"] = modality_;
    item["Date"] = date_;

    // WARNING: Do not add "break" below and do not reorder the case items!
    switch (resourceType_)
    {
      case ResourceType_Instance:
        item["SOPInstanceUID"] = sopInstanceUid_;

      case ResourceType_Series:
        item["SeriesInstanceUID"] = seriesInstanceUid_;

      case ResourceType_Study:
        item["StudyInstanceUID"] = studyInstanceUid_;

      case ResourceType_Patient:
        item["PatientID"] = patientId_;
        break;

      default:
        throw OrthancException(ErrorCode_InternalError);
    }
  }
}
