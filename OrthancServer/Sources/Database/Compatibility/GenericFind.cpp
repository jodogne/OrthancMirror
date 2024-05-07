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
          request.GetLabels().empty() &&
          request.GetOrdering().empty())
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
      ResourceType level;
      std::string parent;

      if (request.IsRetrieveParentIdentifier())
      {
        if (!transaction_.LookupResourceAndParent(internalId, level, parent, identifier))
        {
          return;  // The resource is not available anymore
        }

        if (level == ResourceType_Patient)
        {
          if (!parent.empty())
          {
            throw OrthancException(ErrorCode_DatabasePlugin);
          }
        }
        else
        {
          if (parent.empty())
          {
            throw OrthancException(ErrorCode_DatabasePlugin);
          }
        }
      }
      else
      {
        if (!transaction_.LookupResource(internalId, level, identifier))
        {
          return;  // The resource is not available anymore
        }
      }

      if (level != request.GetLevel())
      {
        throw OrthancException(ErrorCode_DatabasePlugin);
      }

      std::unique_ptr<FindResponse::Resource> resource(new FindResponse::Resource(request.GetLevel(), identifier));

      if (request.IsRetrieveParentIdentifier())
      {
        assert(!parent.empty());
        resource->SetParentIdentifier(parent);
      }

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

      if (request.IsRetrieveMetadata())
      {
        transaction_.GetAllMetadata(resource->GetMetadata(), internalId);
      }

      if (request.IsRetrieveLabels())
      {
        transaction_.ListLabels(resource->GetLabels(), internalId);
      }

      if (request.IsRetrieveAttachments())
      {
        std::set<FileContentType> attachments;
        transaction_.ListAvailableAttachments(attachments, internalId);

        for (std::set<FileContentType>::const_iterator it = attachments.begin(); it != attachments.end(); ++it)
        {
          FileInfo info;
          int64_t revision;
          if (transaction_.LookupAttachment(info, revision, internalId, *it) &&
              info.GetContentType() == *it)
          {
            resource->AddAttachment(info);
          }
          else
          {
            throw OrthancException(ErrorCode_DatabasePlugin);
          }
        }
      }

      if (request.IsRetrieveChildrenIdentifiers())
      {
        std::list<std::string> children;
        transaction_.GetChildrenPublicId(children, internalId);

        for (std::list<std::string>::const_iterator it = children.begin(); it != children.end(); ++it)
        {
          resource->AddChildIdentifier(GetChildResourceType(level), *it);
        }
      }

      for (std::set<MetadataType>::const_iterator it = request.GetRetrieveChildrenMetadata().begin();
           it != request.GetRetrieveChildrenMetadata().end(); ++it)
      {
        std::list<std::string> values;
        transaction_.GetChildrenMetadata(values, internalId, *it);
        resource->AddChildrenMetadata(*it, values);
      }

      response.Add(resource.release());
    }
  }
}
