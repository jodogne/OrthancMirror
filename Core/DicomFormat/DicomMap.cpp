/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2015 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * In addition, as a special exception, the copyright holders of this
 * program give permission to link the code of its release with the
 * OpenSSL project's "OpenSSL" library (or with modified versions of it
 * that use the same license as the "OpenSSL" library), and distribute
 * the linked executables. You must obey the GNU General Public License
 * in all respects for all of the code used other than "OpenSSL". If you
 * modify file(s) with this exception, you may extend this exception to
 * your version of the file(s), but you are not obligated to do so. If
 * you do not wish to do so, delete this exception statement from your
 * version. If you delete this exception statement from all source files
 * in the program, then also delete it here.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#include "../PrecompiledHeaders.h"
#include "DicomMap.h"

#include <stdio.h>
#include <memory>
#include "DicomString.h"
#include "DicomArray.h"
#include "../OrthancException.h"


namespace Orthanc
{
  static DicomTag patientTags[] =
  {
    //DicomTag(0x0010, 0x1010), // PatientAge
    //DicomTag(0x0010, 0x1040)  // PatientAddress
    DicomTag(0x0010, 0x0010),   // PatientName
    DicomTag(0x0010, 0x0030),   // PatientBirthDate
    DicomTag(0x0010, 0x0040),   // PatientSex
    DicomTag(0x0010, 0x1000),   // OtherPatientIDs
    DICOM_TAG_PATIENT_ID
  };

  static DicomTag studyTags[] =
  {
    //DicomTag(0x0010, 0x1020), // PatientSize
    //DicomTag(0x0010, 0x1030)  // PatientWeight
    DicomTag(0x0008, 0x0020),   // StudyDate
    DicomTag(0x0008, 0x0030),   // StudyTime
    DicomTag(0x0008, 0x1030),   // StudyDescription
    DicomTag(0x0020, 0x0010),   // StudyID
    DICOM_TAG_ACCESSION_NUMBER,
    DICOM_TAG_STUDY_INSTANCE_UID
  };

  static DicomTag seriesTags[] =
  {
    //DicomTag(0x0010, 0x1080), // MilitaryRank
    DicomTag(0x0008, 0x0021),   // SeriesDate
    DicomTag(0x0008, 0x0031),   // SeriesTime
    DicomTag(0x0008, 0x0060),   // Modality
    DicomTag(0x0008, 0x0070),   // Manufacturer
    DicomTag(0x0008, 0x1010),   // StationName
    DicomTag(0x0008, 0x103e),   // SeriesDescription
    DicomTag(0x0018, 0x0015),   // BodyPartExamined
    DicomTag(0x0018, 0x0024),   // SequenceName
    DicomTag(0x0018, 0x1030),   // ProtocolName
    DicomTag(0x0020, 0x0011),   // SeriesNumber
    DICOM_TAG_CARDIAC_NUMBER_OF_IMAGES,
    DICOM_TAG_IMAGES_IN_ACQUISITION,
    DICOM_TAG_NUMBER_OF_TEMPORAL_POSITIONS,
    DICOM_TAG_NUMBER_OF_SLICES,
    DICOM_TAG_NUMBER_OF_TIME_SLICES,
    DICOM_TAG_SERIES_INSTANCE_UID
  };

  static DicomTag instanceTags[] =
  {
    DicomTag(0x0008, 0x0012),   // InstanceCreationDate
    DicomTag(0x0008, 0x0013),   // InstanceCreationTime
    DicomTag(0x0020, 0x0012),   // AcquisitionNumber
    DICOM_TAG_IMAGE_INDEX,
    DICOM_TAG_INSTANCE_NUMBER,
    DICOM_TAG_NUMBER_OF_FRAMES,
    DICOM_TAG_TEMPORAL_POSITION_IDENTIFIER,
    DICOM_TAG_SOP_INSTANCE_UID
  };




  void DicomMap::SetValue(uint16_t group, 
                          uint16_t element, 
                          DicomValue* value)
  {
    DicomTag tag(group, element);
    Map::iterator it = map_.find(tag);

    if (it != map_.end())
    {
      delete it->second;
      it->second = value;
    }
    else
    {
      map_.insert(std::make_pair(tag, value));
    }
  }

