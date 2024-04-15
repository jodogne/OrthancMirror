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


#include "FindResponse.h"

#include "../../../OrthancFramework/Sources/DicomFormat/DicomInstanceHasher.h"
#include "../../../OrthancFramework/Sources/OrthancException.h"

#include <cassert>


namespace Orthanc
{
  static void ExtractOrthancIdentifiers(OrthancIdentifiers& identifiers,
                                        ResourceType level,
                                        const DicomMap& dicom)
  {
    switch (level)
    {
      case ResourceType_Patient:
      {
        std::string patientId;
        if (!dicom.LookupStringValue(patientId, Orthanc::DICOM_TAG_PATIENT_ID, false))
        {
          throw OrthancException(ErrorCode_ParameterOutOfRange);
        }
        else
        {
          DicomInstanceHasher hasher(patientId, "", "", "");
          identifiers.SetPatientId(hasher.HashPatient());
        }
        break;
      }

      case ResourceType_Study:
      {
        std::string patientId, studyInstanceUid;
        if (!dicom.LookupStringValue(patientId, Orthanc::DICOM_TAG_PATIENT_ID, false) ||
            !dicom.LookupStringValue(studyInstanceUid, Orthanc::DICOM_TAG_STUDY_INSTANCE_UID, false))
        {
          throw OrthancException(ErrorCode_ParameterOutOfRange);
        }
        else
        {
          DicomInstanceHasher hasher(patientId, studyInstanceUid, "", "");
          identifiers.SetPatientId(hasher.HashPatient());
          identifiers.SetStudyId(hasher.HashStudy());
        }
        break;
      }

      case ResourceType_Series:
      {
        std::string patientId, studyInstanceUid, seriesInstanceUid;
        if (!dicom.LookupStringValue(patientId, Orthanc::DICOM_TAG_PATIENT_ID, false) ||
            !dicom.LookupStringValue(studyInstanceUid, Orthanc::DICOM_TAG_STUDY_INSTANCE_UID, false) ||
            !dicom.LookupStringValue(seriesInstanceUid, Orthanc::DICOM_TAG_SERIES_INSTANCE_UID, false))
        {
          throw OrthancException(ErrorCode_ParameterOutOfRange);
        }
        else
        {
          DicomInstanceHasher hasher(patientId, studyInstanceUid, seriesInstanceUid, "");
          identifiers.SetPatientId(hasher.HashPatient());
          identifiers.SetStudyId(hasher.HashStudy());
          identifiers.SetSeriesId(hasher.HashSeries());
        }
        break;
      }

      case ResourceType_Instance:
      {
        std::string patientId, studyInstanceUid, seriesInstanceUid, sopInstanceUid;
        if (!dicom.LookupStringValue(patientId, Orthanc::DICOM_TAG_PATIENT_ID, false) ||
            !dicom.LookupStringValue(studyInstanceUid, Orthanc::DICOM_TAG_STUDY_INSTANCE_UID, false) ||
            !dicom.LookupStringValue(seriesInstanceUid, Orthanc::DICOM_TAG_SERIES_INSTANCE_UID, false) ||
            !dicom.LookupStringValue(sopInstanceUid, Orthanc::DICOM_TAG_SOP_INSTANCE_UID, false))
        {
          throw OrthancException(ErrorCode_ParameterOutOfRange);
        }
        else
        {
          DicomInstanceHasher hasher(patientId, studyInstanceUid, seriesInstanceUid, sopInstanceUid);
          identifiers.SetPatientId(hasher.HashPatient());
          identifiers.SetStudyId(hasher.HashStudy());
          identifiers.SetSeriesId(hasher.HashSeries());
          identifiers.SetInstanceId(hasher.HashInstance());
        }
        break;
      }

      default:
        throw OrthancException(ErrorCode_NotImplemented);
    }
  }


  FindResponse::Item::Item(ResourceType level,
                           DicomMap* dicomMap /* takes ownership */) :
    level_(level),
    dicomMap_(dicomMap)
  {
    if (dicomMap == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
    else
    {
      ExtractOrthancIdentifiers(identifiers_, level, *dicomMap);
    }
  }


  void FindResponse::Item::AddMetadata(MetadataType metadata,
                                       const std::string& value)
  {
    if (metadata_.find(metadata) != metadata_.end())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);  // Metadata already present
    }
    else
    {
      metadata_[metadata] = value;
    }
  }


  bool FindResponse::Item::LookupMetadata(std::string& value,
                                          MetadataType metadata) const
  {
    std::map<MetadataType, std::string>::const_iterator found = metadata_.find(metadata);

    if (found == metadata_.end())
    {
      return false;
    }
    else
    {
      value = found->second;
      return true;
    }
  }


  void FindResponse::Item::ListMetadata(std::set<MetadataType> target) const
  {
    target.clear();

    for (std::map<MetadataType, std::string>::const_iterator it = metadata_.begin(); it != metadata_.end(); ++it)
    {
      target.insert(it->first);
    }
  }


  const DicomMap& FindResponse::Item::GetDicomMap() const
  {
    if (dicomMap_.get() == NULL)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      return *dicomMap_;
    }
  }


  FindResponse::~FindResponse()
  {
    for (size_t i = 0; i < items_.size(); i++)
    {
      assert(items_[i] != NULL);
      delete items_[i];
    }
  }


  void FindResponse::Add(Item* item /* takes ownership */)
  {
    if (item == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
    else
    {
      items_.push_back(item);
    }
  }


  const FindResponse::Item& FindResponse::GetItem(size_t index) const
  {
    if (index >= items_.size())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
    else
    {
      assert(items_[index] != NULL);
      return *items_[index];
    }
  }
}
