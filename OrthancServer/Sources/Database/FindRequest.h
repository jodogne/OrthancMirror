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

#include <deque>
#include <map>
#include <set>


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
      ResponseContent_IsStable              = (1 << 7),     // This is currently not saved in DB but it could be in the future.

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


    enum Ordering
    {
      Ordering_Ascending,
      Ordering_Descending,
      Ordering_None
    };


    class TagConstraint : public boost::noncopyable
    {
    private:
      DicomTag  tag_;

    public:
      TagConstraint(DicomTag tag) :
        tag_(tag)
      {
      }

      virtual ~TagConstraint()
      {
      }

      virtual DicomTag GetTag() const
      {
        return tag_;
      }

      virtual ConstraintType GetType() const = 0;

      virtual bool IsCaseSensitive() const = 0;  // Needed for PN VR
    };


    class MandatoryConstraint : public TagConstraint
    {
    public:
      virtual ConstraintType GetType() const ORTHANC_OVERRIDE
      {
        return ConstraintType_Mandatory;
      }
    };


    class StringConstraint : public TagConstraint
    {
    private:
      bool  caseSensitive_;

    public:
      StringConstraint(DicomTag tag,
                       bool caseSensitive) :
        TagConstraint(tag),
        caseSensitive_(caseSensitive)
      {
      }

      bool IsCaseSensitive() const
      {
        return caseSensitive_;
      }
    };


    class EqualityConstraint : public StringConstraint
    {
    private:
      std::string  value_;

    public:
      explicit EqualityConstraint(DicomTag tag,
                                  bool caseSensitive,
                                  const std::string& value) :
        StringConstraint(tag, caseSensitive),
        value_(value)
      {
      }

      virtual ConstraintType GetType() const ORTHANC_OVERRIDE
      {
        return ConstraintType_Equality;
      }

      const std::string& GetValue() const
      {
        return value_;
      }
    };


    class RangeConstraint : public StringConstraint
    {
    private:
      std::string  start_;
      std::string  end_;    // Inclusive

    public:
      RangeConstraint(DicomTag tag,
                      bool caseSensitive,
                      const std::string& start,
                      const std::string& end) :
        StringConstraint(tag, caseSensitive),
        start_(start),
        end_(end)
      {
      }

      virtual ConstraintType GetType() const ORTHANC_OVERRIDE
      {
        return ConstraintType_Range;
      }

      const std::string& GetStart() const
      {
        return start_;
      }

      const std::string& GetEnd() const
      {
        return end_;
      }
    };


    class WildcardConstraint : public StringConstraint
    {
    private:
      std::string  value_;

    public:
      explicit WildcardConstraint(DicomTag& tag,
                                  bool caseSensitive,
                                  const std::string& value) :
        StringConstraint(tag, caseSensitive),
        value_(value)
      {
      }

      virtual ConstraintType GetType() const ORTHANC_OVERRIDE
      {
        return ConstraintType_Wildcard;
      }

      const std::string& GetValue() const
      {
        return value_;
      }
    };


    class ListConstraint : public StringConstraint
    {
    private:
      std::set<std::string>  values_;

    public:
      ListConstraint(DicomTag tag,
                     bool caseSensitive) :
        StringConstraint(tag, caseSensitive)
      {
      }

      virtual ConstraintType GetType() const ORTHANC_OVERRIDE
      {
        return ConstraintType_List;
      }

      const std::set<std::string>& GetValues() const
      {
        return values_;
      }
    };


  private:
    ResourceType                         level_;
    ResponseContent                      responseContent_;
    OrthancIdentifiers                   orthancIdentifiers_;
    std::deque<TagConstraint*>           tagConstraints_;
    bool                                 hasLimits_;
    uint64_t                             limitsSince_;
    uint64_t                             limitsCount_;
    bool                                 retrievePatientTags_;
    bool                                 retrieveStudyTags_;
    bool                                 retrieveSeriesTags_;
    bool                                 retrieveInstanceTags_;
    std::map<DicomTag, Ordering>         tagOrdering_;
    std::set<std::string>                labels_;
    std::map<MetadataType, std::string>  metadataConstraints_;

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

    void AddTagConstraint(TagConstraint* constraint /* takes ownership */);

    size_t GetTagConstraintsCount() const
    {
      return tagConstraints_.size();
    }

    const TagConstraint& GetTagConstraint(size_t index) const;

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

    void SetTagOrdering(DicomTag tag,
                        Ordering ordering);

    const std::map<DicomTag, Ordering>& GetTagOrdering() const
    {
      return tagOrdering_;
    }

    void AddLabel(const std::string& label)
    {
      labels_.insert(label);
    }

    const std::set<std::string>& GetLabels() const
    {
      return labels_;
    }

    void AddMetadataConstraint(MetadataType metadata,
                               const std::string& value);

    const std::map<MetadataType, std::string>& GetMetadataConstraints() const
    {
      return metadataConstraints_;
    }
  };
}