  void DicomMap::SetValue(DicomTag tag, 
                          DicomValue* value)
  {
    SetValue(tag.GetGroup(), tag.GetElement(), value);
  }




  void DicomMap::Clear()
  {
    for (Map::iterator it = map_.begin(); it != map_.end(); ++it)
    {
      delete it->second;
    }

    map_.clear();
  }


  void DicomMap::ExtractTags(DicomMap& result,
                             const DicomTag* tags,
                             size_t count) const
  {
    result.Clear();

    for (unsigned int i = 0; i < count; i++)
    {
      Map::const_iterator it = map_.find(tags[i]);
      if (it != map_.end())
      {
        result.SetValue(it->first, it->second->Clone());
      }
    }
  }


  void DicomMap::ExtractPatientInformation(DicomMap& result) const
  {
    ExtractTags(result, patientTags, sizeof(patientTags) / sizeof(DicomTag));
  }

  void DicomMap::ExtractStudyInformation(DicomMap& result) const
  {
    ExtractTags(result, studyTags, sizeof(studyTags) / sizeof(DicomTag));
  }

  void DicomMap::ExtractSeriesInformation(DicomMap& result) const
  {
    ExtractTags(result, seriesTags, sizeof(seriesTags) / sizeof(DicomTag));
  }

  void DicomMap::ExtractInstanceInformation(DicomMap& result) const
  {
    ExtractTags(result, instanceTags, sizeof(instanceTags) / sizeof(DicomTag));
  }



  DicomMap* DicomMap::Clone() const
  {
    std::auto_ptr<DicomMap> result(new DicomMap);

    for (Map::const_iterator it = map_.begin(); it != map_.end(); ++it)
    {
      result->map_.insert(std::make_pair(it->first, it->second->Clone()));
    }

    return result.release();
  }


  void DicomMap::Assign(const DicomMap& other)
  {
    Clear();

    for (Map::const_iterator it = other.map_.begin(); it != other.map_.end(); ++it)
    {
      map_.insert(std::make_pair(it->first, it->second->Clone()));
    }
  }


  const DicomValue& DicomMap::GetValue(const DicomTag& tag) const
  {
    const DicomValue* value = TestAndGetValue(tag);

    if (value)
    {
      return *value;
    }
    else
    {
      throw OrthancException(ErrorCode_InexistentTag);
    }
  }


  const DicomValue* DicomMap::TestAndGetValue(const DicomTag& tag) const
  {
    Map::const_iterator it = map_.find(tag);

    if (it == map_.end())
    {
      return NULL;
    }
    else
    {
      return it->second;
    }
  }


  void DicomMap::Remove(const DicomTag& tag) 
  {
    Map::iterator it = map_.find(tag);
    if (it != map_.end())
    {
      delete it->second;
      map_.erase(it);
    }
  }


  static void SetupFindTemplate(DicomMap& result,
                                const DicomTag* tags,
                                size_t count) 
  {
    result.Clear();

    for (size_t i = 0; i < count; i++)
    {
      result.SetValue(tags[i], "");
    }
  }

  void DicomMap::SetupFindPatientTemplate(DicomMap& result)
  {
    SetupFindTemplate(result, patientTags, sizeof(patientTags) / sizeof(DicomTag));
  }

  void DicomMap::SetupFindStudyTemplate(DicomMap& result)
  {
    SetupFindTemplate(result, studyTags, sizeof(studyTags) / sizeof(DicomTag));
    result.SetValue(DICOM_TAG_ACCESSION_NUMBER, "");
    result.SetValue(DICOM_TAG_PATIENT_ID, "");
  }

  void DicomMap::SetupFindSeriesTemplate(DicomMap& result)
  {
    SetupFindTemplate(result, seriesTags, sizeof(seriesTags) / sizeof(DicomTag));
    result.SetValue(DICOM_TAG_ACCESSION_NUMBER, "");
    result.SetValue(DICOM_TAG_PATIENT_ID, "");
    result.SetValue(DICOM_TAG_STUDY_INSTANCE_UID, "");

    // These tags are considered as "main" by Orthanc, but are not in the Series module
    result.Remove(DicomTag(0x0008, 0x0070));  // Manufacturer
    result.Remove(DicomTag(0x0008, 0x1010));  // Station name
    result.Remove(DicomTag(0x0018, 0x0024));  // Sequence name
    result.Remove(DICOM_TAG_CARDIAC_NUMBER_OF_IMAGES);
    result.Remove(DICOM_TAG_IMAGES_IN_ACQUISITION);
    result.Remove(DICOM_TAG_NUMBER_OF_SLICES);
    result.Remove(DICOM_TAG_NUMBER_OF_TEMPORAL_POSITIONS);
    result.Remove(DICOM_TAG_NUMBER_OF_TIME_SLICES);
  }

