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


#include "FindRequest.h"

#include "../../../OrthancFramework/Sources/OrthancException.h"


#include <cassert>

namespace Orthanc
{
  bool FindRequest::IsCompatibleLevel(ResourceType levelOfInterest) const
  {
    switch (level_)
    {
      case ResourceType_Patient:
        return (levelOfInterest == ResourceType_Patient);

      case ResourceType_Study:
        return (levelOfInterest == ResourceType_Patient ||
                levelOfInterest == ResourceType_Study);

      case ResourceType_Series:
        return (levelOfInterest == ResourceType_Patient ||
                levelOfInterest == ResourceType_Study ||
                levelOfInterest == ResourceType_Series);

      case ResourceType_Instance:
        return (levelOfInterest == ResourceType_Patient ||
                levelOfInterest == ResourceType_Study ||
                levelOfInterest == ResourceType_Series ||
                levelOfInterest == ResourceType_Instance);

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }


  FindRequest::FindRequest(ResourceType level) :
    level_(level),
    hasLimits_(false),
    limitsSince_(0),
    limitsCount_(0),
    responseContent_(ResponseContent_IdentifiersOnly),
    retrievePatientTags_(false),
    retrieveStudyTags_(false),
    retrieveSeriesTags_(false),
    retrieveInstanceTags_(false)
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

  void FindRequest::AddDicomTagConstraint(const DicomTagConstraint& constraint)
  {
    dicomTagConstraints_.push_back(constraint);
  }

  const DicomTagConstraint& FindRequest::GetDicomTagConstraint(size_t index) const
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


  void FindRequest::SetRetrieveTagsAtLevel(ResourceType levelOfInterest,
                                           bool retrieve)
  {
    if (!IsCompatibleLevel(levelOfInterest))
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    switch (levelOfInterest)
    {
      case ResourceType_Patient:
        retrievePatientTags_ = true;
        break;

      case ResourceType_Study:
        retrieveStudyTags_ = true;
        break;

      case ResourceType_Series:
        retrieveSeriesTags_ = true;
        break;

      case ResourceType_Instance:
        retrieveInstanceTags_ = true;
        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }


  bool FindRequest::IsRetrieveTagsAtLevel(ResourceType levelOfInterest) const
  {
    switch (levelOfInterest)
    {
      case ResourceType_Patient:
        return retrievePatientTags_;

      case ResourceType_Study:
        return retrieveStudyTags_;

      case ResourceType_Series:
        return retrieveSeriesTags_;

      case ResourceType_Instance:
        return retrieveInstanceTags_;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
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

}
