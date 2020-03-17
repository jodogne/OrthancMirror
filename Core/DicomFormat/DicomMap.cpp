/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
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

#include "../Compatibility.h"
#include "../Endianness.h"
#include "../Logging.h"
#include "../OrthancException.h"
#include "../Toolbox.h"
#include "DicomArray.h"


namespace Orthanc
{
  namespace
  {
    struct MainDicomTag
    {
      const DicomTag tag_;
      const char*    name_;
    };
  }

  static const MainDicomTag PATIENT_MAIN_DICOM_TAGS[] =
  {
    // { DicomTag(0x0010, 0x1010), "PatientAge" },
    // { DicomTag(0x0010, 0x1040), "PatientAddress" },
    { DicomTag(0x0010, 0x0010), "PatientName" },
    { DicomTag(0x0010, 0x0030), "PatientBirthDate" },
    { DicomTag(0x0010, 0x0040), "PatientSex" },
    { DicomTag(0x0010, 0x1000), "OtherPatientIDs" },
    { DICOM_TAG_PATIENT_ID, "PatientID" }
  };
    
  static const MainDicomTag STUDY_MAIN_DICOM_TAGS[] =
  {
    // { DicomTag(0x0010, 0x1020), "PatientSize" },
    // { DicomTag(0x0010, 0x1030), "PatientWeight" },
    { DICOM_TAG_STUDY_DATE, "StudyDate" },
    { DicomTag(0x0008, 0x0030), "StudyTime" },
    { DicomTag(0x0020, 0x0010), "StudyID" },
    { DICOM_TAG_STUDY_DESCRIPTION, "StudyDescription" },
    { DICOM_TAG_ACCESSION_NUMBER, "AccessionNumber" },
    { DICOM_TAG_STUDY_INSTANCE_UID, "StudyInstanceUID" },

    // New in db v6
    { DICOM_TAG_REQUESTED_PROCEDURE_DESCRIPTION, "RequestedProcedureDescription" },
    { DICOM_TAG_INSTITUTION_NAME, "InstitutionName" },
    { DICOM_TAG_REQUESTING_PHYSICIAN, "RequestingPhysician" },
    { DICOM_TAG_REFERRING_PHYSICIAN_NAME, "ReferringPhysicianName" }
  };
    
  static const MainDicomTag SERIES_MAIN_DICOM_TAGS[] =
  {
    // { DicomTag(0x0010, 0x1080), "MilitaryRank" },
    { DicomTag(0x0008, 0x0021), "SeriesDate" },
    { DicomTag(0x0008, 0x0031), "SeriesTime" },
    { DICOM_TAG_MODALITY, "Modality" },
    { DicomTag(0x0008, 0x0070), "Manufacturer" },
    { DicomTag(0x0008, 0x1010), "StationName" },
    { DICOM_TAG_SERIES_DESCRIPTION, "SeriesDescription" },
    { DicomTag(0x0018, 0x0015), "BodyPartExamined" },
    { DicomTag(0x0018, 0x0024), "SequenceName" },
    { DicomTag(0x0018, 0x1030), "ProtocolName" },
    { DicomTag(0x0020, 0x0011), "SeriesNumber" },
    { DICOM_TAG_CARDIAC_NUMBER_OF_IMAGES, "CardiacNumberOfImages" },
    { DICOM_TAG_IMAGES_IN_ACQUISITION, "ImagesInAcquisition" },
    { DICOM_TAG_NUMBER_OF_TEMPORAL_POSITIONS, "NumberOfTemporalPositions" },
    { DICOM_TAG_NUMBER_OF_SLICES, "NumberOfSlices" },
    { DICOM_TAG_NUMBER_OF_TIME_SLICES, "NumberOfTimeSlices" },
    { DICOM_TAG_SERIES_INSTANCE_UID, "SeriesInstanceUID" },

    // New in db v6
    { DICOM_TAG_IMAGE_ORIENTATION_PATIENT, "ImageOrientationPatient" },
    { DICOM_TAG_SERIES_TYPE, "SeriesType" },
    { DICOM_TAG_OPERATOR_NAME, "OperatorsName" },
    { DICOM_TAG_PERFORMED_PROCEDURE_STEP_DESCRIPTION, "PerformedProcedureStepDescription" },
    { DICOM_TAG_ACQUISITION_DEVICE_PROCESSING_DESCRIPTION, "AcquisitionDeviceProcessingDescription" },
    { DICOM_TAG_CONTRAST_BOLUS_AGENT, "ContrastBolusAgent" }
  };
    
