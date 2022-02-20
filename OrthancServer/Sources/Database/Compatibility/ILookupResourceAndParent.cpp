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


#include "../../PrecompiledHeadersServer.h"
#include "ILookupResourceAndParent.h"

#include "../../../../OrthancFramework/Sources/OrthancException.h"

namespace Orthanc
{
  namespace Compatibility
  {
    bool ILookupResourceAndParent::Apply(ILookupResourceAndParent& database,
                                         int64_t& id,
                                         ResourceType& type,
                                         std::string& parentPublicId,
                                         const std::string& publicId)
    {
      if (!database.LookupResource(id, type, publicId))
      {
        return false;
      }
      else if (type == ResourceType_Patient)
      {
        parentPublicId.clear();
        return true;
      }
      else
      {
        int64_t parentId;
        if (!database.LookupParent(parentId, id))
        {
          throw OrthancException(ErrorCode_InternalError);
        }

        parentPublicId = database.GetPublicId(parentId);
        return true;
      }
    }
  }
}
