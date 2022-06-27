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


#pragma once

#include "../../../OrthancFramework/Sources/DicomFormat/DicomTag.h"

#include <json/value.h>
#include <boost/noncopyable.hpp>

#include <map>

namespace Orthanc
{
  class DatabaseLookup;
  class ParsedDicomFile;
  struct ServerIndexChange;

  /*
   * contains a map of dicom sequences where:
   * the key is a DicomTag
   * the sequence is serialized in Json "full" format
   */
  struct DicomSequencesMap : public boost::noncopyable
  {
    std::map<DicomTag, Json::Value>     sequences_;

    void Deserialize(const Json::Value& serialized);
    void Serialize(Json::Value& target, const std::set<DicomTag>& tagsSubset) const;
    void FromDicomAsJson(const Json::Value& dicomAsJson, const std::set<DicomTag>& tagsSubset);
    void ToJson(Json::Value& target, DicomToJsonFormat format, const std::set<DicomTag>& tagsSubset) const;

    size_t GetSize() const
    {
      return sequences_.size();
    }
  };
}