  static const MainDicomTag INSTANCE_MAIN_DICOM_TAGS[] =
  {
    { DicomTag(0x0008, 0x0012), "InstanceCreationDate" },
    { DicomTag(0x0008, 0x0013), "InstanceCreationTime" },
    { DicomTag(0x0020, 0x0012), "AcquisitionNumber" },
    { DICOM_TAG_IMAGE_INDEX, "ImageIndex" },
    { DICOM_TAG_INSTANCE_NUMBER, "InstanceNumber" },
    { DICOM_TAG_NUMBER_OF_FRAMES, "NumberOfFrames" },
    { DICOM_TAG_TEMPORAL_POSITION_IDENTIFIER, "TemporalPositionIdentifier" },
    { DICOM_TAG_SOP_INSTANCE_UID, "SOPInstanceUID" },

    // New in db v6
    { DICOM_TAG_IMAGE_POSITION_PATIENT, "ImagePositionPatient" },
    { DICOM_TAG_IMAGE_COMMENTS, "ImageComments" },

    /**
     * Main DICOM tags that are not part of any release of the
     * database schema yet, and that will be part of future db v7. In
     * the meantime, the user must call "/tools/reconstruct" once to
     * access these tags if the corresponding DICOM files where
     * indexed in the database by an older version of Orthanc.
     **/
    { DICOM_TAG_IMAGE_ORIENTATION_PATIENT, "ImageOrientationPatient" }  // New in Orthanc 1.4.2
  };


