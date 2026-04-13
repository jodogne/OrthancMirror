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


#pragma once

#include "ServerEnumerations.h"
#include "../../OrthancFramework/Sources/IDynamicObject.h"

#include <string>
#include <json/value.h>

namespace Orthanc
{
  struct ServerIndexChange : public IDynamicObject
  {
  private:
    int64_t      seq_;
    ChangeType   changeType_;
    ResourceType resourceType_;
    std::string  publicId_;
    std::string  date_;

  public:
    ServerIndexChange(ChangeType changeType,
                      ResourceType resourceType,
                      const std::string& publicId);

    ServerIndexChange(int64_t seq,
                      ChangeType changeType,
                      ResourceType resourceType,
                      const std::string& publicId,
                      const std::string& date);

    ServerIndexChange(const ServerIndexChange& other);

    ServerIndexChange* Clone() const
    {
      return new ServerIndexChange(*this);
    }

    int64_t  GetSeq() const
    {
      return seq_;
    }

    ChangeType  GetChangeType() const
    {
      return changeType_;
    }

    ResourceType  GetResourceType() const
    {
      return resourceType_;
    }

    const std::string&  GetPublicId() const
    {
      return publicId_;
    }

    const std::string& GetDate() const
    {
      return date_;
    }

    void Format(Json::Value& item) const;
  };
}
