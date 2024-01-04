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


#include "FindRequestHandler.h"

#include "PluginToolbox.h"

#include "../../../../OrthancFramework/Sources/DicomParsing/FromDcmtkBridge.h"
#include "../../../../OrthancFramework/Sources/OrthancException.h"

#include "../Common/OrthancPluginCppWrapper.h"


void FindRequestHandler::Handle(Orthanc::DicomFindAnswers& answers,
                                const Orthanc::DicomMap& input,
                                const std::list<Orthanc::DicomTag>& sequencesToReturn,
                                const std::string& remoteIp,
                                const std::string& remoteAet,
                                const std::string& calledAet,
                                Orthanc::ModalityManufacturer manufacturer)
{
  std::set<Orthanc::DicomTag> tags;
  input.GetTags(tags);

  Json::Value request = Json::objectValue;
  request["Expand"] = true;
  PluginToolbox::AddLabelsToFindRequest(request, labels_, constraint_);

  Json::Value query = Json::objectValue;
  std::string level;

  for (std::set<Orthanc::DicomTag>::const_iterator it = tags.begin(); it != tags.end(); ++it)
  {
    std::string s;

    if (input.LookupStringValue(s, *it, false) &&
        !s.empty())
    {
      if (*it == Orthanc::DICOM_TAG_QUERY_RETRIEVE_LEVEL)
      {
        level = s;
      }
      else 
      {
        query[it->Format()] = s;
      }
    }
  }

  if (level.empty())
  {
    throw Orthanc::OrthancException(Orthanc::ErrorCode_NetworkProtocol, "Missing QueryRetrieveLevel in DICOM C-FIND request");
  }

  request["Level"] = EnumerationToString(PluginToolbox::ParseQueryRetrieveLevel(level));
  request["Query"] = query;

  Json::Value response;
  if (!OrthancPlugins::RestApiPost(response, "/tools/find", request, false) ||
      response.type() != Json::arrayValue)
  {
    throw Orthanc::OrthancException(Orthanc::ErrorCode_NetworkProtocol, "Invalid DICOM C-FIND request");
  }

  for (Json::Value::ArrayIndex i = 0; i < response.size(); i++)
  {
    if (response[i].type() != Json::objectValue ||
        !response[i].isMember(KEY_MAIN_DICOM_TAGS))
    {
      throw Orthanc::OrthancException(Orthanc::ErrorCode_InternalError);
    }

    if (response[i].isMember(KEY_PATIENT_MAIN_DICOM_TAGS) &&
        response[i][KEY_PATIENT_MAIN_DICOM_TAGS].type() != Json::objectValue)
    {
      throw Orthanc::OrthancException(Orthanc::ErrorCode_InternalError);
    }
            
    Orthanc::DicomMap m;

    for (std::set<Orthanc::DicomTag>::const_iterator it = tags.begin(); it != tags.end(); ++it)
    {
      const std::string tag = Orthanc::FromDcmtkBridge::GetTagName(*it, "");

      if (response[i][KEY_MAIN_DICOM_TAGS].isMember(tag) &&
          response[i][KEY_MAIN_DICOM_TAGS][tag].type() == Json::stringValue)
      {
        m.SetValue(*it, response[i][KEY_MAIN_DICOM_TAGS][tag].asString(), false);
      }
      else if (response[i].isMember(KEY_PATIENT_MAIN_DICOM_TAGS) &&
               response[i][KEY_PATIENT_MAIN_DICOM_TAGS].isMember(tag) &&
               response[i][KEY_PATIENT_MAIN_DICOM_TAGS][tag].type() == Json::stringValue)
      {
        m.SetValue(*it, response[i][KEY_PATIENT_MAIN_DICOM_TAGS][tag].asString(), false);
      }        
    }
            
    m.SetValue(Orthanc::DICOM_TAG_QUERY_RETRIEVE_LEVEL, level, false);
    m.SetValue(Orthanc::DICOM_TAG_RETRIEVE_AE_TITLE, retrieveAet_, false);
    answers.Add(m);
  }

  answers.SetComplete(true);
}