  static void LoadMainDicomTags(const MainDicomTag*& tags,
                                size_t& size,
                                ResourceType level)
  {
    switch (level)
    {
      case ResourceType_Patient:
        tags = PATIENT_MAIN_DICOM_TAGS;
        size = sizeof(PATIENT_MAIN_DICOM_TAGS) / sizeof(MainDicomTag);
        break;

      case ResourceType_Study:
        tags = STUDY_MAIN_DICOM_TAGS;
        size = sizeof(STUDY_MAIN_DICOM_TAGS) / sizeof(MainDicomTag);
        break;

      case ResourceType_Series:
        tags = SERIES_MAIN_DICOM_TAGS;
        size = sizeof(SERIES_MAIN_DICOM_TAGS) / sizeof(MainDicomTag);
        break;

      case ResourceType_Instance:
        tags = INSTANCE_MAIN_DICOM_TAGS;
        size = sizeof(INSTANCE_MAIN_DICOM_TAGS) / sizeof(MainDicomTag);
        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }


  static void LoadMainDicomTags(std::map<DicomTag, std::string>& target,
                                ResourceType level)
  {
    const MainDicomTag* tags = NULL;
    size_t size;
    LoadMainDicomTags(tags, size, level);

    assert(tags != NULL &&
           size != 0);

    for (size_t i = 0; i < size; i++)
    {
      assert(target.find(tags[i].tag_) == target.end());
      
      target[tags[i].tag_] = tags[i].name_;
    }
  }


  namespace
  {
    class DicomTag2 : public DicomTag
    {
    public:
      DicomTag2() :
        DicomTag(0, 0)   // To make std::map<> happy
      {
      }

      DicomTag2(const DicomTag& tag) :
        DicomTag(tag)
      {
      }
    };
  }


  static void LoadMainDicomTags(std::map<std::string, DicomTag2>& target,
                                ResourceType level)
  {
    const MainDicomTag* tags = NULL;
    size_t size;
    LoadMainDicomTags(tags, size, level);

    assert(tags != NULL &&
           size != 0);

    for (size_t i = 0; i < size; i++)
    {
      assert(target.find(tags[i].name_) == target.end());
      
      target[tags[i].name_] = tags[i].tag_;
    }
  }


  void DicomMap::SetValueInternal(uint16_t group, 
                                  uint16_t element, 
                                  DicomValue* value)
  {
    DicomTag tag(group, element);
    Content::iterator it = content_.find(tag);

    if (it != content_.end())
    {
      delete it->second;
      it->second = value;
    }
    else
    {
      content_.insert(std::make_pair(tag, value));
    }
  }


  void DicomMap::Clear()
  {
    for (Content::iterator it = content_.begin(); it != content_.end(); ++it)
    {
      assert(it->second != NULL);
      delete it->second;
    }

    content_.clear();
  }


  static void ExtractTags(DicomMap& result,
                          const DicomMap::Content& source,
                          const MainDicomTag* tags,
                          size_t count)
  {
    result.Clear();

    for (unsigned int i = 0; i < count; i++)
    {
      DicomMap::Content::const_iterator it = source.find(tags[i].tag_);
      if (it != source.end())
      {
        result.SetValue(it->first, *it->second /* value will be cloned */);
      }
    }
  }


  void DicomMap::ExtractPatientInformation(DicomMap& result) const
  {
    ExtractTags(result, content_, PATIENT_MAIN_DICOM_TAGS, sizeof(PATIENT_MAIN_DICOM_TAGS) / sizeof(MainDicomTag));
  }

  void DicomMap::ExtractStudyInformation(DicomMap& result) const
  {
    ExtractTags(result, content_, STUDY_MAIN_DICOM_TAGS, sizeof(STUDY_MAIN_DICOM_TAGS) / sizeof(MainDicomTag));
  }

  void DicomMap::ExtractSeriesInformation(DicomMap& result) const
  {
    ExtractTags(result, content_, SERIES_MAIN_DICOM_TAGS, sizeof(SERIES_MAIN_DICOM_TAGS) / sizeof(MainDicomTag));
  }

  void DicomMap::ExtractInstanceInformation(DicomMap& result) const
  {
    ExtractTags(result, content_, INSTANCE_MAIN_DICOM_TAGS, sizeof(INSTANCE_MAIN_DICOM_TAGS) / sizeof(MainDicomTag));
  }



  DicomMap* DicomMap::Clone() const
  {
    std::unique_ptr<DicomMap> result(new DicomMap);

    for (Content::const_iterator it = content_.begin(); it != content_.end(); ++it)
    {
      result->content_.insert(std::make_pair(it->first, it->second->Clone()));
    }

    return result.release();
  }


  void DicomMap::Assign(const DicomMap& other)
  {
    Clear();

    for (Content::const_iterator it = other.content_.begin(); it != other.content_.end(); ++it)
    {
      content_.insert(std::make_pair(it->first, it->second->Clone()));
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
    Content::const_iterator it = content_.find(tag);

    if (it == content_.end())
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
    Content::iterator it = content_.find(tag);
    if (it != content_.end())
    {
      delete it->second;
      content_.erase(it);
    }
  }


  static void SetupFindTemplate(DicomMap& result,
                                const MainDicomTag* tags,
                                size_t count) 
  {
    result.Clear();

    for (size_t i = 0; i < count; i++)
    {
      result.SetValue(tags[i].tag_, "", false);
    }
  }

  void DicomMap::SetupFindPatientTemplate(DicomMap& result)
  {
    SetupFindTemplate(result, PATIENT_MAIN_DICOM_TAGS, sizeof(PATIENT_MAIN_DICOM_TAGS) / sizeof(MainDicomTag));
  }

  void DicomMap::SetupFindStudyTemplate(DicomMap& result)
  {
    SetupFindTemplate(result, STUDY_MAIN_DICOM_TAGS, sizeof(STUDY_MAIN_DICOM_TAGS) / sizeof(MainDicomTag));
    result.SetValue(DICOM_TAG_ACCESSION_NUMBER, "", false);
    result.SetValue(DICOM_TAG_PATIENT_ID, "", false);

    // These main DICOM tags are only indirectly related to the
    // General Study Module, remove them
    result.Remove(DICOM_TAG_INSTITUTION_NAME);
    result.Remove(DICOM_TAG_REQUESTING_PHYSICIAN);
    result.Remove(DICOM_TAG_REQUESTED_PROCEDURE_DESCRIPTION);
  }

  void DicomMap::SetupFindSeriesTemplate(DicomMap& result)
  {
    SetupFindTemplate(result, SERIES_MAIN_DICOM_TAGS, sizeof(SERIES_MAIN_DICOM_TAGS) / sizeof(MainDicomTag));
    result.SetValue(DICOM_TAG_ACCESSION_NUMBER, "", false);
    result.SetValue(DICOM_TAG_PATIENT_ID, "", false);
    result.SetValue(DICOM_TAG_STUDY_INSTANCE_UID, "", false);

    // These tags are considered as "main" by Orthanc, but are not in the Series module
    result.Remove(DicomTag(0x0008, 0x0070));  // Manufacturer
    result.Remove(DicomTag(0x0008, 0x1010));  // Station name
    result.Remove(DicomTag(0x0018, 0x0024));  // Sequence name
    result.Remove(DICOM_TAG_CARDIAC_NUMBER_OF_IMAGES);
    result.Remove(DICOM_TAG_IMAGES_IN_ACQUISITION);
    result.Remove(DICOM_TAG_NUMBER_OF_SLICES);
    result.Remove(DICOM_TAG_NUMBER_OF_TEMPORAL_POSITIONS);
    result.Remove(DICOM_TAG_NUMBER_OF_TIME_SLICES);
    result.Remove(DICOM_TAG_IMAGE_ORIENTATION_PATIENT);
    result.Remove(DICOM_TAG_SERIES_TYPE);
    result.Remove(DICOM_TAG_ACQUISITION_DEVICE_PROCESSING_DESCRIPTION);
    result.Remove(DICOM_TAG_CONTRAST_BOLUS_AGENT);
  }

  void DicomMap::SetupFindInstanceTemplate(DicomMap& result)
  {
    SetupFindTemplate(result, INSTANCE_MAIN_DICOM_TAGS, sizeof(INSTANCE_MAIN_DICOM_TAGS) / sizeof(MainDicomTag));
    result.SetValue(DICOM_TAG_ACCESSION_NUMBER, "", false);
    result.SetValue(DICOM_TAG_PATIENT_ID, "", false);
    result.SetValue(DICOM_TAG_STUDY_INSTANCE_UID, "", false);
    result.SetValue(DICOM_TAG_SERIES_INSTANCE_UID, "", false);
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
    const MainDicomTag *tags = NULL;
    size_t size;
    LoadMainDicomTags(tags, size, level);

    for (size_t i = 0; i < size; i++)
    {
      if (tags[i].tag_ == tag)
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
    const MainDicomTag *tags = NULL;
    size_t size;
    LoadMainDicomTags(tags, size, level);

    for (size_t i = 0; i < size; i++)
    {
      result.insert(tags[i].tag_);
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


  void DicomMap::GetTags(std::set<DicomTag>& tags) const
  {
    tags.clear();

    for (Content::const_iterator it = content_.begin();
         it != content_.end(); ++it)
    {
      tags.insert(it->first);
    }
  }


  static uint16_t ReadUnsignedInteger16(const char* dicom)
  {
    return le16toh(*reinterpret_cast<const uint16_t*>(dicom));
  }


  static uint32_t ReadUnsignedInteger32(const char* dicom)
  {
    return le32toh(*reinterpret_cast<const uint32_t*>(dicom));
  }


  static bool ValidateTag(const ValueRepresentation& vr,
                          const std::string& value)
  {
    switch (vr)
    {
      case ValueRepresentation_ApplicationEntity:
        return value.size() <= 16;

      case ValueRepresentation_AgeString:
        return (value.size() == 4 &&
                isdigit(value[0]) &&
                isdigit(value[1]) &&
                isdigit(value[2]) &&
                (value[3] == 'D' || value[3] == 'W' || value[3] == 'M' || value[3] == 'Y'));

      case ValueRepresentation_AttributeTag:
        return value.size() == 4;

      case ValueRepresentation_CodeString:
        return value.size() <= 16;

      case ValueRepresentation_Date:
        return value.size() <= 18;

      case ValueRepresentation_DecimalString:
        return value.size() <= 16;

      case ValueRepresentation_DateTime:
        return value.size() <= 54;

      case ValueRepresentation_FloatingPointSingle:
        return value.size() == 4;

      case ValueRepresentation_FloatingPointDouble:
        return value.size() == 8;

      case ValueRepresentation_IntegerString:
        return value.size() <= 12;

      case ValueRepresentation_LongString:
        return value.size() <= 64;

      case ValueRepresentation_LongText:
        return value.size() <= 10240;

      case ValueRepresentation_OtherByte:
        return true;
      
      case ValueRepresentation_OtherDouble:
        return value.size() <= (static_cast<uint64_t>(1) << 32) - 8;

      case ValueRepresentation_OtherFloat:
        return value.size() <= (static_cast<uint64_t>(1) << 32) - 4;

      case ValueRepresentation_OtherLong:
        return true;

      case ValueRepresentation_OtherWord:
        return true;

      case ValueRepresentation_PersonName:
        return true;

      case ValueRepresentation_ShortString:
        return value.size() <= 16;

      case ValueRepresentation_SignedLong:
        return value.size() == 4;

      case ValueRepresentation_Sequence:
        return true;

      case ValueRepresentation_SignedShort:
        return value.size() == 2;

      case ValueRepresentation_ShortText:
        return value.size() <= 1024;

      case ValueRepresentation_Time:
        return value.size() <= 28;

      case ValueRepresentation_UnlimitedCharacters:
        return value.size() <= (static_cast<uint64_t>(1) << 32) - 2;

      case ValueRepresentation_UniqueIdentifier:
        return value.size() <= 64;

      case ValueRepresentation_UnsignedLong:
        return value.size() == 4;

      case ValueRepresentation_Unknown:
        return true;

      case ValueRepresentation_UniversalResource:
        return value.size() <= (static_cast<uint64_t>(1) << 32) - 2;

      case ValueRepresentation_UnsignedShort:
        return value.size() == 2;

      case ValueRepresentation_UnlimitedText:
        return value.size() <= (static_cast<uint64_t>(1) << 32) - 2;

      default:
        // Assume unsupported tags are OK
        return true;
    }
  }


  static void RemoveTagPadding(std::string& value,
                               const ValueRepresentation& vr)
  {
    /**
     * Remove padding from character strings, if need be. For the time
     * being, only the UI VR is supported.
     * http://dicom.nema.org/medical/dicom/current/output/chtml/part05/sect_6.2.html
     **/

    switch (vr)
    {
      case ValueRepresentation_UniqueIdentifier:
      {
        /**
         * "Values with a VR of UI shall be padded with a single
         * trailing NULL (00H) character when necessary to achieve even
         * length."
         **/

        if (!value.empty() &&
            value[value.size() - 1] == '\0')
        {
          value.resize(value.size() - 1);
        }

        break;
      }

      /**
       * TODO implement other VR
       **/

      default:
        // No padding is applicable to this VR
        break;
    }
  }


  static bool ReadNextTag(DicomTag& tag,
                          ValueRepresentation& vr,
                          std::string& value,
                          const char* dicom,
                          size_t size,
                          size_t& position)
  {
    /**
     * http://dicom.nema.org/medical/dicom/current/output/chtml/part05/chapter_7.html#sect_7.1.2
     * This function reads a data element with Explicit VR encoded using Little-Endian.
     **/

    if (position + 6 > size)
    {
      return false;
    }

    tag = DicomTag(ReadUnsignedInteger16(dicom + position),
                   ReadUnsignedInteger16(dicom + position + 2));

    vr = StringToValueRepresentation(std::string(dicom + position + 4, 2), true);
    if (vr == ValueRepresentation_NotSupported)
    {
      return false;
    }

    if (vr == ValueRepresentation_OtherByte ||
        vr == ValueRepresentation_OtherDouble ||
        vr == ValueRepresentation_OtherFloat ||
        vr == ValueRepresentation_OtherLong ||
        vr == ValueRepresentation_OtherWord ||
        vr == ValueRepresentation_Sequence ||
        vr == ValueRepresentation_UnlimitedCharacters ||
        vr == ValueRepresentation_UniversalResource ||
        vr == ValueRepresentation_UnlimitedText ||
        vr == ValueRepresentation_Unknown)    // Note that "UN" should never appear in the Meta Information
    {
      if (position + 12 > size)
      {
        return false;
      }

      uint32_t length = ReadUnsignedInteger32(dicom + position + 8);

      if (position + 12 + length > size)
      {
        return false;
      }

      value.assign(dicom + position + 12, length);
      position += (12 + length);
    }
    else
    {
      if (position + 8 > size)
      {
        return false;
      }

      uint16_t length = ReadUnsignedInteger16(dicom + position + 6);

      if (position + 8 + length > size)
      {
        return false;
      }

      value.assign(dicom + position + 8, length);
      position += (8 + length);
    }

    if (!ValidateTag(vr, value))
    {
      return false;
    }

    RemoveTagPadding(value, vr);

    return true;
  }


  bool DicomMap::IsDicomFile(const char* dicom,
                             size_t size)
  {
    /**
     * http://dicom.nema.org/medical/dicom/current/output/chtml/part10/chapter_7.html
     * According to Table 7.1-1, besides the "DICM" DICOM prefix, the
     * file preamble (i.e. dicom[0..127]) should not be taken into
     * account to determine whether the file is or is not a DICOM file.
     **/

    return (size >= 132 &&
            dicom[128] == 'D' &&
            dicom[129] == 'I' &&
            dicom[130] == 'C' &&
            dicom[131] == 'M');
  }
    

  bool DicomMap::ParseDicomMetaInformation(DicomMap& result,
                                           const char* dicom,
                                           size_t size)
  {
    if (!IsDicomFile(dicom, size))
    {
      return false;
    }


    /**
     * The DICOM File Meta Information must be encoded using the
     * Explicit VR Little Endian Transfer Syntax
     * (UID=1.2.840.10008.1.2.1).
     **/

    result.Clear();

    // First, we read the "File Meta Information Group Length" tag
    // (0002,0000) to know where to stop reading the meta header
    size_t position = 132;

    DicomTag tag(0x0000, 0x0000);  // Dummy initialization
    ValueRepresentation vr;
    std::string value;
    if (!ReadNextTag(tag, vr, value, dicom, size, position) ||
        tag.GetGroup() != 0x0002 ||
        tag.GetElement() != 0x0000 ||
        vr != ValueRepresentation_UnsignedLong ||
        value.size() != 4)
    {
      return false;
    }

    size_t stopPosition = position + ReadUnsignedInteger32(value.c_str());
    if (stopPosition > size)
    {
      return false;
    }

    while (position < stopPosition)
    {
      if (ReadNextTag(tag, vr, value, dicom, size, position))
      {
        result.SetValue(tag, value, IsBinaryValueRepresentation(vr));
      }
      else
      {
        return false;
      }
    }

    return true;
  }


  static std::string ValueAsString(const DicomMap& summary,
                                   const DicomTag& tag)
  {
    const DicomValue& value = summary.GetValue(tag);
    if (value.IsNull())
    {
      return "(null)";
    }
    else
    {
      return value.GetContent();
    }
  }


  void DicomMap::LogMissingTagsForStore() const
  {
    std::string s, t;

    if (HasTag(DICOM_TAG_PATIENT_ID))
    {
      if (t.size() > 0)
        t += ", ";
      t += "PatientID=" + ValueAsString(*this, DICOM_TAG_PATIENT_ID);
    }
    else
    {
      if (s.size() > 0)
        s += ", ";
      s += "PatientID";
    }

    if (HasTag(DICOM_TAG_STUDY_INSTANCE_UID))
    {
      if (t.size() > 0)
        t += ", ";
      t += "StudyInstanceUID=" + ValueAsString(*this, DICOM_TAG_STUDY_INSTANCE_UID);
    }
    else
    {
      if (s.size() > 0)
        s += ", ";
      s += "StudyInstanceUID";
    }

    if (HasTag(DICOM_TAG_SERIES_INSTANCE_UID))
    {
      if (t.size() > 0)
        t += ", ";
      t += "SeriesInstanceUID=" + ValueAsString(*this, DICOM_TAG_SERIES_INSTANCE_UID);
    }
    else
    {
      if (s.size() > 0)
        s += ", ";
      s += "SeriesInstanceUID";
    }

    if (HasTag(DICOM_TAG_SOP_INSTANCE_UID))
    {
      if (t.size() > 0)
        t += ", ";
      t += "SOPInstanceUID=" + ValueAsString(*this, DICOM_TAG_SOP_INSTANCE_UID);
    }
    else
    {
      if (s.size() > 0)
        s += ", ";
      s += "SOPInstanceUID";
    }

    if (t.size() == 0)
    {
      LOG(ERROR) << "Store has failed because all the required tags (" << s << ") are missing (is it a DICOMDIR file?)";
    }
    else
    {
      LOG(ERROR) << "Store has failed because required tags (" << s << ") are missing for the following instance: " << t;
    }
  }


  bool DicomMap::LookupStringValue(std::string& result,
                                   const DicomTag& tag,
                                   bool allowBinary) const
  {
    const DicomValue* value = TestAndGetValue(tag);

    if (value == NULL)
    {
      return false;
    }
    else
    {
      return value->CopyToString(result, allowBinary);
    }
  }
    
  bool DicomMap::ParseInteger32(int32_t& result,
                                const DicomTag& tag) const
  {
    const DicomValue* value = TestAndGetValue(tag);

    if (value == NULL)
    {
      return false;
    }
    else
    {
      return value->ParseInteger32(result);
    }
  }

  bool DicomMap::ParseInteger64(int64_t& result,
                                const DicomTag& tag) const
  {
    const DicomValue* value = TestAndGetValue(tag);

    if (value == NULL)
    {
      return false;
    }
    else
    {
      return value->ParseInteger64(result);
    }
  }

  bool DicomMap::ParseUnsignedInteger32(uint32_t& result,
                                        const DicomTag& tag) const
  {
    const DicomValue* value = TestAndGetValue(tag);

    if (value == NULL)
    {
      return false;
    }
    else
    {
      return value->ParseUnsignedInteger32(result);
    }
  }

  bool DicomMap::ParseUnsignedInteger64(uint64_t& result,
                                        const DicomTag& tag) const
  {
    const DicomValue* value = TestAndGetValue(tag);

    if (value == NULL)
    {
      return false;
    }
    else
    {
      return value->ParseUnsignedInteger64(result);
    }
  }

  bool DicomMap::ParseFloat(float& result,
                            const DicomTag& tag) const
  {
    const DicomValue* value = TestAndGetValue(tag);

    if (value == NULL)
    {
      return false;
    }
    else
    {
      return value->ParseFloat(result);
    }
  }

  bool DicomMap::ParseFirstFloat(float& result,
                                 const DicomTag& tag) const
  {
    const DicomValue* value = TestAndGetValue(tag);

    if (value == NULL)
    {
      return false;
    }
    else
    {
      return value->ParseFirstFloat(result);
    }
  }

  bool DicomMap::ParseDouble(double& result,
                             const DicomTag& tag) const
  {
    const DicomValue* value = TestAndGetValue(tag);

    if (value == NULL)
    {
      return false;
    }
    else
    {
      return value->ParseDouble(result);
    }
  }

  
  void DicomMap::FromDicomAsJson(const Json::Value& dicomAsJson)
  {
    if (dicomAsJson.type() != Json::objectValue)
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }
    
    Clear();
    
    Json::Value::Members tags = dicomAsJson.getMemberNames();
    for (Json::Value::Members::const_iterator
           it = tags.begin(); it != tags.end(); ++it)
    {
      DicomTag tag(0, 0);
      if (!DicomTag::ParseHexadecimal(tag, it->c_str()))
      {
        throw OrthancException(ErrorCode_CorruptedFile);
      }

      const Json::Value& value = dicomAsJson[*it];

      if (value.type() != Json::objectValue ||
          !value.isMember("Type") ||
          !value.isMember("Value") ||
          value["Type"].type() != Json::stringValue)
      {
        throw OrthancException(ErrorCode_CorruptedFile);
      }

      if (value["Type"] == "String")
      {
        if (value["Value"].type() != Json::stringValue)
        {
          throw OrthancException(ErrorCode_CorruptedFile);
        }
        else
        {
          SetValue(tag, value["Value"].asString(), false /* not binary */);
        }
      }
    }
  }


  void DicomMap::Merge(const DicomMap& other)
  {
    for (Content::const_iterator it = other.content_.begin();
         it != other.content_.end(); ++it)
    {
      assert(it->second != NULL);

      if (content_.find(it->first) == content_.end())
      {
        content_[it->first] = it->second->Clone();
      }
    }
  }


  void DicomMap::MergeMainDicomTags(const DicomMap& other,
                                    ResourceType level)
  {
    const MainDicomTag* tags = NULL;
    size_t size = 0;

    LoadMainDicomTags(tags, size, level);
    assert(tags != NULL && size > 0);

    for (size_t i = 0; i < size; i++)
    {
      Content::const_iterator found = other.content_.find(tags[i].tag_);

      if (found != other.content_.end() &&
          content_.find(tags[i].tag_) == content_.end())
      {
        assert(found->second != NULL);
        content_[tags[i].tag_] = found->second->Clone();
      }
    }
  }
    

  void DicomMap::ExtractMainDicomTags(const DicomMap& other)
  {
    Clear();
    MergeMainDicomTags(other, ResourceType_Patient);
    MergeMainDicomTags(other, ResourceType_Study);
    MergeMainDicomTags(other, ResourceType_Series);
    MergeMainDicomTags(other, ResourceType_Instance);
  }    


  bool DicomMap::HasOnlyMainDicomTags() const
  {
    // TODO - Speed up possible by making this std::set a global variable

    std::set<DicomTag> mainDicomTags;
    GetMainDicomTags(mainDicomTags);

    for (Content::const_iterator it = content_.begin(); it != content_.end(); ++it)
    {
      if (mainDicomTags.find(it->first) == mainDicomTags.end())
      {
        return false;
      }
    }

    return true;
  }
    

  void DicomMap::Serialize(Json::Value& target) const
  {
    target = Json::objectValue;

    for (Content::const_iterator it = content_.begin(); it != content_.end(); ++it)
    {
      assert(it->second != NULL);
      
      std::string tag = it->first.Format();

      Json::Value value;
      it->second->Serialize(value);

      target[tag] = value;
    }
  }
  

  void DicomMap::Unserialize(const Json::Value& source)
  {
    Clear();

    if (source.type() != Json::objectValue)
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    Json::Value::Members tags = source.getMemberNames();

    for (size_t i = 0; i < tags.size(); i++)
    {
      DicomTag tag(0, 0);
      
      if (!DicomTag::ParseHexadecimal(tag, tags[i].c_str()) ||
          content_.find(tag) != content_.end())
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }

      std::unique_ptr<DicomValue> value(new DicomValue);
      value->Unserialize(source[tags[i]]);

      content_[tag] = value.release();
    }
  }


  void DicomMap::FromDicomWeb(const Json::Value& source)
  {
    static const char* const ALPHABETIC = "Alphabetic";
    static const char* const IDEOGRAPHIC = "Ideographic";
    static const char* const INLINE_BINARY = "InlineBinary";
    static const char* const PHONETIC = "Phonetic";
    static const char* const VALUE = "Value";
    static const char* const VR = "vr";
  
    Clear();

    if (source.type() != Json::objectValue)
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }
  
    Json::Value::Members tags = source.getMemberNames();

    for (size_t i = 0; i < tags.size(); i++)
    {
      const Json::Value& item = source[tags[i]];
      DicomTag tag(0, 0);

      if (item.type() != Json::objectValue ||
          !item.isMember(VR) ||
          item[VR].type() != Json::stringValue ||
          !DicomTag::ParseHexadecimal(tag, tags[i].c_str()))
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }

      ValueRepresentation vr = StringToValueRepresentation(item[VR].asString(), false);

      if (item.isMember(INLINE_BINARY))
      {
        const Json::Value& value = item[INLINE_BINARY];

        if (value.type() == Json::stringValue)
        {
          std::string decoded;
          Toolbox::DecodeBase64(decoded, value.asString());
          SetValue(tag, decoded, true /* binary data */);
        }
      }
      else if (!item.isMember(VALUE))
      {
        // Tag is present, but it has a null value
        SetValue(tag, "", false /* not binary */);
      }
      else
      {
        const Json::Value& value = item[VALUE];

        if (value.type() == Json::arrayValue)
        {
          bool supported = true;
          
          std::string s;
          for (Json::Value::ArrayIndex i = 0; i < value.size() && supported; i++)
          {
            if (!s.empty())
            {
              s += '\\';
            }

            switch (value[i].type())
            {
              case Json::objectValue:
                if (vr == ValueRepresentation_PersonName &&
                    value[i].type() == Json::objectValue)
                {
                  if (value[i].isMember(ALPHABETIC) &&
                      value[i][ALPHABETIC].type() == Json::stringValue)
                  {
                    s += value[i][ALPHABETIC].asString();
                  }

                  bool hasIdeographic = false;
                  
                  if (value[i].isMember(IDEOGRAPHIC) &&
                      value[i][IDEOGRAPHIC].type() == Json::stringValue)
                  {
                    s += '=' + value[i][IDEOGRAPHIC].asString();
                    hasIdeographic = true;
                  }
                  
                  if (value[i].isMember(PHONETIC) &&
                      value[i][PHONETIC].type() == Json::stringValue)
                  {
                    if (!hasIdeographic)
                    {
                      s += '=';
                    }
                      
                    s += '=' + value[i][PHONETIC].asString();
                  }
                }
                else
                {
                  // This is the case of sequences
                  supported = false;
                }

                break;
            
              case Json::stringValue:
                s += value[i].asString();
                break;
              
              case Json::intValue:
                s += boost::lexical_cast<std::string>(value[i].asInt());
                break;
              
              case Json::uintValue:
                s += boost::lexical_cast<std::string>(value[i].asUInt());
                break;
              
              case Json::realValue:
                s += boost::lexical_cast<std::string>(value[i].asDouble());
                break;
              
              default:
                break;
            }
          }

          if (supported)
          {
            SetValue(tag, s, false /* not binary */);
          }
        }
      }
    }
  }


  std::string DicomMap::GetStringValue(const DicomTag& tag,
                                       const std::string& defaultValue,
                                       bool allowBinary) const
  {
    std::string s;
    if (LookupStringValue(s, tag, allowBinary))
    {
      return s;
    }
    else
    {
      return defaultValue;
    }
  }


  void DicomMap::RemoveBinaryTags()
  {
    Content kept;

    for (Content::iterator it = content_.begin(); it != content_.end(); ++it)
    {
      assert(it->second != NULL);

      if (!it->second->IsBinary() &&
          !it->second->IsNull())
      {
        kept[it->first] = it->second;
      }
      else
      {
        delete it->second;
      }
    }

    content_ = kept;
  }


  void DicomMap::DumpMainDicomTags(Json::Value& target,
                                   ResourceType level) const
  {
    std::map<DicomTag, std::string> mainTags;   // TODO - Create a singleton to hold this map
    LoadMainDicomTags(mainTags, level);
    
    target = Json::objectValue;

    for (Content::const_iterator it = content_.begin(); it != content_.end(); ++it)
    {
      assert(it->second != NULL);
      
      if (!it->second->IsBinary() &&
          !it->second->IsNull())
      {
        std::map<DicomTag, std::string>::const_iterator found = mainTags.find(it->first);

        if (found != mainTags.end())
        {
          target[found->second] = it->second->GetContent();
        }
      }
    }    
  }
  

  void DicomMap::ParseMainDicomTags(const Json::Value& source,
                                    ResourceType level)
  {
    if (source.type() != Json::objectValue)
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }
    
    std::map<std::string, DicomTag2> mainTags;   // TODO - Create a singleton to hold this map
    LoadMainDicomTags(mainTags, level);
    
    Json::Value::Members members = source.getMemberNames();
    for (size_t i = 0; i < members.size(); i++)
    {
      std::map<std::string, DicomTag2>::const_iterator found = mainTags.find(members[i]);

      if (found != mainTags.end())
      {
        const Json::Value& value = source[members[i]];
        if (value.type() != Json::stringValue)
        {
          throw OrthancException(ErrorCode_BadFileFormat);
        }
        else
        {
          SetValue(found->second, value.asString(), false);
        }
      }
    }
  }


  void DicomMap::Print(FILE* fp) const
  {
    DicomArray a(*this);
    a.Print(fp);
  }
}
