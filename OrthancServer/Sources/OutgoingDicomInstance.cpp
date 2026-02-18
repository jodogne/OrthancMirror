/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2026 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2026 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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
#include "OutgoingDicomInstance.h"

#include "OrthancConfiguration.h"

#include "../../OrthancFramework/Sources/DicomParsing/ParsedDicomFile.h"
#include "../../OrthancFramework/Sources/OrthancException.h"
#include "../../OrthancFramework/Sources/Toolbox.h"


namespace Orthanc
{
  OutgoingDicomInstance* OutgoingDicomInstance::CreateFromBuffer(const std::string& buffer)
  {
    return CreateFromBuffer(buffer.empty() ? NULL : buffer.c_str(), buffer.size());
  }

  OutgoingDicomInstance* OutgoingDicomInstance::CreateFromBuffer(const void* buffer, size_t size)
  {
    return new OutgoingDicomInstance(new ParsedDicomFile(buffer, size));
  }

  void OutgoingDicomInstance::GetSimplifiedJson(Json::Value& simplifiedTags) const
  {
    if (dicom_.get() == NULL)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    std::set<DicomTag> allMainDicomTags;
    DicomMap::GetAllMainDicomTags(allMainDicomTags);

    Json::Value dicomAsJson;
    OrthancConfiguration::DefaultDicomDatasetToJson(dicomAsJson, *dicom_, allMainDicomTags); // don't crop any main dicom tags

    Toolbox::SimplifyDicomAsJson(simplifiedTags, dicomAsJson, DicomToJsonFormat_Human);
  }

}
