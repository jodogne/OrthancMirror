/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2024 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2024 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

#include "PluginEnumerations.h"

#include "../../../../OrthancFramework/Sources/DicomNetworking/RemoteModalityParameters.h"

#include <json/value.h>

namespace PluginToolbox
{
  bool IsValidLabel(const std::string& label);
  
  Orthanc::ResourceType ParseQueryRetrieveLevel(const std::string& level);
  
  bool IsSameAETitle(bool isStrict,
                     const std::string& aet1,
                     const std::string& aet2);

  bool LookupAETitle(std::string& name,
                     Orthanc::RemoteModalityParameters& parameters,
                     bool isStrict,
                     const std::string& aet);

  void ParseLabels(std::set<std::string>& targetLabels,
                   LabelsConstraint& targetConstraint,
                   const Json::Value& serverConfig);
  
  void AddLabelsToFindRequest(Json::Value& request,
                              const std::set<std::string>& labels,
                              LabelsConstraint constraint);
  
  LabelsConstraint StringToLabelsConstraint(const std::string& s);
}
