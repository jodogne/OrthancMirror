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

#include "../../../OrthancFramework/Sources/DicomFormat/DicomTag.h"
#include "../Search/DatabaseDicomTagConstraints.h"
#include "../Search/DatabaseMetadataConstraint.h"
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
  class MainDicomTagsRegistry;

  class FindRequest : public boost::noncopyable
  {
  public:
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

    enum OrderingCast
    {
      OrderingCast_String,
      OrderingCast_Int,
      OrderingCast_Float
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
      OrderingCast        cast_;
      Key                 key_;

    public:
      Ordering(const Key& key,
               OrderingCast cast,
               OrderingDirection direction) :
        direction_(direction),
        cast_(cast),
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

      OrderingCast GetCast() const
      {
        return cast_;
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


    class ParentSpecification : public boost::noncopyable
    {
    private:
      bool  mainDicomTags_;
      bool  metadata_;

    public:
      ParentSpecification() :
        mainDicomTags_(false),
        metadata_(false)
      {
      }

      void SetRetrieveMainDicomTags(bool retrieve)
      {
        mainDicomTags_ = retrieve;
      }

      bool IsRetrieveMainDicomTags() const
      {
        return mainDicomTags_;
      }

      void SetRetrieveMetadata(bool retrieve)
      {
        metadata_ = retrieve;
      }

      bool IsRetrieveMetadata() const
      {
        return metadata_;
      }

      bool IsOfInterest() const
      {
        return (mainDicomTags_ || metadata_);
      }
    };


    class ChildrenSpecification : public boost::noncopyable
    {
    private:
      bool                    identifiers_;
      std::set<MetadataType>  metadata_;
      std::set<DicomTag>      mainDicomTags_;
      bool                    count_;

    public:
      ChildrenSpecification() :
        identifiers_(false),
        count_(false)
      {
      }

      void SetRetrieveIdentifiers(bool retrieve)
      {
        identifiers_ = retrieve;
      }

      bool IsRetrieveIdentifiers() const
      {
        return identifiers_;
      }

      void SetRetrieveCount(bool retrieve)
      {
        count_ = retrieve;
      }

      bool IsRetrieveCount() const
      {
        return count_;
      }

      void AddMetadata(MetadataType metadata)
      {
        metadata_.insert(metadata);
      }

      const std::set<MetadataType>& GetMetadata() const
      {
        return metadata_;
      }

      void AddMainDicomTag(const DicomTag& tag)
      {
        mainDicomTags_.insert(tag);
      }

      const std::set<DicomTag>& GetMainDicomTags() const
      {
        return mainDicomTags_;
      }

      bool IsOfInterest() const
      {
        return (identifiers_ || !metadata_.empty() || !mainDicomTags_.empty() || count_);
      }
    };


  private:
    // filter & ordering fields
    ResourceType                         level_;                // The level of the response (the filtering on tags, labels and metadata also happens at this level)
    OrthancIdentifiers                   orthancIdentifiers_;   // The response must belong to this Orthanc resources hierarchy
    DatabaseDicomTagConstraints          dicomTagConstraints_;  // All tags filters (note: the order is not important)
    bool                                 hasLimits_;
    uint64_t                             limitsSince_;
    uint64_t                             limitsCount_;
    std::set<std::string>                labels_;
    LabelsConstraint                     labelsConstraint_;

    std::deque<Ordering*>                ordering_;             // The ordering criteria (note: the order is important !)
    std::deque<DatabaseMetadataConstraint*>  metadataConstraints_;  // All metadata filters (note: the order is not important)

    bool                                 retrieveMainDicomTags_;
    bool                                 retrieveMetadata_;
    bool                                 retrieveMetadataRevisions_;
    bool                                 retrieveLabels_;
    bool                                 retrieveAttachments_;
    bool                                 retrieveParentIdentifier_;
    ParentSpecification                  retrieveParentPatient_;
    ParentSpecification                  retrieveParentStudy_;
    ParentSpecification                  retrieveParentSeries_;
    ChildrenSpecification                retrieveChildrenStudies_;
    ChildrenSpecification                retrieveChildrenSeries_;
    ChildrenSpecification                retrieveChildrenInstances_;
    bool                                 retrieveOneInstanceMetadataAndAttachments_;

    std::unique_ptr<MainDicomTagsRegistry>  mainDicomTagsRegistry_;

  public:
    explicit FindRequest(ResourceType level);

    ~FindRequest();

    ResourceType GetLevel() const
    {
      return level_;
    }

    void SetOrthancId(ResourceType level,
                      const std::string& id);

    void SetOrthancPatientId(const std::string& id);

    void SetOrthancStudyId(const std::string& id);

    void SetOrthancSeriesId(const std::string& id);

    void SetOrthancInstanceId(const std::string& id);

    const OrthancIdentifiers& GetOrthancIdentifiers() const
    {
      return orthancIdentifiers_;
    }

    DatabaseDicomTagConstraints& GetDicomTagConstraints()
    {
      return dicomTagConstraints_;
    }

    const DatabaseDicomTagConstraints& GetDicomTagConstraints() const
    {
      return dicomTagConstraints_;
    }

    size_t GetMetadataConstraintsCount() const
    {
      return metadataConstraints_.size();
    }

    void ClearLimits()
    {
      hasLimits_ = false;
    }

    void SetLimits(uint64_t since,
                   uint64_t count);

    bool HasLimits() const
    {
      return hasLimits_;
    }

    uint64_t GetLimitsSince() const;

    uint64_t GetLimitsCount() const;

    void AddOrdering(const DicomTag& tag,
                     OrderingCast cast,
                     OrderingDirection direction);

    void AddOrdering(MetadataType metadataType,
                     OrderingCast cast,
                     OrderingDirection direction);

    const std::deque<Ordering*>& GetOrdering() const
    {
      return ordering_;
    }

    void AddMetadataConstraint(DatabaseMetadataConstraint* constraint);

    const std::deque<DatabaseMetadataConstraint*>& GetMetadataConstraint() const
    {
      return metadataConstraints_;
    }

    void SetLabels(const std::set<std::string>& labels)
    {
      labels_ = labels;
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
      return labelsConstraint_;
    }

    void SetLabelsConstraint(LabelsConstraint constraint)
    {
      labelsConstraint_ = constraint;
    }

    void SetRetrieveMainDicomTags(bool retrieve)
    {
      retrieveMainDicomTags_ = retrieve;
    }

    bool IsRetrieveMainDicomTags() const
    {
      return retrieveMainDicomTags_;
    }

    void SetRetrieveMetadata(bool retrieve)
    {
      retrieveMetadata_ = retrieve;
    }

    bool IsRetrieveMetadata() const
    {
      return retrieveMetadata_;
    }

    void SetRetrieveMetadataRevisions(bool retrieve)
    {
      retrieveMetadataRevisions_ = retrieve;
    }

    bool IsRetrieveMetadataRevisions() const
    {
      return retrieveMetadataRevisions_;
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

    ParentSpecification& GetParentSpecification(ResourceType level);

    const ParentSpecification& GetParentSpecification(ResourceType level) const
    {
      return const_cast<FindRequest&>(*this).GetParentSpecification(level);
    }

    ChildrenSpecification& GetChildrenSpecification(ResourceType level);

    const ChildrenSpecification& GetChildrenSpecification(ResourceType level) const
    {
      return const_cast<FindRequest&>(*this).GetChildrenSpecification(level);
    }

    void SetRetrieveOneInstanceMetadataAndAttachments(bool retrieve);

    bool IsRetrieveOneInstanceMetadataAndAttachments() const;

    bool HasConstraints() const;

    bool IsTrivialFind(std::string& publicId /* out */) const;
  };
}
