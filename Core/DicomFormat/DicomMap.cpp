/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2018 Osimis S.A., Belgium
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

#include "../Endianness.h"
#include "../Logging.h"
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
    DICOM_TAG_STUDY_DATE,
    DicomTag(0x0008, 0x0030),   // StudyTime
    DicomTag(0x0020, 0x0010),   // StudyID
    DICOM_TAG_STUDY_DESCRIPTION,
    DICOM_TAG_ACCESSION_NUMBER,
    DICOM_TAG_STUDY_INSTANCE_UID,
    DICOM_TAG_REQUESTED_PROCEDURE_DESCRIPTION,   // New in db v6
    DICOM_TAG_INSTITUTION_NAME,                  // New in db v6
    DICOM_TAG_REQUESTING_PHYSICIAN,              // New in db v6
    DICOM_TAG_REFERRING_PHYSICIAN_NAME           // New in db v6
  };

  static DicomTag seriesTags[] =
  {
    //DicomTag(0x0010, 0x1080), // MilitaryRank
    DicomTag(0x0008, 0x0021),   // SeriesDate
    DicomTag(0x0008, 0x0031),   // SeriesTime
    DICOM_TAG_MODALITY,
    DicomTag(0x0008, 0x0070),   // Manufacturer
    DicomTag(0x0008, 0x1010),   // StationName
    DICOM_TAG_SERIES_DESCRIPTION,
    DicomTag(0x0018, 0x0015),   // BodyPartExamined
    DicomTag(0x0018, 0x0024),   // SequenceName
    DicomTag(0x0018, 0x1030),   // ProtocolName
    DicomTag(0x0020, 0x0011),   // SeriesNumber
    DICOM_TAG_CARDIAC_NUMBER_OF_IMAGES,
    DICOM_TAG_IMAGES_IN_ACQUISITION,
    DICOM_TAG_NUMBER_OF_TEMPORAL_POSITIONS,
    DICOM_TAG_NUMBER_OF_SLICES,
    DICOM_TAG_NUMBER_OF_TIME_SLICES,
    DICOM_TAG_SERIES_INSTANCE_UID,
    DICOM_TAG_IMAGE_ORIENTATION_PATIENT,                  // New in db v6
    DICOM_TAG_SERIES_TYPE,                                // New in db v6
    DICOM_TAG_OPERATOR_NAME,                              // New in db v6
    DICOM_TAG_PERFORMED_PROCEDURE_STEP_DESCRIPTION,       // New in db v6
    DICOM_TAG_ACQUISITION_DEVICE_PROCESSING_DESCRIPTION,  // New in db v6
    DICOM_TAG_CONTRAST_BOLUS_AGENT                        // New in db v6
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
    DICOM_TAG_SOP_INSTANCE_UID,
    DICOM_TAG_IMAGE_POSITION_PATIENT,    // New in db v6
    DICOM_TAG_IMAGE_COMMENTS             // New in db v6
  };


  void DicomMap::LoadMainDicomTags(const DicomTag*& tags,
                                   size_t& size,
                                   ResourceType level)
  {
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
  }


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
      result.SetValue(tags[i], "", false);
    }
  }

  void DicomMap::SetupFindPatientTemplate(DicomMap& result)
  {
    SetupFindTemplate(result, patientTags, sizeof(patientTags) / sizeof(DicomTag));
  }

  void DicomMap::SetupFindStudyTemplate(DicomMap& result)
  {
    SetupFindTemplate(result, studyTags, sizeof(studyTags) / sizeof(DicomTag));
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
    SetupFindTemplate(result, seriesTags, sizeof(seriesTags) / sizeof(DicomTag));
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
    SetupFindTemplate(result, instanceTags, sizeof(instanceTags) / sizeof(DicomTag));
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


  void DicomMap::GetTags(std::set<DicomTag>& tags) const
  {
    tags.clear();

    for (Map::const_iterator it = map_.begin();
         it != map_.end(); ++it)
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


  bool DicomMap::ParseDicomMetaInformation(DicomMap& result,
                                           const char* dicom,
                                           size_t size)
  {
    /**
     * http://dicom.nema.org/medical/dicom/current/output/chtml/part10/chapter_7.html
     * According to Table 7.1-1, besides the "DICM" DICOM prefix, the
     * file preamble (i.e. dicom[0..127]) should not be taken into
     * account to determine whether the file is or is not a DICOM file.
     **/

    if (size < 132 ||
        dicom[128] != 'D' ||
        dicom[129] != 'I' ||
        dicom[130] != 'C' ||
        dicom[131] != 'M')
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


  bool DicomMap::CopyToString(std::string& result,
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
}
