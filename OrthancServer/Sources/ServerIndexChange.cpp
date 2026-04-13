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
#include "ServerIndexChange.h"

#include "../../OrthancFramework/Sources/SystemToolbox.h"


namespace Orthanc
{
  ServerIndexChange::ServerIndexChange(ChangeType changeType,
                                       ResourceType resourceType,
                                       const std::string& publicId) :
    seq_(-1),
    changeType_(changeType),
    resourceType_(resourceType),
    publicId_(publicId),
    date_(SystemToolbox::GetNowIsoString(true /* use UTC time (not local time) */))
  {
  }


  ServerIndexChange::ServerIndexChange(int64_t seq,
                                       ChangeType changeType,
                                       ResourceType resourceType,
                                       const std::string& publicId,
                                       const std::string& date) :
    seq_(seq),
    changeType_(changeType),
    resourceType_(resourceType),
    publicId_(publicId),
    date_(date)
  {
  }


  ServerIndexChange::ServerIndexChange(const ServerIndexChange& other) :
    seq_(other.seq_),
    changeType_(other.changeType_),
    resourceType_(other.resourceType_),
    publicId_(other.publicId_),
    date_(other.date_)
  {
  }


  void ServerIndexChange::Format(Json::Value& item) const
  {
    item = Json::objectValue;
    item["Seq"] = static_cast<int>(seq_);
    item["ChangeType"] = EnumerationToString(changeType_);
    item["ResourceType"] = EnumerationToString(resourceType_);
    item["ID"] = publicId_;
    item["Path"] = GetBasePath(resourceType_, publicId_);
    item["Date"] = date_;
  }
}
