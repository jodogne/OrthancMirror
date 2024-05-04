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


#include "GenericFind.h"

#include "../../../../OrthancFramework/Sources/DicomFormat/DicomArray.h"
#include "../../../../OrthancFramework/Sources/OrthancException.h"


namespace Orthanc
{
  namespace Compatibility
  {
    void GenericFind::ExecuteFind(std::list<std::string>& identifiers,
                                  const FindRequest& request,
                                  const std::vector<DatabaseConstraint>& normalized)
    {
      if (!request.GetOrthancIdentifiers().HasPatientId() &&
          !request.GetOrthancIdentifiers().HasStudyId() &&
          !request.GetOrthancIdentifiers().HasSeriesId() &&
          !request.GetOrthancIdentifiers().HasInstanceId() &&
          request.GetDicomTagConstraintsCount() == 0 &&
          request.GetMetadataConstraintsCount() == 0 &&
          request.GetOrdering().empty() &&
          request.GetLabels().empty())
      {
        if (request.HasLimits())
        {
          transaction_.GetAllPublicIds(identifiers, request.GetLevel(), request.GetLimitsSince(), request.GetLimitsCount());
        }
        else
        {
          transaction_.GetAllPublicIds(identifiers, request.GetLevel());
        }
      }
      else
      {
        throw OrthancException(ErrorCode_NotImplemented);  // Not supported
      }
    }


    void GenericFind::ExecuteExpand(FindResponse& response,
                                    const FindRequest& request,
                                    const std::string& identifier)
    {
      int64_t internalId;
      ResourceType t;
      if (!transaction_.LookupResource(internalId, t, identifier) ||
          t != request.GetLevel())
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      std::unique_ptr<FindResponse::Resource> resource(new FindResponse::Resource(request.GetLevel(), identifier));

      if (request.IsRetrieveMainDicomTags())
      {
        DicomMap m;
        transaction_.GetMainDicomTags(m, internalId);

        DicomArray a(m);
        for (size_t i = 0; i < a.GetSize(); i++)
        {
          const DicomElement& element = a.GetElement(i);
          if (element.GetValue().IsString())
          {
            resource->AddStringDicomTag(element.GetTag().GetGroup(), element.GetTag().GetElement(),
                                        element.GetValue().GetContent());
          }
          else
          {
            throw OrthancException(ErrorCode_BadParameterType);
          }
        }
      }

      if (request.IsRetrieveParentIdentifier())
      {
        int64_t parentId;
        if (transaction_.LookupParent(parentId, internalId))
        {
          resource->SetParentIdentifier(transaction_.GetPublicId(parentId));
        }
        else
        {
          throw OrthancException(ErrorCode_InternalError);
        }
      }

      // TODO-FIND: Continue


      /**
       * Sanity checks
       **/

      if (request.IsRetrieveMainDicomTags())
      {
        DicomMap tmp;
        resource->GetMainDicomTags(tmp);
        if (tmp.GetSize() == 0)
        {
          throw OrthancException(ErrorCode_InternalError);
        }
      }


      response.Add(resource.release());
    }
  }
}
