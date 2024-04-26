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
#include "../ServerEnumerations.h"
#include "OrthancIdentifiers.h"
#include "../Search/DicomTagConstraint.h"
#include "../Search/LabelsConstraint.h"
#include "../Search/DatabaseConstraint.h"

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
    enum ResponseContent
    {
      ResponseContent_MainDicomTags         = (1 << 0),     // retrieve all tags from MainDicomTags and DicomIdentifiers
      ResponseContent_Metadata              = (1 << 1),     // retrieve all metadata, their values and revision
      ResponseContent_Labels                = (1 << 2),     // get all labels
      ResponseContent_Attachments           = (1 << 3),     // retrieve all attachments, their values and revision
      ResponseContent_Parent                = (1 << 4),     // get the id of the parent
      ResponseContent_Children              = (1 << 5),     // retrieve the list of children ids
      ResponseContent_ChildInstanceId       = (1 << 6),     // When you need to access all tags from a patient/study/series, you might need to open the DICOM file of a child instance
      ResponseContent_ChildrenMetadata      = (1 << 7),     // That is actually required to compute the series status but could be usefull for other stuffs.
      ResponseContent_IsStable              = (1 << 8),     // This is currently not saved in DB but it could be in the future.

      ResponseContent_IdentifiersOnly       = 0,
      ResponseContent_INTERNAL              = 0x7FFFFFFF
    };

    enum ConstraintType
    {
      ConstraintType_Mandatory,
      ConstraintType_Equality,
      ConstraintType_Range,
      ConstraintType_Wildcard,
      ConstraintType_List
    };

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
      KeyType                       type_;
      boost::shared_ptr<DicomTag>   dicomTag_;
      MetadataType                  metadata_;
      
      // TODO-FIND: to execute the query, we actually need:
      // ResourceType level_;
      // DicomTagType dicomTagType_;
      // these are however only populated in StatelessDatabaseOperations -> we had to add the normalized lookup arg to ExecuteFind

    public:
      Key(const DicomTag& dicomTag) :
        type_(KeyType_DicomTag),
        dicomTag_(new DicomTag(dicomTag)),
        metadata_(MetadataType_EndUser)
      {
      }

      Key(MetadataType metadata) :
        type_(KeyType_Metadata),
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
        return *dicomTag_;
      }

      MetadataType GetMetadataType() const
      {
        assert(GetType() == KeyType_Metadata);
        return metadata_;
      }
    };

    class Ordering : public boost::noncopyable
    {
      OrderingDirection   direction_;
      Key                 key_;

    public:
      Ordering(const Key& key,
               OrderingDirection direction) :
        direction_(direction),
        key_(key)
      {
      }

    public:
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

    // TODO-FIND: this class hierarchy actually adds complexity and is very redundant with DicomTagConstraint.
    //       e.g, in this class hierarchy, it is difficult to implement an equivalent to DicomTagConstraint::ConvertToDatabaseConstraint
    //       I have the feeling we can just have a MetadataConstraint in the same way as DicomTagConstraint
    //       and both convert to a DatabaseConstraint in StatelessDatabaseOperations
    // class FilterConstraint : public boost::noncopyable
    // {
    //   Key              key_;
    
    // protected:
    //   FilterConstraint(const Key& key) :
    //     key_(key)
    //   {
    //   }

    // public:
    //   virtual ~FilterConstraint()
    //   {
    //   }

    //   const Key& GetKey() const
    //   {
    //     return key_;
    //   }

    //   virtual ConstraintType GetType() const = 0;
    //   virtual bool IsCaseSensitive() const = 0;  // Needed for PN VR


    // };


    // class MandatoryConstraint : public FilterConstraint
    // {
    // public:
    //   virtual ConstraintType GetType() const ORTHANC_OVERRIDE
    //   {
    //     return ConstraintType_Mandatory;
    //   }
    // };


    // class StringConstraint : public FilterConstraint
    // {
    // private:
    //   bool  caseSensitive_;

    // public:
    //   StringConstraint(Key key,
    //                    bool caseSensitive) :
    //     FilterConstraint(key),
    //     caseSensitive_(caseSensitive)
    //   {
    //   }

    //   bool IsCaseSensitive() const
    //   {
    //     return caseSensitive_;
    //   }
    // };


    // class EqualityConstraint : public StringConstraint
    // {
    // private:
    //   std::string  value_;

    // public:
    //   explicit EqualityConstraint(Key key,
    //                               bool caseSensitive,
    //                               const std::string& value) :
    //     StringConstraint(key, caseSensitive),
    //     value_(value)
    //   {
    //   }

    //   virtual ConstraintType GetType() const ORTHANC_OVERRIDE
    //   {
    //     return ConstraintType_Equality;
    //   }

    //   const std::string& GetValue() const
    //   {
    //     return value_;
    //   }
    // };


    // class RangeConstraint : public StringConstraint
    // {
    // private:
    //   std::string  start_;
    //   std::string  end_;    // Inclusive

    // public:
    //   RangeConstraint(Key key,
    //                   bool caseSensitive,
    //                   const std::string& start,
    //                   const std::string& end) :
    //     StringConstraint(key, caseSensitive),
    //     start_(start),
    //     end_(end)
    //   {
    //   }

    //   virtual ConstraintType GetType() const ORTHANC_OVERRIDE
    //   {
    //     return ConstraintType_Range;
    //   }

    //   const std::string& GetStart() const
    //   {
    //     return start_;
    //   }

    //   const std::string& GetEnd() const
    //   {
    //     return end_;
    //   }
    // };


    // class WildcardConstraint : public StringConstraint
    // {
    // private:
    //   std::string  value_;

    // public:
    //   explicit WildcardConstraint(Key& key,
    //                               bool caseSensitive,
    //                               const std::string& value) :
    //     StringConstraint(key, caseSensitive),
    //     value_(value)
    //   {
    //   }

    //   virtual ConstraintType GetType() const ORTHANC_OVERRIDE
    //   {
    //     return ConstraintType_Wildcard;
    //   }

    //   const std::string& GetValue() const
    //   {
    //     return value_;
    //   }
    // };


    // class ListConstraint : public StringConstraint
    // {
    // private:
    //   std::set<std::string>  values_;

    // public:
    //   ListConstraint(Key key,
    //                  bool caseSensitive) :
    //     StringConstraint(key, caseSensitive)
    //   {
    //   }

    //   virtual ConstraintType GetType() const ORTHANC_OVERRIDE
    //   {
    //     return ConstraintType_List;
    //   }

    //   const std::set<std::string>& GetValues() const
    //   {
    //     return values_;
    //   }
    // };


  private:

    // filter & ordering fields
    ResourceType                         level_;                // The level of the response (the filtering on tags, labels and metadata also happens at this level)
    OrthancIdentifiers                   orthancIdentifiers_;   // The response must belong to this Orthanc resources hierarchy
    // std::deque<FilterConstraint*>        filterConstraints_;    // All tags and metadata filters (note: the order is not important)
    std::vector<DicomTagConstraint>      dicomTagConstraints_;  // All tags filters (note: the order is not important)
    std::deque<void*>   /* TODO-FIND */       metadataConstraints_;  // All metadata filters (note: the order is not important)
    bool                                 hasLimits_;
    uint64_t                             limitsSince_;
    uint64_t                             limitsCount_;
    std::set<std::string>                labels_;
    LabelsConstraint                     labelsContraint_;
    std::deque<Ordering*>                ordering_;             // The ordering criteria (note: the order is important !)

    // response fields
    ResponseContent                      responseContent_;
    
    // TODO: check if these 4 options are required.  We might just have a retrieveParentTags that could be part of the ResponseContent enum ?
    bool                                 retrievePatientTags_;
    bool                                 retrieveStudyTags_;
    bool                                 retrieveSeriesTags_;
    bool                                 retrieveInstanceTags_;

    bool IsCompatibleLevel(ResourceType levelOfInterest) const;

  public:
    FindRequest(ResourceType level);

    ~FindRequest();

    ResourceType GetLevel() const
    {
      return level_;
    }


    void SetResponseContent(ResponseContent content)
    {
      responseContent_ = content;
    }

    void AddResponseContent(ResponseContent content)
    {
      responseContent_ = static_cast<ResponseContent>(static_cast<uint32_t>(responseContent_) | content);
    }

    ResponseContent GetResponseContent() const
    {
      return responseContent_;
    }

    bool HasResponseContent(ResponseContent content) const
    {
      return (responseContent_ & content) == content;
    }

    bool IsResponseIdentifiersOnly() const
    {
      return responseContent_ == ResponseContent_IdentifiersOnly;
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


    void SetRetrieveTagsAtLevel(ResourceType levelOfInterest,
                                bool retrieve);

    bool IsRetrieveTagsAtLevel(ResourceType levelOfInterest) const;

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
  };
}
