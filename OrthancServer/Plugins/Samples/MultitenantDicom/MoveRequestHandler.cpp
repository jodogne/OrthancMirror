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


#include "MoveRequestHandler.h"

#include "PluginToolbox.h"

#include "../../../../OrthancFramework/Sources/OrthancException.h"
#include "../../../../OrthancFramework/Sources/Toolbox.h"

#include "../Common/OrthancPluginCppWrapper.h"


class MoveRequestHandler::Iterator : public Orthanc::IMoveRequestIterator
{
private:
  std::string  targetModality_;
  Json::Value  body_;
  bool         done_;

public:
  Iterator(const std::string& targetModality,
           const Json::Value& body) :
    targetModality_(targetModality),
    body_(body),
    done_(false)
  {
  }

  virtual unsigned int GetSubOperationCount() const ORTHANC_OVERRIDE
  {
    return 1;
  }

  virtual Status DoNext() ORTHANC_OVERRIDE
  {
    Json::Value answer;

    if (done_)
    {
      return Status_Failure;
    }
    else if (OrthancPlugins::RestApiPost(answer, "/modalities/" + targetModality_ + "/store", body_, false))
    {
      done_ = true;
      return Status_Success;
    }
    else
    {
      done_ = true;
      return Status_Failure;
    }
  }
};


void MoveRequestHandler::ExecuteLookup(std::set<std::string>& publicIds,
                                       Orthanc::ResourceType level,
                                       const Orthanc::DicomTag& tag,
                                       const std::string& value) const
{
  std::vector<std::string> tokens;
  Orthanc::Toolbox::TokenizeString(tokens, value, '\\');

  for (size_t i = 0; i < tokens.size(); i++)
  {
    if (!tokens[i].empty())
    {
      Json::Value request = Json::objectValue;
      request["Level"] = Orthanc::EnumerationToString(level);
      request["Query"][tag.Format()] = tokens[i];
      PluginToolbox::AddLabelsToFindRequest(request, labels_, constraint_);

      Json::Value response;
      if (OrthancPlugins::RestApiPost(response, "/tools/find", request, false) &&
          response.type() == Json::arrayValue)
      {
        for (Json::Value::ArrayIndex j = 0; j < response.size(); j++)
        {
          if (response[j].type() != Json::stringValue)
          {
            throw Orthanc::OrthancException(Orthanc::ErrorCode_InternalError);
          }
          else
          {
            publicIds.insert(response[j].asString());
          }
        }
      }
      else
      {
        throw Orthanc::OrthancException(Orthanc::ErrorCode_InternalError);
      }
    }
  }
}


void MoveRequestHandler::LookupIdentifiers(std::set<std::string>& publicIds,
                                           Orthanc::ResourceType level,
                                           const Orthanc::DicomMap& input) const
{
  std::string value;

  switch (level)
  {
    case Orthanc::ResourceType_Patient:
      if (input.LookupStringValue(value, Orthanc::DICOM_TAG_PATIENT_ID, false) &&
          !value.empty())
      {
        ExecuteLookup(publicIds, level, Orthanc::DICOM_TAG_PATIENT_ID, value);
      }
      break;

    case Orthanc::ResourceType_Study:
      if (input.LookupStringValue(value, Orthanc::DICOM_TAG_STUDY_INSTANCE_UID, false) &&
          !value.empty())
      {
        ExecuteLookup(publicIds, level, Orthanc::DICOM_TAG_STUDY_INSTANCE_UID, value);
      }
      else if (input.LookupStringValue(value, Orthanc::DICOM_TAG_ACCESSION_NUMBER, false) &&
               !value.empty())
      {
        ExecuteLookup(publicIds, level, Orthanc::DICOM_TAG_ACCESSION_NUMBER, value);
      }
      break;

    case Orthanc::ResourceType_Series:
      if (input.LookupStringValue(value, Orthanc::DICOM_TAG_SERIES_INSTANCE_UID, false) &&
          !value.empty())
      {
        ExecuteLookup(publicIds, level, Orthanc::DICOM_TAG_SERIES_INSTANCE_UID, value);
      }
      break;

    case Orthanc::ResourceType_Instance:
      if (input.LookupStringValue(value, Orthanc::DICOM_TAG_SOP_INSTANCE_UID, false) &&
          !value.empty())
      {
        ExecuteLookup(publicIds, level, Orthanc::DICOM_TAG_SOP_INSTANCE_UID, value);
      }
      break;

    default:
      throw Orthanc::OrthancException(Orthanc::ErrorCode_ParameterOutOfRange);
  }
}


Orthanc::IMoveRequestIterator* MoveRequestHandler::Handle(const std::string& targetAet,
                                                          const Orthanc::DicomMap& input,
                                                          const std::string& originatorIp,
                                                          const std::string& originatorAet,
                                                          const std::string& calledAet,
                                                          uint16_t originatorId)
{
  std::set<std::string> publicIds;

  std::string s;
  if (input.LookupStringValue(s, Orthanc::DICOM_TAG_QUERY_RETRIEVE_LEVEL, false) &&
      !s.empty())
  {
    LookupIdentifiers(publicIds, PluginToolbox::ParseQueryRetrieveLevel(s), input);
  }
  else
  {
    // The query level is not present in the C-Move request, which
    // does not follow the DICOM standard. This is for instance the
    // behavior of Tudor DICOM. Try and automatically deduce the
    // query level: Start from the instance level, going up to the
    // patient level until a valid DICOM identifier is found.
    LookupIdentifiers(publicIds, Orthanc::ResourceType_Instance, input);

    if (publicIds.empty())
    {
      LookupIdentifiers(publicIds, Orthanc::ResourceType_Series, input);
    }

    if (publicIds.empty())
    {
      LookupIdentifiers(publicIds, Orthanc::ResourceType_Study, input);
    }

    if (publicIds.empty())
    {
      LookupIdentifiers(publicIds, Orthanc::ResourceType_Patient, input);
    }
  }

  Json::Value resources = Json::arrayValue;
  for (std::set<std::string>::const_iterator it = publicIds.begin(); it != publicIds.end(); ++it)
  {
    resources.append(*it);
  }

  std::string targetName;
  Orthanc::RemoteModalityParameters targetParameters;
  if (!PluginToolbox::LookupAETitle(targetName, targetParameters, isStrictAet_, targetAet))
  {
    throw Orthanc::OrthancException(Orthanc::ErrorCode_NetworkProtocol, "Unknown target AET: " + targetAet);
  }

  Json::Value body;
  body["CalledAet"] = calledAet;
  body["MoveOriginatorAet"] = originatorAet;
  body["MoveOriginatorID"] = originatorId;
  body["Resources"] = resources;
  body["Synchronous"] = isSynchronous_;

  return new Iterator(targetName, body);
}
