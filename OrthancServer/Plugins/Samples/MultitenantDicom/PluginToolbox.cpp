/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2024 Osimis S.A., Belgium
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


#include "PluginToolbox.h"

#include "../../../../OrthancFramework/Sources/OrthancException.h"
#include "../../../../OrthancFramework/Sources/SerializationToolbox.h"
#include "../../../../OrthancFramework/Sources/Toolbox.h"

#include "../Common/OrthancPluginCppWrapper.h"


namespace PluginToolbox
{
  bool IsValidLabel(const std::string& label)
  {
    if (label.empty())
    {
      return false;
    }

    if (label.size() > 64)
    {
      // This limitation is for MySQL, which cannot use a TEXT
      // column of undefined length as a primary key
      return false;
    }
      
    for (size_t i = 0; i < label.size(); i++)
    {
      if (!(label[i] == '_' ||
            label[i] == '-' ||
            (label[i] >= 'a' && label[i] <= 'z') ||
            (label[i] >= 'A' && label[i] <= 'Z') ||
            (label[i] >= '0' && label[i] <= '9')))
      {
        return false;
      }
    }

    return true;
  }


  Orthanc::ResourceType ParseQueryRetrieveLevel(const std::string& level)
  {
    if (level == "PATIENT")
    {
      return Orthanc::ResourceType_Patient;
    }
    else if (level == "STUDY")
    {
      return Orthanc::ResourceType_Study;
    }
    else if (level == "SERIES")
    {
      return Orthanc::ResourceType_Series;
    }
    else if (level == "INSTANCE")
    {
      return Orthanc::ResourceType_Instance;
    }
    else
    {
      throw Orthanc::OrthancException(Orthanc::ErrorCode_NetworkProtocol, "Bad value for QueryRetrieveLevel in DICOM C-FIND: " + level);
    }
  }


  bool IsSameAETitle(bool isStrict,
                     const std::string& aet1,
                     const std::string& aet2)
  {
    if (isStrict)
    {
      // Case-sensitive matching
      return aet1 == aet2;
    }
    else
    {
      // Case-insensitive matching (default)
      std::string tmp1, tmp2;
      Orthanc::Toolbox::ToLowerCase(tmp1, aet1);
      Orthanc::Toolbox::ToLowerCase(tmp2, aet2);
      return tmp1 == tmp2;
    }
  }


  bool LookupAETitle(std::string& name,
                     Orthanc::RemoteModalityParameters& parameters,
                     bool isStrict,
                     const std::string& aet)
  {
    Json::Value modalities;
    if (!OrthancPlugins::RestApiGet(modalities, "/modalities?expand", false))
    {
      throw Orthanc::OrthancException(Orthanc::ErrorCode_InternalError, "Unable to obtain the list of the remote modalities");
    }

    std::vector<std::string> names = modalities.getMemberNames();
    for (size_t i = 0; i < names.size(); i++)
    {
      parameters = Orthanc::RemoteModalityParameters(modalities[names[i]]);
      
      if (IsSameAETitle(isStrict, parameters.GetApplicationEntityTitle(), aet))
      {
        name = names[i];
        return true;
      }
    }

    return false;
  }


  void ParseLabels(std::set<std::string>& targetLabels,
                   LabelsConstraint& targetConstraint,
                   const Json::Value& serverConfig)
  {
    Orthanc::SerializationToolbox::ReadSetOfStrings(targetLabels, serverConfig, KEY_LABELS);

    for (std::set<std::string>::const_iterator it = targetLabels.begin(); it != targetLabels.end(); ++it)
    {
      if (!PluginToolbox::IsValidLabel(*it))
      {
        throw Orthanc::OrthancException(Orthanc::ErrorCode_ParameterOutOfRange, "Invalid label: " + *it);
      }
    }

    std::string s = Orthanc::SerializationToolbox::ReadString(serverConfig, KEY_LABELS_CONSTRAINT, KEY_ALL);
    targetConstraint = PluginToolbox::StringToLabelsConstraint(s);
  }
  

  void AddLabelsToFindRequest(Json::Value& request,
                              const std::set<std::string>& labels,
                              LabelsConstraint constraint)
  {
    Json::Value items = Json::arrayValue;
    for (std::set<std::string>::const_iterator it = labels.begin(); it != labels.end(); ++it)
    {
      items.append(*it);
    }

    request[KEY_LABELS] = items;

    switch (constraint)
    {
      case LabelsConstraint_All:
        request[KEY_LABELS_CONSTRAINT] = KEY_ALL;
        break;

      case LabelsConstraint_Any:
        request[KEY_LABELS_CONSTRAINT] = KEY_ANY;
        break;

      case LabelsConstraint_None:
        request[KEY_LABELS_CONSTRAINT] = KEY_NONE;
        break;

      default:
        throw Orthanc::OrthancException(Orthanc::ErrorCode_ParameterOutOfRange);
    }
  }


  LabelsConstraint StringToLabelsConstraint(const std::string& s)
  {
    if (s == KEY_ALL)
    {
      return LabelsConstraint_All;
    }
    else if (s == KEY_ANY)
    {
      return LabelsConstraint_Any;
    }
    else if (s == KEY_NONE)
    {
      return LabelsConstraint_None;
    }
    else
    {
      throw Orthanc::OrthancException(Orthanc::ErrorCode_ParameterOutOfRange, "Bad value for constraint of labels: " + s);
    }
  }
}
