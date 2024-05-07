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


#pragma once

#include "../../../OrthancFramework/Sources/DicomFormat/DicomTag.h"
#include "../Search/DatabaseConstraint.h"
#include "../Search/DicomTagConstraint.h"
#include "../Search/ISqlLookupFormatter.h"
#include "../ServerEnumerations.h"
#include "OrthancIdentifiers.h"

#include <deque>
#include <map>
#include <set>
#include <cassert>
#include <boost/shared_ptr.hpp>

namespace Orthanc
{
  class FindRequest : public boost::noncopyable
  {
  public:
    /**

       TO DISCUSS:

       (1) ResponseContent_ChildInstanceId       = (1 << 6),     // When you need to access all tags from a patient/study/series, you might need to open the DICOM file of a child instance

       if (requestedTags.size() > 0 && resourceType != ResourceType_Instance) // if we are requesting specific tags that might be outside of the MainDicomTags, we must get a childInstanceId too
       {
       responseContent = static_cast<FindRequest::ResponseContent>(responseContent | FindRequest::ResponseContent_ChildInstanceId);
       }


       (2) ResponseContent_IsStable              = (1 << 8),     // This is currently not saved in DB but it could be in the future.

     **/


    enum KeyType  // used for ordering and filters
    {
      KeyType_DicomTag,
      KeyType_Metadata
    };


    enum OrderingDirection
    {
      OrderingDirection_Ascending,
      OrderingDirection_Descending
    };


    class Key
    {
    private:
      KeyType       type_;
      DicomTag      dicomTag_;
      MetadataType  metadata_;
      
      // TODO-FIND: to execute the query, we actually need:
      // ResourceType level_;
      // DicomTagType dicomTagType_;
      // these are however only populated in StatelessDatabaseOperations -> we had to add the normalized lookup arg to ExecuteFind

    public:
      explicit Key(const DicomTag& dicomTag) :
        type_(KeyType_DicomTag),
        dicomTag_(dicomTag),
        metadata_(MetadataType_EndUser)
      {
      }

      explicit Key(MetadataType metadata) :
        type_(KeyType_Metadata),
        dicomTag_(0, 0),
        metadata_(metadata)
      {
      }

      KeyType GetType() const
      {
        return type_;
      }

      const DicomTag& GetDicomTag() const
      {
        assert(GetType() == KeyType_DicomTag);
        return dicomTag_;
      }

      MetadataType GetMetadataType() const
      {
        assert(GetType() == KeyType_Metadata);
        return metadata_;
      }
    };

    class Ordering : public boost::noncopyable
    {
    private:
      OrderingDirection   direction_;
      Key                 key_;

    public:
      Ordering(const Key& key,
               OrderingDirection direction) :
        direction_(direction),
        key_(key)
      {
      }

      KeyType GetKeyType() const
      {
        return key_.GetType();
      }

      OrderingDirection GetDirection() const
      {
        return direction_;
      }

      MetadataType GetMetadataType() const
      {
        return key_.GetMetadataType();
      }

      DicomTag GetDicomTag() const
      {
        return key_.GetDicomTag();
      }
    };


  private:

    // filter & ordering fields
    ResourceType                         level_;                // The level of the response (the filtering on tags, labels and metadata also happens at this level)
    OrthancIdentifiers                   orthancIdentifiers_;   // The response must belong to this Orthanc resources hierarchy
    std::vector<DicomTagConstraint>      dicomTagConstraints_;  // All tags filters (note: the order is not important)
    std::deque<void*>   /* TODO-FIND */       metadataConstraints_;  // All metadata filters (note: the order is not important)
    bool                                 hasLimits_;
    uint64_t                             limitsSince_;
    uint64_t                             limitsCount_;
    std::set<std::string>                labels_;
    LabelsConstraint                     labelsContraint_;
    std::deque<Ordering*>                ordering_;             // The ordering criteria (note: the order is important !)

    bool                                 retrieveMainDicomTags_;
    bool                                 retrieveMetadata_;
    bool                                 retrieveLabels_;
    bool                                 retrieveAttachments_;
    bool                                 retrieveParentIdentifier_;
    bool                                 retrieveChildrenIdentifiers_;
    std::set<MetadataType>               retrieveChildrenMetadata_;

  public:
    explicit FindRequest(ResourceType level);

    ~FindRequest();

    ResourceType GetLevel() const
    {
      return level_;
    }

    void SetOrthancPatientId(const std::string& id)
    {
      orthancIdentifiers_.SetPatientId(id);
    }

    void SetOrthancStudyId(const std::string& id)
    {
      orthancIdentifiers_.SetStudyId(id);
    }

    void SetOrthancSeriesId(const std::string& id)
    {
      orthancIdentifiers_.SetSeriesId(id);
    }

    void SetOrthancInstanceId(const std::string& id)
    {
      orthancIdentifiers_.SetInstanceId(id);
    }

    const OrthancIdentifiers& GetOrthancIdentifiers() const
    {
      return orthancIdentifiers_;
    }

    void AddDicomTagConstraint(const DicomTagConstraint& constraint);

    size_t GetDicomTagConstraintsCount() const
    {
      return dicomTagConstraints_.size();
    }

    size_t GetMetadataConstraintsCount() const
    {
      return metadataConstraints_.size();
    }

    const DicomTagConstraint& GetDicomTagConstraint(size_t index) const;

    void SetLimits(uint64_t since,
                   uint64_t count);

    bool HasLimits() const
    {
      return hasLimits_;
    }

    uint64_t GetLimitsSince() const;

    uint64_t GetLimitsCount() const;

    void AddOrdering(const DicomTag& tag, OrderingDirection direction);

    void AddOrdering(MetadataType metadataType, OrderingDirection direction);

    const std::deque<Ordering*>& GetOrdering() const
    {
      return ordering_;
    }

    void AddLabel(const std::string& label)
    {
      labels_.insert(label);
    }

    const std::set<std::string>& GetLabels() const
    {
      return labels_;
    }

    LabelsConstraint GetLabelsConstraint() const
    {
      return labelsContraint_;
    }

    void SetRetrieveMetadata(bool retrieve)
    {
      retrieveMetadata_ = retrieve;
    }

    bool IsRetrieveMainDicomTags() const
    {
      return retrieveMainDicomTags_;
    }

    void SetRetrieveMainDicomTags(bool retrieve)
    {
      retrieveMainDicomTags_ = retrieve;
    }

    bool IsRetrieveMetadata() const
    {
      return retrieveMetadata_;
    }

    void SetRetrieveLabels(bool retrieve)
    {
      retrieveLabels_ = retrieve;
    }

    bool IsRetrieveLabels() const
    {
      return retrieveLabels_;
    }

    void SetRetrieveAttachments(bool retrieve)
    {
      retrieveAttachments_ = retrieve;
    }

    bool IsRetrieveAttachments() const
    {
      return retrieveAttachments_;
    }

    void SetRetrieveParentIdentifier(bool retrieve);

    bool IsRetrieveParentIdentifier() const
    {
      return retrieveParentIdentifier_;
    }

    void SetRetrieveChildrenIdentifiers(bool retrieve);

    bool IsRetrieveChildrenIdentifiers() const
    {
      return retrieveChildrenIdentifiers_;
    }

    void AddRetrieveChildrenMetadata(MetadataType metadata);

    bool IsRetrieveChildrenMetadata(MetadataType metadata) const
    {
      return retrieveChildrenMetadata_.find(metadata) != retrieveChildrenMetadata_.end();
    }

    const std::set<MetadataType>& GetRetrieveChildrenMetadata() const
    {
      return retrieveChildrenMetadata_;
    }
  };
}
