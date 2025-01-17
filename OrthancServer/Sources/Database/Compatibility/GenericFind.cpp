/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2025 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2025 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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
                            IDatabaseWrapper::ICompatibilityTransaction& transaction,
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
                                       IDatabaseWrapper::ICompatibilityTransaction& compatibilityTransaction,
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
          GetChildren(children, compatibilityTransaction, currentResources);
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
                                  const IDatabaseWrapper::Capabilities& capabilities,
                                  const FindRequest& request)
    {
      if (!request.GetLabels().empty() &&
          !capabilities.HasLabelsSupport())
      {
        throw OrthancException(ErrorCode_NotImplemented, "The database backend doesn't support labels");
      }

      if (!request.GetOrdering().empty())
      {
        throw OrthancException(ErrorCode_NotImplemented, "The database backend doesn't support ordering");
      }

      if (!request.HasConstraints() &&
          !request.GetOrthancIdentifiers().HasPatientId() &&
          !request.GetOrthancIdentifiers().HasStudyId() &&
          !request.GetOrthancIdentifiers().HasSeriesId() &&
          !request.GetOrthancIdentifiers().HasInstanceId())
      {
        if (!request.HasLimits())
        {
          transaction_.GetAllPublicIds(identifiers, request.GetLevel());
        }
        else if (request.GetLimitsCount() != 0)
        {
          compatibilityTransaction_.GetAllPublicIdsCompatibility(identifiers, request.GetLevel(), request.GetLimitsSince(), request.GetLimitsCount());
        }
        else
        {
          // Starting with Orthanc 1.12.5, "limit=0" means "no limit"
          std::list<std::string> tmp;
          transaction_.GetAllPublicIds(tmp, request.GetLevel());

          size_t count = 0;
          for (std::list<std::string>::const_iterator it = tmp.begin(); it != tmp.end(); ++it)
          {
            if (count >= request.GetLimitsSince())
            {
              identifiers.push_back(*it);
            }

            count++;
          }
        }
      }
      else if (!request.HasConstraints() &&
               (request.GetLevel() == ResourceType_Study ||
                request.GetLevel() == ResourceType_Series ||
                request.GetLevel() == ResourceType_Instance) &&
               request.GetOrthancIdentifiers().HasPatientId() &&
               !request.GetOrthancIdentifiers().HasStudyId() &&
               !request.GetOrthancIdentifiers().HasSeriesId() &&
               !request.GetOrthancIdentifiers().HasInstanceId())
      {
        GetChildrenIdentifiers(identifiers, transaction_, compatibilityTransaction_,
                               request.GetOrthancIdentifiers(), ResourceType_Patient, request.GetLevel());
      }
      else if (!request.HasConstraints() &&
               (request.GetLevel() == ResourceType_Series ||
                request.GetLevel() == ResourceType_Instance) &&
               !request.GetOrthancIdentifiers().HasPatientId() &&
               request.GetOrthancIdentifiers().HasStudyId() &&
               !request.GetOrthancIdentifiers().HasSeriesId() &&
               !request.GetOrthancIdentifiers().HasInstanceId())
      {
        GetChildrenIdentifiers(identifiers, transaction_, compatibilityTransaction_,
                               request.GetOrthancIdentifiers(), ResourceType_Study, request.GetLevel());
      }
      else if (!request.HasConstraints() &&
               request.GetLevel() == ResourceType_Instance &&
               !request.GetOrthancIdentifiers().HasPatientId() &&
               !request.GetOrthancIdentifiers().HasStudyId() &&
               request.GetOrthancIdentifiers().HasSeriesId() &&
               !request.GetOrthancIdentifiers().HasInstanceId())
      {
        GetChildrenIdentifiers(identifiers, transaction_, compatibilityTransaction_,
                               request.GetOrthancIdentifiers(), ResourceType_Series, request.GetLevel());
      }
      else if (request.GetMetadataConstraintsCount() == 0 &&
               request.GetOrdering().empty() &&
               !request.GetOrthancIdentifiers().HasPatientId() &&
               !request.GetOrthancIdentifiers().HasStudyId() &&
               !request.GetOrthancIdentifiers().HasSeriesId() &&
               !request.GetOrthancIdentifiers().HasInstanceId())
      {
        compatibilityTransaction_.ApplyLookupResources(identifiers, NULL /* TODO-FIND: Could the "instancesId" information be exploited? */,
                                                       request.GetDicomTagConstraints(), request.GetLevel(), request.GetLabels(),
                                                       request.GetLabelsConstraint(), request.HasLimits() ? request.GetLimitsCount() : 0);
      }
      else
      {
        throw OrthancException(ErrorCode_NotImplemented);
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
          if (request.GetParentSpecification(ResourceType_Patient).IsOfInterest())
          {
            return ResourceType_Patient;
          }
          else
          {
            return ResourceType_Study;
          }

        case ResourceType_Series:
          if (request.GetParentSpecification(ResourceType_Patient).IsOfInterest())
          {
            return ResourceType_Patient;
          }
          else if (request.GetParentSpecification(ResourceType_Study).IsOfInterest())
          {
            return ResourceType_Study;
          }
          else
          {
            return ResourceType_Series;
          }

        case ResourceType_Instance:
          if (request.GetParentSpecification(ResourceType_Patient).IsOfInterest())
          {
            return ResourceType_Patient;
          }
          else if (request.GetParentSpecification(ResourceType_Study).IsOfInterest())
          {
            return ResourceType_Study;
          }
          else if (request.GetParentSpecification(ResourceType_Series).IsOfInterest())
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


    static ResourceType GetBottomLevelOfInterest(const FindRequest& request)
    {
      switch (request.GetLevel())
      {
        case ResourceType_Patient:
          if (request.GetChildrenSpecification(ResourceType_Instance).IsOfInterest())
          {
            return ResourceType_Instance;
          }
          else if (request.GetChildrenSpecification(ResourceType_Series).IsOfInterest())
          {
            return ResourceType_Series;
          }
          else if (request.GetChildrenSpecification(ResourceType_Study).IsOfInterest())
          {
            return ResourceType_Study;
          }
          else
          {
            return ResourceType_Patient;
          }

        case ResourceType_Study:
          if (request.GetChildrenSpecification(ResourceType_Instance).IsOfInterest())
          {
            return ResourceType_Instance;
          }
          else if (request.GetChildrenSpecification(ResourceType_Series).IsOfInterest())
          {
            return ResourceType_Series;
          }
          else
          {
            return ResourceType_Study;
          }

        case ResourceType_Series:
          if (request.GetChildrenSpecification(ResourceType_Instance).IsOfInterest())
          {
            return ResourceType_Instance;
          }
          else
          {
            return ResourceType_Series;
          }

        case ResourceType_Instance:
          return ResourceType_Instance;

        default:
          throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
    }


    void GenericFind::ExecuteExpand(FindResponse& response,
                                    const IDatabaseWrapper::Capabilities& capabilities,
                                    const FindRequest& request,
                                    const std::string& identifier)
    {
      int64_t internalId;
      ResourceType level;
      std::string parent;

      if (request.IsRetrieveParentIdentifier())
      {
        if (!compatibilityTransaction_.LookupResourceAndParent(internalId, level, parent, identifier))
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
        throw OrthancException(ErrorCode_UnknownResource, "Wrong resource level for this ID");  // this might happen e.g if you call /instances/... with a series instance id
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
        std::map<MetadataType, std::string> metadata;
        transaction_.GetAllMetadata(metadata, internalId);

        for (std::map<MetadataType, std::string>::const_iterator
               it = metadata.begin(); it != metadata.end(); ++it)
        {
          if (request.IsRetrieveMetadataRevisions() &&
              capabilities.HasRevisionsSupport())
          {
            std::string value;
            int64_t revision;
            if (transaction_.LookupMetadata(value, revision, internalId, it->first) &&
                value == it->second)
            {
              resource->AddMetadata(level, it->first, it->second, revision);
            }
            else
            {
              throw OrthancException(ErrorCode_DatabasePlugin);
            }
          }
          else
          {
            resource->AddMetadata(level, it->first, it->second, 0 /* revision not requested */);
          }
        }
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

          if (request.GetParentSpecification(currentLevel).IsRetrieveMainDicomTags())
          {
            RetrieveMainDicomTags(*resource, currentLevel, currentId);
          }

          if (request.GetParentSpecification(currentLevel).IsRetrieveMetadata())
          {
            std::map<MetadataType, std::string> metadata;
            transaction_.GetAllMetadata(metadata, currentId);

            for (std::map<MetadataType, std::string>::const_iterator
                   it = metadata.begin(); it != metadata.end(); ++it)
            {
              resource->AddMetadata(currentLevel, it->first, it->second, 0 /* revision not request */);
            }
          }
        }
      }

      if (capabilities.HasLabelsSupport() &&
          request.IsRetrieveLabels())
      {
        compatibilityTransaction_.ListLabels(resource->GetLabels(), internalId);
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
            resource->AddAttachment(info, revision);
          }
          else
          {
            throw OrthancException(ErrorCode_DatabasePlugin);
          }
        }
      }

      {
        const ResourceType bottomLevel = GetBottomLevelOfInterest(request);

        std::list<int64_t> currentIds;
        currentIds.push_back(internalId);

        ResourceType currentLevel = level;

        while (currentLevel != bottomLevel)
        {
          ResourceType childrenLevel = GetChildResourceType(currentLevel);

          if (request.GetChildrenSpecification(childrenLevel).IsRetrieveIdentifiers() || 
              request.GetChildrenSpecification(childrenLevel).IsRetrieveCount())
          {
            for (std::list<int64_t>::const_iterator it = currentIds.begin(); it != currentIds.end(); ++it)
            {
              std::list<std::string> ids;
              compatibilityTransaction_.GetChildrenPublicId(ids, *it);

              for (std::list<std::string>::const_iterator it2 = ids.begin(); it2 != ids.end(); ++it2)
              {
                resource->AddChildIdentifier(childrenLevel, *it2);
              }
              resource->IncrementChildrenCount(childrenLevel, ids.size());
            }
          }

          const std::set<MetadataType>& metadata = request.GetChildrenSpecification(childrenLevel).GetMetadata();

          for (std::set<MetadataType>::const_iterator it = metadata.begin(); it != metadata.end(); ++it)
          {
            for (std::list<int64_t>::const_iterator it2 = currentIds.begin(); it2 != currentIds.end(); ++it2)
            {
              std::list<std::string> values;
              transaction_.GetChildrenMetadata(values, *it2, *it);

              for (std::list<std::string>::const_iterator it3 = values.begin(); it3 != values.end(); ++it3)
              {
                resource->AddChildrenMetadataValue(childrenLevel, *it, *it3);
              }
            }
          }

          const std::set<DicomTag>& mainDicomTags = request.GetChildrenSpecification(childrenLevel).GetMainDicomTags();

          if (childrenLevel != bottomLevel ||
              !mainDicomTags.empty())
          {
            std::list<int64_t> childrenIds;

            for (std::list<int64_t>::const_iterator it = currentIds.begin(); it != currentIds.end(); ++it)
            {
              std::list<int64_t> tmp;
              transaction_.GetChildrenInternalId(tmp, *it);

              childrenIds.splice(childrenIds.end(), tmp);
            }

            if (!mainDicomTags.empty())
            {
              for (std::list<int64_t>::const_iterator it = childrenIds.begin(); it != childrenIds.end(); ++it)
              {
                DicomMap m;
                transaction_.GetMainDicomTags(m, *it);

                for (std::set<DicomTag>::const_iterator it2 = mainDicomTags.begin(); it2 != mainDicomTags.end(); ++it2)
                {
                  std::string value;
                  if (m.LookupStringValue(value, *it2, false /* no binary allowed */))
                  {
                    resource->AddChildrenMainDicomTagValue(childrenLevel, *it2, value);
                  }
                }
              }
            }

            currentIds = childrenIds;
          }
          else
          {
            currentIds.clear();
          }

          currentLevel = childrenLevel;
        }
      }

      if (request.GetLevel() != ResourceType_Instance &&
          request.IsRetrieveOneInstanceMetadataAndAttachments())
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

        std::map<MetadataType, std::string> metadata;
        transaction_.GetAllMetadata(metadata, currentId);

        std::set<FileContentType> attachmentsType;
        transaction_.ListAvailableAttachments(attachmentsType, currentId);

        std::map<FileContentType, FileInfo> attachments;
        for (std::set<FileContentType>::const_iterator it = attachmentsType.begin(); it != attachmentsType.end(); ++it)
        {
          FileInfo info;
          int64_t revision;  // Unused in this case
          if (transaction_.LookupAttachment(info, revision, currentId, *it))
          {
            attachments[*it] = info;
          }
          else
          {
            throw OrthancException(ErrorCode_DatabasePlugin);
          }
        }

        resource->SetOneInstanceMetadataAndAttachments(transaction_.GetPublicId(currentId), metadata, attachments);
      }

      response.Add(resource.release());
    }
  }
}
