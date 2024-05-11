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
    static bool IsRequestWithoutContraint(const FindRequest& request)
    {
      return (request.GetDicomTagConstraintsCount() == 0 &&
              request.GetMetadataConstraintsCount() == 0 &&
              request.GetLabels().empty() &&
              request.GetOrdering().empty());
    }

    static void GetChildren(std::list<int64_t>& target,
                            IDatabaseWrapper::ITransaction& transaction,
                            const std::list<int64_t>& resources)
    {
      target.clear();

      for (std::list<int64_t>::const_iterator it = resources.begin(); it != resources.end(); ++it)
      {
        std::list<int64_t> tmp;
        transaction.GetChildrenInternalId(tmp, *it);
        target.splice(target.begin(), tmp);
      }
    }

    static void GetChildren(std::list<std::string>& target,
                            IDatabaseWrapper::ITransaction& transaction,
                            const std::list<int64_t>& resources)
    {
      target.clear();

      for (std::list<int64_t>::const_iterator it = resources.begin(); it != resources.end(); ++it)
      {
        std::list<std::string> tmp;
        transaction.GetChildrenPublicId(tmp, *it);
        target.splice(target.begin(), tmp);
      }
    }

    static void GetChildrenIdentifiers(std::list<std::string>& children,
                                       IDatabaseWrapper::ITransaction& transaction,
                                       const OrthancIdentifiers& identifiers,
                                       ResourceType topLevel,
                                       ResourceType bottomLevel)
    {
      if (!IsResourceLevelAboveOrEqual(topLevel, bottomLevel) ||
          topLevel == bottomLevel)
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      std::list<int64_t> currentResources;
      ResourceType currentLevel;

      {
        int64_t id;
        if (!transaction.LookupResource(id, currentLevel, identifiers.GetLevel(topLevel)) ||
            currentLevel != topLevel)
        {
          throw OrthancException(ErrorCode_InexistentItem);
        }

        currentResources.push_back(id);
      }

      while (currentLevel != bottomLevel)
      {
        ResourceType nextLevel = GetChildResourceType(currentLevel);
        if (nextLevel == bottomLevel)
        {
          GetChildren(children, transaction, currentResources);
        }
        else
        {
          std::list<int64_t> nextResources;
          GetChildren(nextResources, transaction, currentResources);
          currentResources.swap(nextResources);
        }

        currentLevel = nextLevel;
      }
    }

    void GenericFind::ExecuteFind(std::list<std::string>& identifiers,
                                  const FindRequest& request)
    {
      if (IsRequestWithoutContraint(request) &&
          !request.GetOrthancIdentifiers().HasPatientId() &&
          !request.GetOrthancIdentifiers().HasStudyId() &&
          !request.GetOrthancIdentifiers().HasSeriesId() &&
          !request.GetOrthancIdentifiers().HasInstanceId())
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
      else if (IsRequestWithoutContraint(request) &&
               request.GetLevel() == ResourceType_Patient &&
               request.GetOrthancIdentifiers().HasPatientId() &&
               !request.GetOrthancIdentifiers().HasStudyId() &&
               !request.GetOrthancIdentifiers().HasSeriesId() &&
               !request.GetOrthancIdentifiers().HasInstanceId())
      {
        // TODO-FIND: This is a trivial case for which no transaction is needed
        identifiers.push_back(request.GetOrthancIdentifiers().GetPatientId());
      }
      else if (IsRequestWithoutContraint(request) &&
               request.GetLevel() == ResourceType_Study &&
               !request.GetOrthancIdentifiers().HasPatientId() &&
               request.GetOrthancIdentifiers().HasStudyId() &&
               !request.GetOrthancIdentifiers().HasSeriesId() &&
               !request.GetOrthancIdentifiers().HasInstanceId())
      {
        // TODO-FIND: This is a trivial case for which no transaction is needed
        identifiers.push_back(request.GetOrthancIdentifiers().GetStudyId());
      }
      else if (IsRequestWithoutContraint(request) &&
               request.GetLevel() == ResourceType_Series &&
               !request.GetOrthancIdentifiers().HasPatientId() &&
               !request.GetOrthancIdentifiers().HasStudyId() &&
               request.GetOrthancIdentifiers().HasSeriesId() &&
               !request.GetOrthancIdentifiers().HasInstanceId())
      {
        // TODO-FIND: This is a trivial case for which no transaction is needed
        identifiers.push_back(request.GetOrthancIdentifiers().GetSeriesId());
      }
      else if (IsRequestWithoutContraint(request) &&
               request.GetLevel() == ResourceType_Instance &&
               !request.GetOrthancIdentifiers().HasPatientId() &&
               !request.GetOrthancIdentifiers().HasStudyId() &&
               !request.GetOrthancIdentifiers().HasSeriesId() &&
               request.GetOrthancIdentifiers().HasInstanceId())
      {
        // TODO-FIND: This is a trivial case for which no transaction is needed
        identifiers.push_back(request.GetOrthancIdentifiers().GetInstanceId());
      }
      else if (IsRequestWithoutContraint(request) &&
               (request.GetLevel() == ResourceType_Study ||
                request.GetLevel() == ResourceType_Series ||
                request.GetLevel() == ResourceType_Instance) &&
               request.GetOrthancIdentifiers().HasPatientId() &&
               !request.GetOrthancIdentifiers().HasStudyId() &&
               !request.GetOrthancIdentifiers().HasSeriesId() &&
               !request.GetOrthancIdentifiers().HasInstanceId())
      {
        GetChildrenIdentifiers(identifiers, transaction_, request.GetOrthancIdentifiers(), ResourceType_Patient, request.GetLevel());
      }
      else if (IsRequestWithoutContraint(request) &&
               (request.GetLevel() == ResourceType_Series ||
                request.GetLevel() == ResourceType_Instance) &&
               !request.GetOrthancIdentifiers().HasPatientId() &&
               request.GetOrthancIdentifiers().HasStudyId() &&
               !request.GetOrthancIdentifiers().HasSeriesId() &&
               !request.GetOrthancIdentifiers().HasInstanceId())
      {
        GetChildrenIdentifiers(identifiers, transaction_, request.GetOrthancIdentifiers(), ResourceType_Study, request.GetLevel());
      }
      else if (IsRequestWithoutContraint(request) &&
               request.GetLevel() == ResourceType_Instance &&
               !request.GetOrthancIdentifiers().HasPatientId() &&
               !request.GetOrthancIdentifiers().HasStudyId() &&
               request.GetOrthancIdentifiers().HasSeriesId() &&
               !request.GetOrthancIdentifiers().HasInstanceId())
      {
        GetChildrenIdentifiers(identifiers, transaction_, request.GetOrthancIdentifiers(), ResourceType_Series, request.GetLevel());
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
          if (request.GetParentRetrieveSpecification(ResourceType_Patient).IsRetrieveMainDicomTags() ||
              request.GetParentRetrieveSpecification(ResourceType_Patient).IsRetrieveMetadata())
          {
            return ResourceType_Patient;
          }
          else
          {
            return ResourceType_Study;
          }

        case ResourceType_Series:
          if (request.GetParentRetrieveSpecification(ResourceType_Patient).IsRetrieveMainDicomTags() ||
              request.GetParentRetrieveSpecification(ResourceType_Patient).IsRetrieveMetadata())
          {
            return ResourceType_Patient;
          }
          else if (request.GetParentRetrieveSpecification(ResourceType_Study).IsRetrieveMainDicomTags() ||
                   request.GetParentRetrieveSpecification(ResourceType_Study).IsRetrieveMetadata())
          {
            return ResourceType_Study;
          }
          else
          {
            return ResourceType_Series;
          }

        case ResourceType_Instance:
          if (request.GetParentRetrieveSpecification(ResourceType_Patient).IsRetrieveMainDicomTags() ||
              request.GetParentRetrieveSpecification(ResourceType_Patient).IsRetrieveMetadata())
          {
            return ResourceType_Patient;
          }
          else if (request.GetParentRetrieveSpecification(ResourceType_Study).IsRetrieveMainDicomTags() ||
                   request.GetParentRetrieveSpecification(ResourceType_Study).IsRetrieveMetadata())
          {
            return ResourceType_Study;
          }
          else if (request.GetParentRetrieveSpecification(ResourceType_Series).IsRetrieveMainDicomTags() ||
                   request.GetParentRetrieveSpecification(ResourceType_Series).IsRetrieveMetadata())
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

      std::unique_ptr<FindResponse::Resource> resource(new FindResponse::Resource(request.GetLevel(), internalId, identifier));

      if (request.IsRetrieveParentIdentifier())
      {
        assert(!parent.empty());
        resource->SetParentIdentifier(parent);
      }

      if (request.IsRetrieveMainDicomTags())
      {
        RetrieveMainDicomTags(*resource, level, internalId);
      }

      if (request.IsRetrieveMetadata())
      {
        transaction_.GetAllMetadata(resource->GetMetadata(level), internalId);
      }

      {
        const ResourceType topLevel = GetTopLevelOfInterest(request);

        int64_t currentId = internalId;
        ResourceType currentLevel = level;

        while (currentLevel != topLevel)
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

          if (request.GetParentRetrieveSpecification(currentLevel).IsRetrieveMainDicomTags())
          {
            RetrieveMainDicomTags(*resource, currentLevel, currentId);
          }

          if (request.GetParentRetrieveSpecification(currentLevel).IsRetrieveMetadata())
          {
            transaction_.GetAllMetadata(resource->GetMetadata(currentLevel), currentId);
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

      if (request.GetLevel() != ResourceType_Instance &&
          request.GetChildrenRetrieveSpecification(GetChildResourceType(request.GetLevel())).IsRetrieveIdentifiers())
      {
        // TODO-FIND: Retrieve other levels than immediate children
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

      if (request.IsRetrieveOneInstanceIdentifier())
      {
        int64_t currentId = internalId;
        ResourceType currentLevel = level;

        while (currentLevel != ResourceType_Instance)
        {
          std::list<int64_t> children;
          transaction_.GetChildrenInternalId(children, currentId);
          if (children.empty())
          {
            throw OrthancException(ErrorCode_DatabasePlugin);
          }
          else
          {
            currentId = children.front();
            currentLevel = GetChildResourceType(currentLevel);
          }
        }

        resource->SetOneInstanceIdentifier(transaction_.GetPublicId(currentId));
      }

      response.Add(resource.release());
    }
  }
}
