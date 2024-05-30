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


namespace Orthanc
{
  OrthancIdentifiers::OrthancIdentifiers(const OrthancIdentifiers& other)
  {
    if (other.HasPatientId())
    {
      SetPatientId(other.GetPatientId());
    }

    if (other.HasStudyId())
    {
      SetStudyId(other.GetStudyId());
    }

    if (other.HasSeriesId())
    {
      SetSeriesId(other.GetSeriesId());
    }

    if (other.HasInstanceId())
    {
      SetInstanceId(other.GetInstanceId());
    }
  }


  void OrthancIdentifiers::SetPatientId(const std::string& id)
  {
    if (HasPatientId())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      patientId_.reset(new std::string(id));
    }
  }


  const std::string& OrthancIdentifiers::GetPatientId() const
  {
    if (HasPatientId())
    {
      return *patientId_;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }


  void OrthancIdentifiers::SetStudyId(const std::string& id)
  {
    if (HasStudyId())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      studyId_.reset(new std::string(id));
    }
  }


  const std::string& OrthancIdentifiers::GetStudyId() const
  {
    if (HasStudyId())
    {
      return *studyId_;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }


  void OrthancIdentifiers::SetSeriesId(const std::string& id)
  {
    if (HasSeriesId())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      seriesId_.reset(new std::string(id));
    }
  }


  const std::string& OrthancIdentifiers::GetSeriesId() const
  {
    if (HasSeriesId())
    {
      return *seriesId_;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }


  void OrthancIdentifiers::SetInstanceId(const std::string& id)
  {
    if (HasInstanceId())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      instanceId_.reset(new std::string(id));
    }
  }


  const std::string& OrthancIdentifiers::GetInstanceId() const
  {
    if (HasInstanceId())
    {
      return *instanceId_;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }


  ResourceType OrthancIdentifiers::DetectLevel() const
  {
    if (HasPatientId() &&
        !HasStudyId() &&
        !HasSeriesId() &&
        !HasInstanceId())
    {
      return ResourceType_Patient;
    }
    else if (HasPatientId() &&
             HasStudyId() &&
             !HasSeriesId() &&
             !HasInstanceId())
    {
      return ResourceType_Study;
    }
    else if (HasPatientId() &&
             HasStudyId() &&
             HasSeriesId() &&
             !HasInstanceId())
    {
      return ResourceType_Series;
    }
    else if (HasPatientId() &&
             HasStudyId() &&
             HasSeriesId() &&
             HasInstanceId())
    {
      return ResourceType_Instance;
    }
    else
    {
      throw OrthancException(ErrorCode_InexistentItem);
    }
  }


  void OrthancIdentifiers::SetLevel(ResourceType level,
                                    const std::string id)
  {
    switch (level)
    {
      case ResourceType_Patient:
        SetPatientId(id);
        break;

      case ResourceType_Study:
        SetStudyId(id);
        break;

      case ResourceType_Series:
        SetSeriesId(id);
        break;

      case ResourceType_Instance:
        SetInstanceId(id);
        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }


  std::string OrthancIdentifiers::GetLevel(ResourceType level) const
  {
    switch (level)
    {
      case ResourceType_Patient:
        return GetPatientId();

      case ResourceType_Study:
        return GetStudyId();

      case ResourceType_Series:
        return GetSeriesId();

      case ResourceType_Instance:
        return GetInstanceId();

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }
}
