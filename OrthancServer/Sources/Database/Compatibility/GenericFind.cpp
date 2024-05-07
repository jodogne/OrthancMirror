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

#include <stack>


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


    void GenericFind::RetrieveMainDicomTags(FindResponse::Resource& target,
                                            ResourceType level,
                                            int64_t internalId)
    {
      DicomMap m;
      transaction_.GetMainDicomTags(m, internalId);

      DicomArray a(m);
      for (size_t i = 0; i < a.GetSize(); i++)
      {
        const DicomElement& element = a.GetElement(i);
        if (element.GetValue().IsString())
        {
          target.AddStringDicomTag(level, element.GetTag().GetGroup(),
                                   element.GetTag().GetElement(), element.GetValue().GetContent());
        }
        else
        {
          throw OrthancException(ErrorCode_BadParameterType);
        }
      }
    }


    static ResourceType GetTopLevelOfInterest(const FindRequest& request)
    {
      switch (request.GetLevel())
      {
        case ResourceType_Patient:
          return ResourceType_Patient;

        case ResourceType_Study:
          if (request.IsRetrieveMainDicomTags(ResourceType_Patient) ||
              request.IsRetrieveMetadata(ResourceType_Patient))
          {
            return ResourceType_Patient;
          }
          else
          {
            return ResourceType_Study;
          }

        case ResourceType_Series:
          if (request.IsRetrieveMainDicomTags(ResourceType_Patient) ||
              request.IsRetrieveMetadata(ResourceType_Patient))
          {
            return ResourceType_Patient;
          }
          else if (request.IsRetrieveMainDicomTags(ResourceType_Study) ||
                   request.IsRetrieveMetadata(ResourceType_Study))
          {
            return ResourceType_Study;
          }
          else
          {
            return ResourceType_Series;
          }

        case ResourceType_Instance:
          if (request.IsRetrieveMainDicomTags(ResourceType_Patient) ||
              request.IsRetrieveMetadata(ResourceType_Patient))
          {
            return ResourceType_Patient;
          }
          else if (request.IsRetrieveMainDicomTags(ResourceType_Study) ||
                   request.IsRetrieveMetadata(ResourceType_Study))
          {
            return ResourceType_Study;
          }
          else if (request.IsRetrieveMainDicomTags(ResourceType_Series) ||
                   request.IsRetrieveMetadata(ResourceType_Series))
          {
            return ResourceType_Series;
          }
          else
          {
            return ResourceType_Instance;
          }

        default:
          throw OrthancException(ErrorCode_ParameterOutOfRange);
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

      {
        int64_t currentId = internalId;
        ResourceType currentLevel = level;
        const ResourceType topLevel = GetTopLevelOfInterest(request);

        for (;;)
        {
          if (request.IsRetrieveMainDicomTags(currentLevel))
          {
            RetrieveMainDicomTags(*resource, currentLevel, currentId);
          }

          if (request.IsRetrieveMetadata(currentLevel))
          {
            transaction_.GetAllMetadata(resource->GetMetadata(currentLevel), currentId);
          }

          if (currentLevel == topLevel)
          {
            break;
          }
          else
          {
            int64_t parentId;
            if (transaction_.LookupParent(parentId, currentId))
            {
              currentId = parentId;
              currentLevel = GetParentResourceType(currentLevel);
            }
            else
            {
              throw OrthancException(ErrorCode_DatabasePlugin);
            }
          }
        }
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
          resource->AddChildIdentifier(*it);
        }
      }

      for (std::set<MetadataType>::const_iterator it = request.GetRetrieveChildrenMetadata().begin();
           it != request.GetRetrieveChildrenMetadata().end(); ++it)
      {
        std::list<std::string> values;
        transaction_.GetChildrenMetadata(values, internalId, *it);
        resource->AddChildrenMetadata(*it, values);
      }

      if (!request.GetRetrieveAttachmentOfOneInstance().empty())
      {
        std::set<FileContentType> todo = request.GetRetrieveAttachmentOfOneInstance();
        std::stack< std::pair<ResourceType, int64_t> > candidates;
        candidates.push(std::make_pair(level, internalId));

        while (!todo.empty() &&
               !candidates.empty())
        {
          std::pair<ResourceType, int64_t> top = candidates.top();
          candidates.pop();

          if (top.first == ResourceType_Instance)
          {
            std::set<FileContentType> nextTodo;

            for (std::set<FileContentType>::const_iterator it = todo.begin(); it != todo.end(); ++it)
            {
              FileInfo attachment;
              int64_t revision;
              if (transaction_.LookupAttachment(attachment, revision, top.second, *it))
              {
                resource->AddAttachmentOfOneInstance(attachment);
              }
              else
              {
                nextTodo.insert(*it);
              }
            }

            todo = nextTodo;
          }
          else
          {
            std::list<int64_t> children;
            transaction_.GetChildrenInternalId(children, top.second);
            for (std::list<int64_t>::const_iterator it = children.begin(); it != children.end(); ++it)
            {
              candidates.push(std::make_pair(GetChildResourceType(top.first), *it));
            }
          }
        }
      }

      response.Add(resource.release());
    }
  }
}
