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


#pragma once

#include "Database/FindRequest.h"
#include "Database/FindResponse.h"

namespace Orthanc
{
  class DatabaseLookup;
  class ServerContext;
  class ServerIndex;

  class ResourceFinder : public boost::noncopyable
  {
  public:
    class IVisitor : public boost::noncopyable
    {
    public:
      virtual ~IVisitor()
      {
      }

      virtual void Apply(const FindResponse::Resource& resource,
                         const DicomMap& requestedTags) = 0;

      virtual void MarkAsComplete() = 0;
    };

  private:
    enum PagingMode
    {
      PagingMode_FullDatabase,
      PagingMode_FullManual,
      PagingMode_ManualSkip
    };

    FindRequest                      request_;
    uint64_t                         databaseLimits_;
    std::unique_ptr<DatabaseLookup>  lookup_;
    bool                             isSimpleLookup_;
    bool                             canBeFullyPerformedInDb_;
    PagingMode                       pagingMode_;
    bool                             hasLimitsSince_;
    bool                             hasLimitsCount_;
    uint64_t                         limitsSince_;
    uint64_t                         limitsCount_;
    ResponseContentFlags             responseContent_;
    FindStorageAccessMode            storageAccessMode_;
    bool                             supportsChildExistQueries_;
    std::set<DicomTag>               requestedTags_;
    std::set<DicomTag>               requestedComputedTags_;

    bool                             isWarning002Enabled_;
    bool                             isWarning004Enabled_;
    bool                             isWarning005Enabled_;

    bool IsRequestedComputedTag(const DicomTag& tag) const
    {
      return requestedComputedTags_.find(tag) != requestedComputedTags_.end();
    }

    void ConfigureChildrenCountComputedTag(DicomTag tag,
                                           ResourceType parentLevel,
                                           ResourceType childLevel);

    void InjectChildrenCountComputedTag(DicomMap& requestedTags,
                                        DicomTag tag,
                                        const FindResponse::Resource& resource,
                                        ResourceType level) const;

    static SeriesStatus GetSeriesStatus(uint32_t& expectedNumberOfInstances,
                                        const FindResponse::Resource& resource);

    void InjectComputedTags(DicomMap& requestedTags,
                            const FindResponse::Resource& resource) const;

    void UpdateRequestLimits(ServerContext& context);

    bool HasRequestedTags() const
    {
      return requestedTags_.size() > 0;
    }

    bool IsStorageAccessAllowed();

  public:
    ResourceFinder(ResourceType level,
                   ResponseContentFlags responseContent,
                   FindStorageAccessMode storageAccessMode,
                   bool supportsChildExistQueries);

    void SetDatabaseLimits(uint64_t limits);

    void SetOrthancId(ResourceType level,
                      const std::string& id)
    {
      request_.SetOrthancId(level, id);
    }

    void SetLimitsSince(uint64_t since);

    void SetLimitsCount(uint64_t count);

    void SetDatabaseLookup(const DatabaseLookup& lookup);

    void AddRequestedTag(const DicomTag& tag);

    void AddRequestedTags(const std::set<DicomTag>& tags);

    void AddOrdering(const DicomTag& tag,
                     FindRequest::OrderingCast cast,
                     FindRequest::OrderingDirection direction)
    {
      request_.AddOrdering(tag, cast, direction);
    }

    void AddOrdering(MetadataType metadataType,
                     FindRequest::OrderingCast cast,
                     FindRequest::OrderingDirection direction)
    {
      request_.AddOrdering(metadataType, cast, direction);
    }

    void AddMetadataConstraint(DatabaseMetadataConstraint* constraint)
    {
      request_.AddMetadataConstraint(constraint);
    }

    void SetLabels(const std::set<std::string>& labels)
    {
      request_.SetLabels(labels);
    }

    void AddLabel(const std::string& label)
    {
      request_.AddLabel(label);
    }

    void SetLabelsConstraint(LabelsConstraint constraint)
    {
      request_.SetLabelsConstraint(constraint);
    }

    void SetRetrieveOneInstanceMetadataAndAttachments(bool retrieve)
    {
      request_.SetRetrieveOneInstanceMetadataAndAttachments(retrieve);
    }

    void SetRetrieveMetadata(bool retrieve)
    {
      request_.SetRetrieveMetadata(retrieve);
    }

    void SetRetrieveAttachments(bool retrieve)
    {
      request_.SetRetrieveAttachments(retrieve);
    }

    // NB: "index" is only used in this method to fill the "IsStable" information
    void Expand(Json::Value& target,
                const FindResponse::Resource& resource,
                ServerIndex& index,
                DicomToJsonFormat format) const;

    void Execute(IVisitor& visitor,
                 ServerContext& context);

    void Execute(Json::Value& target,
                 ServerContext& context,
                 DicomToJsonFormat format,
                 bool includeAllMetadata);

    bool ExecuteOneResource(Json::Value& target,
                            ServerContext& context,
                            DicomToJsonFormat format,
                            bool includeAllMetadata);

    uint64_t Count(ServerContext& context) const;

    bool CanBeFullyPerformedInDb() const
    {
      return canBeFullyPerformedInDb_;
    }
  };
}
