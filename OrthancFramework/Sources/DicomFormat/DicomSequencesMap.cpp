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

#include "../PrecompiledHeaders.h"
#include "DicomSequencesMap.h"
#include "../Toolbox.h"

namespace Orthanc
{

#if ORTHANC_ENABLE_DCMTK == 1

  // copy all tags from Json (used to read from metadata)
  void DicomSequencesMap::Deserialize(const Json::Value& serialized)
  {
    Json::Value::Members members = serialized.getMemberNames();
    for (size_t i = 0; i < members.size(); i++)
    {
      DicomTag tag(0, 0);
      if (DicomTag::ParseHexadecimal(tag, members[i].c_str()))
      {
        sequences_[tag] = serialized[members[i]];
      }
    }
  }

#endif

  // serialize a subet of tags (used to store in the metadata)
  void DicomSequencesMap::Serialize(Json::Value& target, const std::set<DicomTag>& tags) const
  {
    // add the sequences to "target"
    for (std::map<DicomTag, Json::Value>::const_iterator it = sequences_.begin();
          it != sequences_.end(); ++it)
    {
      if (tags.find(it->first) != tags.end())
      {
        target[it->first.Format()] = it->second;
      }
    }
  }

  // copy a subset of tags from Json
  void DicomSequencesMap::FromDicomAsJson(const Json::Value& dicomAsJson, const std::set<DicomTag>& tags)
  {
    for (std::set<DicomTag>::const_iterator it = tags.begin();
         it != tags.end(); ++it)
    {
      std::string tag = it->Format();
      if (dicomAsJson.isMember(tag))
      {
        sequences_[*it] = dicomAsJson[tag];
      }
    }
  }

  void DicomSequencesMap::ToJson(Json::Value& target, DicomToJsonFormat format, const std::set<DicomTag>& tags) const
  {
    // add the sequences to "target"
    for (std::map<DicomTag, Json::Value>::const_iterator it = sequences_.begin();
          it != sequences_.end(); ++it)
    {
      Json::Value sequenceFullJson = Json::objectValue;
      sequenceFullJson[it->first.Format()] = it->second;

      Json::Value& requestedFormatJson = sequenceFullJson;
      Json::Value convertedJson;

      if (format != DicomToJsonFormat_Full)
      {
        Toolbox::SimplifyDicomAsJson(convertedJson, sequenceFullJson, format);
        requestedFormatJson = convertedJson;
      }
      
      Json::Value::Members keys = requestedFormatJson.getMemberNames();  
      for (size_t i = 0; i < keys.size(); i++)  // there should always be only one member in this JSON
      {
        target[keys[i]] = requestedFormatJson[keys[i]];
      }
    }
  }
}