  void DicomMap::SetupFindInstanceTemplate(DicomMap& result)
  {
    SetupFindTemplate(result, instanceTags, sizeof(instanceTags) / sizeof(DicomTag));
    result.SetValue(DICOM_TAG_ACCESSION_NUMBER, "");
    result.SetValue(DICOM_TAG_PATIENT_ID, "");
    result.SetValue(DICOM_TAG_STUDY_INSTANCE_UID, "");
    result.SetValue(DICOM_TAG_SERIES_INSTANCE_UID, "");
  }


  void DicomMap::CopyTagIfExists(const DicomMap& source,
                                 const DicomTag& tag)
  {
    if (source.HasTag(tag))
    {
      SetValue(tag, source.GetValue(tag));
    }
  }


  bool DicomMap::IsMainDicomTag(const DicomTag& tag, ResourceType level)
  {
    DicomTag *tags = NULL;
    size_t size;

    switch (level)
    {
      case ResourceType_Patient:
        tags = patientTags;
        size = sizeof(patientTags) / sizeof(DicomTag);
        break;

      case ResourceType_Study:
        tags = studyTags;
        size = sizeof(studyTags) / sizeof(DicomTag);
        break;

      case ResourceType_Series:
        tags = seriesTags;
        size = sizeof(seriesTags) / sizeof(DicomTag);
        break;

      case ResourceType_Instance:
        tags = instanceTags;
        size = sizeof(instanceTags) / sizeof(DicomTag);
        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    for (size_t i = 0; i < size; i++)
    {
      if (tags[i] == tag)
      {
        return true;
      }
    }

    return false;
  }

  bool DicomMap::IsMainDicomTag(const DicomTag& tag)
  {
    return (IsMainDicomTag(tag, ResourceType_Patient) ||
            IsMainDicomTag(tag, ResourceType_Study) ||
            IsMainDicomTag(tag, ResourceType_Series) ||
            IsMainDicomTag(tag, ResourceType_Instance));
  }


  void DicomMap::GetMainDicomTagsInternal(std::set<DicomTag>& result, ResourceType level)
  {
    DicomTag *tags = NULL;
    size_t size;

    switch (level)
    {
      case ResourceType_Patient:
        tags = patientTags;
        size = sizeof(patientTags) / sizeof(DicomTag);
        break;

      case ResourceType_Study:
        tags = studyTags;
        size = sizeof(studyTags) / sizeof(DicomTag);
        break;

      case ResourceType_Series:
        tags = seriesTags;
        size = sizeof(seriesTags) / sizeof(DicomTag);
        break;

      case ResourceType_Instance:
        tags = instanceTags;
        size = sizeof(instanceTags) / sizeof(DicomTag);
        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    for (size_t i = 0; i < size; i++)
    {
      result.insert(tags[i]);
    }
  }


  void DicomMap::GetMainDicomTags(std::set<DicomTag>& result, ResourceType level)
  {
    result.clear();
    GetMainDicomTagsInternal(result, level);
  }


  void DicomMap::GetMainDicomTags(std::set<DicomTag>& result)
  {
    result.clear();
    GetMainDicomTagsInternal(result, ResourceType_Patient);
    GetMainDicomTagsInternal(result, ResourceType_Study);
    GetMainDicomTagsInternal(result, ResourceType_Series);
    GetMainDicomTagsInternal(result, ResourceType_Instance);
  }


  void DicomMap::Print(FILE* fp) const
  {
    DicomArray a(*this);
    a.Print(fp);
  }


  void DicomMap::GetTags(std::set<DicomTag>& tags) const
  {
    tags.clear();

    for (Map::const_iterator it = map_.begin();
         it != map_.end(); ++it)
    {
      tags.insert(it->first);
    }
  }
}
