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


#include "FindRequest.h"

#include "../../../OrthancFramework/Sources/OrthancException.h"

#include "MainDicomTagsRegistry.h"

#include <cassert>


namespace Orthanc
{
  FindRequest::ParentSpecification& FindRequest::GetParentSpecification(ResourceType level)
  {
    if (!IsResourceLevelAboveOrEqual(level, level_) ||
        level == level_)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    switch (level)
    {
      case ResourceType_Patient:
        return retrieveParentPatient_;

      case ResourceType_Study:
        return retrieveParentStudy_;

      case ResourceType_Series:
        return retrieveParentSeries_;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }


  FindRequest::ChildrenSpecification& FindRequest::GetChildrenSpecification(ResourceType level)
  {
    if (!IsResourceLevelAboveOrEqual(level_, level) ||
        level == level_)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    switch (level)
    {
      case ResourceType_Study:
        return retrieveChildrenStudies_;

      case ResourceType_Series:
        return retrieveChildrenSeries_;

      case ResourceType_Instance:
        return retrieveChildrenInstances_;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }


  FindRequest::FindRequest(ResourceType level) :
    level_(level),
    hasLimits_(false),
    limitsSince_(0),
    limitsCount_(0),
    labelsConstraint_(LabelsConstraint_All),
    retrieveMainDicomTags_(false),
    retrieveMetadata_(false),
    retrieveMetadataRevisions_(false),
    retrieveLabels_(false),
    retrieveAttachments_(false),
    retrieveParentIdentifier_(false),
    retrieveOneInstanceMetadataAndAttachments_(false)
  {
  }


  FindRequest::~FindRequest()
  {
    for (std::deque<Ordering*>::iterator it = ordering_.begin(); it != ordering_.end(); ++it)
    {
      assert(*it != NULL);
      delete *it;
    }

    for (std::deque<DatabaseMetadataConstraint*>::iterator it = metadataConstraints_.begin(); it != metadataConstraints_.end(); ++it)
    {
      assert(*it != NULL);
      delete *it;
    }
  }


  void FindRequest::SetOrthancId(ResourceType level,
                                 const std::string& id)
  {
    switch (level)
    {
      case ResourceType_Patient:
        SetOrthancPatientId(id);
        break;

      case ResourceType_Study:
        SetOrthancStudyId(id);
        break;

      case ResourceType_Series:
        SetOrthancSeriesId(id);
        break;

      case ResourceType_Instance:
        SetOrthancInstanceId(id);
        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }


  void FindRequest::SetOrthancPatientId(const std::string& id)
  {
    orthancIdentifiers_.SetPatientId(id);
  }


  void FindRequest::SetOrthancStudyId(const std::string& id)
  {
    if (level_ == ResourceType_Patient)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      orthancIdentifiers_.SetStudyId(id);
    }
  }


  void FindRequest::SetOrthancSeriesId(const std::string& id)
  {
    if (level_ == ResourceType_Patient ||
        level_ == ResourceType_Study)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      orthancIdentifiers_.SetSeriesId(id);
    }
  }


  void FindRequest::SetOrthancInstanceId(const std::string& id)
  {
    if (level_ == ResourceType_Patient ||
        level_ == ResourceType_Study ||
        level_ == ResourceType_Series)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      orthancIdentifiers_.SetInstanceId(id);
    }
  }


  void FindRequest::SetLimits(uint64_t since,
                              uint64_t count)
  {
    hasLimits_ = true;
    limitsSince_ = since;
    limitsCount_ = count;
  }


  uint64_t FindRequest::GetLimitsSince() const
  {
    if (hasLimits_)
    {
      return limitsSince_;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }


  uint64_t FindRequest::GetLimitsCount() const
  {
    if (hasLimits_)
    {
      return limitsCount_;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }


  void FindRequest::AddOrdering(const DicomTag& tag,
                                OrderingCast cast,
                                OrderingDirection direction)
  {
    ordering_.push_back(new Ordering(Key(tag), cast, direction));
  }


  void FindRequest::AddOrdering(MetadataType metadataType, 
                                OrderingCast cast,
                                OrderingDirection direction)
  {
    ordering_.push_back(new Ordering(Key(metadataType), cast, direction));
  }


  void FindRequest::AddMetadataConstraint(DatabaseMetadataConstraint* constraint)
  {
    metadataConstraints_.push_back(constraint);
  }


  void FindRequest::SetRetrieveParentIdentifier(bool retrieve)
  {
    if (level_ == ResourceType_Patient)
    {
      throw OrthancException(ErrorCode_BadParameterType);
    }
    else
    {
      retrieveParentIdentifier_ = retrieve;
    }
  }


  void FindRequest::SetRetrieveOneInstanceMetadataAndAttachments(bool retrieve)
  {
    if (level_ == ResourceType_Instance)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      retrieveOneInstanceMetadataAndAttachments_ = retrieve;
    }
  }


  bool FindRequest::IsRetrieveOneInstanceMetadataAndAttachments() const
  {
    if (level_ == ResourceType_Instance)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      return retrieveOneInstanceMetadataAndAttachments_;
    }
  }

  bool FindRequest::HasConstraints() const
  {
    return (!GetDicomTagConstraints().IsEmpty() ||
            GetMetadataConstraintsCount() != 0 ||
            !GetLabels().empty() ||
            !GetOrdering().empty());
  }


  bool FindRequest::IsTrivialFind(std::string& publicId /* out */) const
  {
    if (HasConstraints())
    {
      return false;
    }
    else if (GetLevel() == ResourceType_Patient &&
             GetOrthancIdentifiers().HasPatientId() &&
             !GetOrthancIdentifiers().HasStudyId() &&
             !GetOrthancIdentifiers().HasSeriesId() &&
             !GetOrthancIdentifiers().HasInstanceId())
    {
      publicId = GetOrthancIdentifiers().GetPatientId();
      return true;
    }
    else if (GetLevel() == ResourceType_Study &&
             !GetOrthancIdentifiers().HasPatientId() &&
             GetOrthancIdentifiers().HasStudyId() &&
             !GetOrthancIdentifiers().HasSeriesId() &&
             !GetOrthancIdentifiers().HasInstanceId())
    {
      publicId = GetOrthancIdentifiers().GetStudyId();
      return true;
    }
    else if (GetLevel() == ResourceType_Series &&
             !GetOrthancIdentifiers().HasPatientId() &&
             !GetOrthancIdentifiers().HasStudyId() &&
             GetOrthancIdentifiers().HasSeriesId() &&
             !GetOrthancIdentifiers().HasInstanceId())
    {
      publicId = GetOrthancIdentifiers().GetSeriesId();
      return true;
    }
    else if (GetLevel() == ResourceType_Instance &&
             !GetOrthancIdentifiers().HasPatientId() &&
             !GetOrthancIdentifiers().HasStudyId() &&
             !GetOrthancIdentifiers().HasSeriesId() &&
             GetOrthancIdentifiers().HasInstanceId())
    {
      publicId = GetOrthancIdentifiers().GetInstanceId();
      return true;
    }
    else
    {
      return false;
    }
  }
}
