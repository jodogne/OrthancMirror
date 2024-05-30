/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2024 Orthanc Team SRL, Belgium
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


#include "FindRequest.h"

#include "../../../OrthancFramework/Sources/OrthancException.h"

#include "MainDicomTagsRegistry.h"

#include <cassert>


namespace Orthanc
{
  FindRequest::ParentRetrieveSpecification& FindRequest::GetParentRetrieveSpecification(ResourceType level)
  {
    if (!IsResourceLevelAboveOrEqual(level, level_))
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


  FindRequest::ChildrenRetrieveSpecification& FindRequest::GetChildrenRetrieveSpecification(ResourceType level)
  {
    if (!IsResourceLevelAboveOrEqual(level_, level))
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
    retrieveMainDicomTags_(false),
    retrieveMetadata_(false),
    retrieveLabels_(false),
    retrieveAttachments_(false),
    retrieveParentIdentifier_(false),
    retrieveOneInstanceIdentifier_(false)
  {
  }


  FindRequest::~FindRequest()
  {

    for (std::deque<Ordering*>::iterator it = ordering_.begin(); it != ordering_.end(); ++it)
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


  void FindRequest::AddDicomTagConstraint(const DicomTagConstraint& constraint)
  {
    // This behaves like "StatelessDatabaseOperations::NormalizeLookup()" in Orthanc <= 1.12.3

    if (mainDicomTagsRegistry_.get() == NULL)
    {
      // Lazy creation of the registry of main DICOM tags
      mainDicomTagsRegistry_.reset(new MainDicomTagsRegistry());
    }

    ResourceType level;
    DicomTagType type;

    mainDicomTagsRegistry_->LookupTag(level, type, constraint.GetTag());

    if (type == DicomTagType_Identifier ||
        type == DicomTagType_Main)
    {
      // Use the fact that patient-level tags are copied at the study level
      if (level == ResourceType_Patient &&
          GetLevel() != ResourceType_Patient)
      {
        level = ResourceType_Study;
      }

      dicomTagConstraints_.push_back(constraint.ConvertToDatabaseConstraint(level, type));
    }
  }


  const DatabaseConstraint& FindRequest::GetDicomTagConstraint(size_t index) const
  {
    if (index >= dicomTagConstraints_.size())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
    else
    {
      return dicomTagConstraints_[index];
    }
  }


  void FindRequest::SetLimits(uint64_t since,
                              uint64_t count)
  {
    if (hasLimits_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      hasLimits_ = true;
      limitsSince_ = since;
      limitsCount_ = count;
    }
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
                                OrderingDirection direction)
  {
    ordering_.push_back(new Ordering(Key(tag), direction));
  }


  void FindRequest::AddOrdering(MetadataType metadataType, 
                                OrderingDirection direction)
  {
    ordering_.push_back(new Ordering(Key(metadataType), direction));
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


  void FindRequest::AddRetrieveChildrenMetadata(MetadataType metadata)
  {
    if (IsRetrieveChildrenMetadata(metadata))
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      retrieveChildrenMetadata_.insert(metadata);
    }
  }


  void FindRequest::SetRetrieveOneInstanceIdentifier(bool retrieve)
  {
    if (level_ == ResourceType_Instance)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      retrieveOneInstanceIdentifier_ = retrieve;
    }
  }
}
