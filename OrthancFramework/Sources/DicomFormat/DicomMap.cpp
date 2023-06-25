/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2021-2023 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 **/


#include "../PrecompiledHeaders.h"
#include "DicomMap.h"

#include <stdio.h>
#include <memory>
#include <boost/algorithm/string/join.hpp>

#include "../Compatibility.h"
#include "../Endianness.h"
#include "../Logging.h"
#include "../OrthancException.h"
#include "../Toolbox.h"
#include "DicomArray.h"
#include "DicomImageInformation.h"

#if ORTHANC_ENABLE_DCMTK == 1
#include "../DicomParsing/FromDcmtkBridge.h"
#endif

#if !defined(__EMSCRIPTEN__)
// Multithreading is not supported in WebAssembly
#  include <boost/thread/shared_mutex.hpp>
#endif

namespace Orthanc
{
  // WARNING: the DEFAULT list of main dicom tags below are the list as they 
  // were in Orthanc 1.10 before we introduced the dynamic main dicom tags.
  // This list has not changed since Orthanc 1.4.2 and had a single change since
  // Orthanc 0.9.5.
  // These lists have a specific signature.  When a resource does not have
  // the metadata "MainDicomTagsSignature", we'll assume that they were stored
  // with an Orthanc prior to 1.11.  It is therefore very important that you never
  // change these lists !

  static const DicomTag DEFAULT_PATIENT_MAIN_DICOM_TAGS[] =
  {
    // { DicomTag(0x0010, 0x1010), "PatientAge" },
    // { DicomTag(0x0010, 0x1040), "PatientAddress" },
    DICOM_TAG_PATIENT_NAME,
    DICOM_TAG_PATIENT_BIRTH_DATE,
    DICOM_TAG_PATIENT_SEX,
    DICOM_TAG_OTHER_PATIENT_IDS,
    DICOM_TAG_PATIENT_ID
  };
  
  static const DicomTag DEFAULT_STUDY_MAIN_DICOM_TAGS[] =
  {
    // { DicomTag(0x0010, 0x1020), "PatientSize" },
    // { DicomTag(0x0010, 0x1030), "PatientWeight" },
    DICOM_TAG_STUDY_DATE,
    DICOM_TAG_STUDY_TIME,
    DICOM_TAG_STUDY_ID,
    DICOM_TAG_STUDY_DESCRIPTION,
    DICOM_TAG_ACCESSION_NUMBER,
    DICOM_TAG_STUDY_INSTANCE_UID,
    
    // New in db v6 (Orthanc 0.9.5)
    DICOM_TAG_REQUESTED_PROCEDURE_DESCRIPTION,
    DICOM_TAG_INSTITUTION_NAME,
    DICOM_TAG_REQUESTING_PHYSICIAN,
    DICOM_TAG_REFERRING_PHYSICIAN_NAME
  };

  static const DicomTag DEFAULT_SERIES_MAIN_DICOM_TAGS[] =
  {
    // { DicomTag(0x0010, 0x1080), "MilitaryRank" },
    DICOM_TAG_SERIES_DATE,
    DICOM_TAG_SERIES_TIME,
    DICOM_TAG_MODALITY,
    DICOM_TAG_MANUFACTURER,
    DICOM_TAG_STATION_NAME,
    DICOM_TAG_SERIES_DESCRIPTION,
    DICOM_TAG_BODY_PART_EXAMINED,
    DICOM_TAG_SEQUENCE_NAME,
    DICOM_TAG_PROTOCOL_NAME,
    DICOM_TAG_SERIES_NUMBER,
    DICOM_TAG_CARDIAC_NUMBER_OF_IMAGES,
    DICOM_TAG_IMAGES_IN_ACQUISITION,
    DICOM_TAG_NUMBER_OF_TEMPORAL_POSITIONS,
    DICOM_TAG_NUMBER_OF_SLICES,
    DICOM_TAG_NUMBER_OF_TIME_SLICES,
    DICOM_TAG_SERIES_INSTANCE_UID,

    // New in db v6 (Orthanc 0.9.5)
    DICOM_TAG_IMAGE_ORIENTATION_PATIENT,
    DICOM_TAG_SERIES_TYPE,
    DICOM_TAG_OPERATOR_NAME,
    DICOM_TAG_PERFORMED_PROCEDURE_STEP_DESCRIPTION,
    DICOM_TAG_ACQUISITION_DEVICE_PROCESSING_DESCRIPTION,
    DICOM_TAG_CONTRAST_BOLUS_AGENT
  };

  static const DicomTag DEFAULT_INSTANCE_MAIN_DICOM_TAGS[] =
  {
    DICOM_TAG_INSTANCE_CREATION_DATE,
    DICOM_TAG_INSTANCE_CREATION_TIME,
    DICOM_TAG_ACQUISITION_NUMBER,
    DICOM_TAG_IMAGE_INDEX,
    DICOM_TAG_INSTANCE_NUMBER,
    DICOM_TAG_NUMBER_OF_FRAMES,
    DICOM_TAG_TEMPORAL_POSITION_IDENTIFIER,
    DICOM_TAG_SOP_INSTANCE_UID,

    // New in db v6 (Orthanc 0.9.5)
    DICOM_TAG_IMAGE_POSITION_PATIENT,
    DICOM_TAG_IMAGE_COMMENTS,

    /**
     * Main DICOM tags that are not part of any release of the
     * database schema yet, and that will be part of future db v7. In
     * the meantime, the user must call "/tools/reconstruct" once to
     * access these tags if the corresponding DICOM files where
     * indexed in the database by an older version of Orthanc.
     **/
    DICOM_TAG_IMAGE_ORIENTATION_PATIENT  // New in Orthanc 1.4.2
  };

  class DicomMap::MainDicomTagsConfiguration : public boost::noncopyable
  {
  private:
#if !defined(__EMSCRIPTEN__)
    typedef boost::unique_lock<boost::shared_mutex> WriterLock;
    typedef boost::shared_lock<boost::shared_mutex> ReaderLock;

    boost::shared_mutex mutex_;
#endif
    
    std::set<DicomTag> patientsMainDicomTagsByLevel_;
    std::set<DicomTag> studiesMainDicomTagsByLevel_;
    std::set<DicomTag> seriesMainDicomTagsByLevel_;
    std::set<DicomTag> instancesMainDicomTagsByLevel_;

    std::set<DicomTag> allMainDicomTags_;

    std::map<ResourceType, std::string> signatures_;
    std::map<ResourceType, std::string> defaultSignatures_;

    MainDicomTagsConfiguration()
    {
      ResetDefaultMainDicomTags();
    }

    std::string ComputeSignature(const std::set<DicomTag>& tags)
    {
      // std::set are sorted by default (which is important for us !)
      std::set<std::string> tagsIds;
      for (std::set<DicomTag>::const_iterator it = tags.begin(); it != tags.end(); ++it)
      {
        tagsIds.insert(it->Format());
      }

      std::string signatureText = boost::algorithm::join(tagsIds, ";");

      return signatureText;
    }

    void LoadDefaultMainDicomTags(ResourceType level)
    {
      const DicomTag* tags = NULL;
      size_t size;

      switch (level)
      {
        case ResourceType_Patient:
          tags = DEFAULT_PATIENT_MAIN_DICOM_TAGS;
          size = sizeof(DEFAULT_PATIENT_MAIN_DICOM_TAGS) / sizeof(DicomTag);
          break;

        case ResourceType_Study:
          tags = DEFAULT_STUDY_MAIN_DICOM_TAGS;
          size = sizeof(DEFAULT_STUDY_MAIN_DICOM_TAGS) / sizeof(DicomTag);
          break;

        case ResourceType_Series:
          tags = DEFAULT_SERIES_MAIN_DICOM_TAGS;
          size = sizeof(DEFAULT_SERIES_MAIN_DICOM_TAGS) / sizeof(DicomTag);
          break;

        case ResourceType_Instance:
          tags = DEFAULT_INSTANCE_MAIN_DICOM_TAGS;
          size = sizeof(DEFAULT_INSTANCE_MAIN_DICOM_TAGS) / sizeof(DicomTag);
          break;

        default:
          throw OrthancException(ErrorCode_ParameterOutOfRange);
      }

      assert(tags != NULL &&
            size != 0);

      for (size_t i = 0; i < size; i++)
      {
        AddMainDicomTagInternal(tags[i], level);
      }
    }

    std::set<DicomTag>& GetMainDicomTagsByLevelInternal(ResourceType level)
    {
      switch (level)
      {
        case ResourceType_Patient:
          return patientsMainDicomTagsByLevel_;

        case ResourceType_Study:
          return studiesMainDicomTagsByLevel_;

        case ResourceType_Series:
          return seriesMainDicomTagsByLevel_;

        case ResourceType_Instance:
          return instancesMainDicomTagsByLevel_;

        default:
          throw OrthancException(ErrorCode_InternalError);
      }
    }

    void AddMainDicomTagInternal(const DicomTag& tag,
                                 ResourceType level)
    {
      std::set<DicomTag>& existingLevelTags = GetMainDicomTagsByLevelInternal(level);
      
      if (existingLevelTags.find(tag) != existingLevelTags.end())
      {
        throw OrthancException(ErrorCode_MainDicomTagsMultiplyDefined, tag.Format() + " is already defined");
      }

      existingLevelTags.insert(tag);
      allMainDicomTags_.insert(tag);

      signatures_[level] = ComputeSignature(GetMainDicomTagsByLevelInternal(level));
    }

  public:
    // Singleton pattern
    static MainDicomTagsConfiguration& GetInstance()
    {
      static MainDicomTagsConfiguration parameters;
      return parameters;
    }

    void ResetDefaultMainDicomTags()
    {
#if !defined(__EMSCRIPTEN__)
      WriterLock lock(mutex_);
#endif
      
      patientsMainDicomTagsByLevel_.clear();
      studiesMainDicomTagsByLevel_.clear();
      seriesMainDicomTagsByLevel_.clear();
      instancesMainDicomTagsByLevel_.clear();

      allMainDicomTags_.clear();

      // by default, initialize with the previous static list (up to 1.10.0)
      LoadDefaultMainDicomTags(ResourceType_Patient);
      LoadDefaultMainDicomTags(ResourceType_Study);
      LoadDefaultMainDicomTags(ResourceType_Series);
      LoadDefaultMainDicomTags(ResourceType_Instance);

      defaultSignatures_[ResourceType_Patient] = signatures_[ResourceType_Patient];
      defaultSignatures_[ResourceType_Study] = signatures_[ResourceType_Study];
      defaultSignatures_[ResourceType_Series] = signatures_[ResourceType_Series];
      defaultSignatures_[ResourceType_Instance] = signatures_[ResourceType_Instance];
    }

    void AddMainDicomTag(const DicomTag& tag,
                         ResourceType level)
    {
#if !defined(__EMSCRIPTEN__)
      WriterLock lock(mutex_);
#endif
      
      AddMainDicomTagInternal(tag, level);
    }

    void GetAllMainDicomTags(std::set<DicomTag>& target)
    {
#if !defined(__EMSCRIPTEN__)
      ReaderLock lock(mutex_);
#endif
      
      target = allMainDicomTags_;
    }

    void GetMainDicomTagsByLevel(std::set<DicomTag>& target,
                                 ResourceType level)
    {
#if !defined(__EMSCRIPTEN__)
      ReaderLock lock(mutex_);
#endif
      
      target = GetMainDicomTagsByLevelInternal(level);
    }

    std::string GetMainDicomTagsSignature(ResourceType level)
    {
#if !defined(__EMSCRIPTEN__)
      ReaderLock lock(mutex_);
#endif
      
      assert(signatures_.find(level) != signatures_.end());
      return signatures_[level];
    }

    std::string GetDefaultMainDicomTagsSignature(ResourceType level)
    {
#if !defined(__EMSCRIPTEN__)
      ReaderLock lock(mutex_);
#endif
      
      assert(defaultSignatures_.find(level) != defaultSignatures_.end());
      return defaultSignatures_[level];
    }

    bool IsMainDicomTag(const DicomTag& tag)
    {
#if !defined(__EMSCRIPTEN__)
      ReaderLock lock(mutex_);
#endif
      
      return allMainDicomTags_.find(tag) != allMainDicomTags_.end();
    }

    bool IsMainDicomTag(const DicomTag& tag,
                        ResourceType level)
    {
#if !defined(__EMSCRIPTEN__)
      ReaderLock lock(mutex_);
#endif
      
      const std::set<DicomTag>& mainDicomTags = GetMainDicomTagsByLevelInternal(level);
      return mainDicomTags.find(tag) != mainDicomTags.end();
    }
  };


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

  void DicomMap::SetNullValue(uint16_t group, uint16_t element)
  {
    SetValueInternal(group, element, new DicomValue);
  }

  void DicomMap::SetNullValue(const DicomTag &tag)
  {
    SetValueInternal(tag.GetGroup(), tag.GetElement(), new DicomValue);
  }

  void DicomMap::SetValue(uint16_t group, uint16_t element, const DicomValue &value)
  {
    SetValueInternal(group, element, value.Clone());
  }

  void DicomMap::SetValue(const DicomTag &tag, const DicomValue &value)
  {
    SetValueInternal(tag.GetGroup(), tag.GetElement(), value.Clone());
  }

  void DicomMap::SetValue(const DicomTag &tag, const std::string &str, bool isBinary)
  {
    SetValueInternal(tag.GetGroup(), tag.GetElement(), new DicomValue(str, isBinary));
  }

  void DicomMap::SetValue(uint16_t group, uint16_t element, const std::string &str, bool isBinary)
  {
    SetValueInternal(group, element, new DicomValue(str, isBinary));
  }

  void DicomMap::SetSequenceValue(const DicomTag& tag, const Json::Value& value)
  {
    SetValueInternal(tag.GetGroup(), tag.GetElement(), new DicomValue(value));
  }

  bool DicomMap::HasTag(uint16_t group, uint16_t element) const
  {
    return HasTag(DicomTag(group, element));
  }

  bool DicomMap::HasTag(const DicomTag &tag) const
  {
    return content_.find(tag) != content_.end();
  }

  const DicomValue &DicomMap::GetValue(uint16_t group, uint16_t element) const
  {
    return GetValue(DicomTag(group, element));
  }


  static void ExtractTagsInternal(DicomMap& result,
                                  const DicomMap::Content& source,
                                  const std::set<DicomTag>& mainDicomTags)
  {
    result.Clear();

    for (std::set<DicomTag>::const_iterator itmt = mainDicomTags.begin();
         itmt != mainDicomTags.end(); ++itmt)
    {
      DicomMap::Content::const_iterator it = source.find(*itmt);
      if (it != source.end())
      {
        result.SetValue(it->first, *it->second /* value will be cloned */);
      }
    }
  }

  void DicomMap::ExtractTags(DicomMap& result, const std::set<DicomTag>& tags) const
  {
    result.Clear();

    for (std::set<DicomTag>::const_iterator itmt = tags.begin();
         itmt != tags.end(); ++itmt)
    {
      DicomMap::Content::const_iterator it = content_.find(*itmt);
      if (it != content_.end())
      {
        result.SetValue(it->first, *it->second /* value will be cloned */);
      }
    }
  }

  void DicomMap::ExtractResourceInformation(DicomMap& result, ResourceType level) const
  {
    std::set<DicomTag> mainDicomTags;
    DicomMap::MainDicomTagsConfiguration::GetInstance().GetMainDicomTagsByLevel(mainDicomTags, level);
    ExtractTagsInternal(result, content_, mainDicomTags);
  }

  void DicomMap::ExtractPatientInformation(DicomMap& result) const
  {
    ExtractResourceInformation(result, ResourceType_Patient);
  }

  void DicomMap::ExtractStudyInformation(DicomMap& result) const
  {
    ExtractResourceInformation(result, ResourceType_Study);
  }

  void DicomMap::ExtractSeriesInformation(DicomMap& result) const
  {
    ExtractResourceInformation(result, ResourceType_Series);
  }

  void DicomMap::ExtractInstanceInformation(DicomMap& result) const
  {
    ExtractResourceInformation(result, ResourceType_Instance);
  }


  DicomMap::~DicomMap()
  {
    Clear();
  }

  size_t DicomMap::GetSize() const
  {
    return content_.size();
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

  const DicomValue *DicomMap::TestAndGetValue(uint16_t group, uint16_t element) const
  {
    return TestAndGetValue(DicomTag(group, element));
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

  void DicomMap::RemoveTags(const std::set<DicomTag>& tags) 
  {
    for (std::set<DicomTag>::const_iterator it = tags.begin(); it != tags.end(); ++it)
    {
      Remove(*it);
    }
  }

  void DicomMap::SetupFindPatientTemplate(DicomMap& result)
  {
    result.Clear();

    // Identifying tags
    result.SetValue(DICOM_TAG_PATIENT_ID, "", false);

    // Other tags in the "Patient" module
    result.SetValue(DICOM_TAG_OTHER_PATIENT_IDS, "", false);
    result.SetValue(DICOM_TAG_PATIENT_BIRTH_DATE, "", false);
    result.SetValue(DICOM_TAG_PATIENT_NAME, "", false);
    result.SetValue(DICOM_TAG_PATIENT_SEX, "", false);
  }

  void DicomMap::SetupFindStudyTemplate(DicomMap& result)
  {
    result.Clear();

    // Identifying tags
    result.SetValue(DICOM_TAG_PATIENT_ID, "", false);
    result.SetValue(DICOM_TAG_ACCESSION_NUMBER, "", false);
    result.SetValue(DICOM_TAG_STUDY_INSTANCE_UID, "", false);

    // Other tags in the "General Study" module
    result.SetValue(DICOM_TAG_REFERRING_PHYSICIAN_NAME, "", false);
    result.SetValue(DICOM_TAG_STUDY_DATE, "", false);
    result.SetValue(DICOM_TAG_STUDY_DESCRIPTION, "", false);
    result.SetValue(DICOM_TAG_STUDY_ID, "", false);
    result.SetValue(DICOM_TAG_STUDY_TIME, "", false);
  }

  void DicomMap::SetupFindSeriesTemplate(DicomMap& result)
  {
    result.Clear();

    // Identifying tags
    result.SetValue(DICOM_TAG_PATIENT_ID, "", false);
    result.SetValue(DICOM_TAG_ACCESSION_NUMBER, "", false);
    result.SetValue(DICOM_TAG_STUDY_INSTANCE_UID, "", false);
    result.SetValue(DICOM_TAG_SERIES_INSTANCE_UID, "", false);

    // Other tags in the "General Series" module
    result.SetValue(DICOM_TAG_BODY_PART_EXAMINED, "", false);
    result.SetValue(DICOM_TAG_MODALITY, "", false);
    result.SetValue(DICOM_TAG_OPERATOR_NAME, "", false);
    result.SetValue(DICOM_TAG_PERFORMED_PROCEDURE_STEP_DESCRIPTION, "", false);
    result.SetValue(DICOM_TAG_PROTOCOL_NAME, "", false);
    result.SetValue(DICOM_TAG_SERIES_DATE, "", false);
    result.SetValue(DICOM_TAG_SERIES_DESCRIPTION, "", false);
    result.SetValue(DICOM_TAG_SERIES_NUMBER, "", false);
    result.SetValue(DICOM_TAG_SERIES_TIME, "", false);
  }

  void DicomMap::SetupFindInstanceTemplate(DicomMap& result)
  {
    result.Clear();

    // Identifying tags
    result.SetValue(DICOM_TAG_PATIENT_ID, "", false);
    result.SetValue(DICOM_TAG_ACCESSION_NUMBER, "", false);
    result.SetValue(DICOM_TAG_STUDY_INSTANCE_UID, "", false);
    result.SetValue(DICOM_TAG_SERIES_INSTANCE_UID, "", false);
    result.SetValue(DICOM_TAG_SOP_INSTANCE_UID, "", false);

    // Other tags in the "SOP Common" module
    result.SetValue(DICOM_TAG_ACQUISITION_NUMBER, "", false);
    result.SetValue(DICOM_TAG_IMAGE_COMMENTS, "", false);
    result.SetValue(DICOM_TAG_IMAGE_INDEX, "", false);
    result.SetValue(DICOM_TAG_IMAGE_ORIENTATION_PATIENT, "", false);
    result.SetValue(DICOM_TAG_IMAGE_POSITION_PATIENT, "", false);
    result.SetValue(DICOM_TAG_INSTANCE_CREATION_DATE, "", false);
    result.SetValue(DICOM_TAG_INSTANCE_CREATION_TIME, "", false);
    result.SetValue(DICOM_TAG_INSTANCE_NUMBER, "", false);
    result.SetValue(DICOM_TAG_NUMBER_OF_FRAMES, "", false);
    result.SetValue(DICOM_TAG_TEMPORAL_POSITION_IDENTIFIER, "", false);
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
    return DicomMap::MainDicomTagsConfiguration::GetInstance().IsMainDicomTag(tag, level);
  }

  bool DicomMap::IsMainDicomTag(const DicomTag& tag)
  {
    return (IsMainDicomTag(tag, ResourceType_Patient) ||
            IsMainDicomTag(tag, ResourceType_Study) ||
            IsMainDicomTag(tag, ResourceType_Series) ||
            IsMainDicomTag(tag, ResourceType_Instance));
  }

  static bool IsGenericComputedTag(const DicomTag& tag)
  {
    return tag == DICOM_TAG_RETRIEVE_URL ||
      tag == DICOM_TAG_RETRIEVE_AE_TITLE;
  }

  bool DicomMap::IsComputedTag(const DicomTag& tag)
  {
    return (IsComputedTag(tag, ResourceType_Patient) ||
            IsComputedTag(tag, ResourceType_Study) ||
            IsComputedTag(tag, ResourceType_Series) ||
            IsComputedTag(tag, ResourceType_Instance) ||
            IsGenericComputedTag(tag));
  }

  bool DicomMap::IsComputedTag(const DicomTag& tag, ResourceType level)
  {

    switch (level)
    {
      case ResourceType_Patient:
        return (
          tag == DICOM_TAG_NUMBER_OF_PATIENT_RELATED_STUDIES ||
          tag == DICOM_TAG_NUMBER_OF_PATIENT_RELATED_SERIES ||
          tag == DICOM_TAG_NUMBER_OF_PATIENT_RELATED_INSTANCES
        );
      case ResourceType_Study:
        return (
          tag == DICOM_TAG_MODALITIES_IN_STUDY ||
          tag == DICOM_TAG_SOP_CLASSES_IN_STUDY ||
          tag == DICOM_TAG_NUMBER_OF_STUDY_RELATED_INSTANCES ||
          tag == DICOM_TAG_NUMBER_OF_STUDY_RELATED_SERIES
        );
      case ResourceType_Series:
        return (
          tag == DICOM_TAG_NUMBER_OF_SERIES_RELATED_INSTANCES
        );
      case ResourceType_Instance:
        return (
          tag == DICOM_TAG_INSTANCE_AVAILABILITY
        );
      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }

  bool DicomMap::HasOnlyComputedTags(const std::set<DicomTag>& tags)
  {
    if (tags.size() == 0)
    {
      return false;
    }

    for (std::set<DicomTag>::const_iterator it = tags.begin(); it != tags.end(); ++it)
    {
      if (!IsComputedTag(*it))
      {
        return false;
      }
    }
    return true;
  }

  bool DicomMap::HasComputedTags(const std::set<DicomTag>& tags)
  {
    for (std::set<DicomTag>::const_iterator it = tags.begin(); it != tags.end(); ++it)
    {
      if (IsComputedTag(*it))
      {
        return true;
      }
    }

    return false;
  }

  bool DicomMap::HasComputedTags(const std::set<DicomTag>& tags, ResourceType level)
  {
    for (std::set<DicomTag>::const_iterator it = tags.begin(); it != tags.end(); ++it)
    {
      if (IsComputedTag(*it, level))
      {
        return true;
      }
    }
    return false;
  }


  void DicomMap::GetMainDicomTags(std::set<DicomTag>& target,
                                  ResourceType level)
  {
    DicomMap::MainDicomTagsConfiguration::GetInstance().GetMainDicomTagsByLevel(target, level);
  }

  void DicomMap::GetAllMainDicomTags(std::set<DicomTag>& target)
  {
    DicomMap::MainDicomTagsConfiguration::GetInstance().GetAllMainDicomTags(target);
  }

  void DicomMap::AddMainDicomTag(const DicomTag& tag, ResourceType level)
  {
    DicomMap::MainDicomTagsConfiguration::GetInstance().AddMainDicomTag(tag, level);
  }

  void DicomMap::ResetDefaultMainDicomTags()
  {
    DicomMap::MainDicomTagsConfiguration::GetInstance().ResetDefaultMainDicomTags();
  }

  std::string DicomMap::GetMainDicomTagsSignature(ResourceType level)
  {
    return DicomMap::MainDicomTagsConfiguration::GetInstance().GetMainDicomTagsSignature(level);
  }

  std::string DicomMap::GetDefaultMainDicomTagsSignature(ResourceType level)
  {
    return DicomMap::MainDicomTagsConfiguration::GetInstance().GetDefaultMainDicomTagsSignature(level);
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


  static uint16_t ReadLittleEndianUint16(const char* dicom)
  {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(dicom);

    return (static_cast<uint16_t>(p[0]) |
            (static_cast<uint16_t>(p[1]) << 8));
  }


  static uint32_t ReadLittleEndianUint32(const char* dicom)
  {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(dicom);

    return (static_cast<uint32_t>(p[0]) |
            (static_cast<uint32_t>(p[1]) << 8) |
            (static_cast<uint32_t>(p[2]) << 16) |
            (static_cast<uint32_t>(p[3]) << 24));
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

    tag = DicomTag(ReadLittleEndianUint16(dicom + position),
                   ReadLittleEndianUint16(dicom + position + 2));

    vr = StringToValueRepresentation(std::string(dicom + position + 4, 2), true);
    if (vr == ValueRepresentation_NotSupported)
    {
      return false;
    }

    // http://dicom.nema.org/medical/dicom/current/output/chtml/part05/chapter_7.html#sect_7.1.2
    if (vr == ValueRepresentation_ApplicationEntity   /* AE */ ||
        vr == ValueRepresentation_AgeString           /* AS */ ||
        vr == ValueRepresentation_AttributeTag        /* AT */ ||
        vr == ValueRepresentation_CodeString          /* CS */ ||
        vr == ValueRepresentation_Date                /* DA */ ||
        vr == ValueRepresentation_DecimalString       /* DS */ ||
        vr == ValueRepresentation_DateTime            /* DT */ ||
        vr == ValueRepresentation_FloatingPointSingle /* FL */ ||
        vr == ValueRepresentation_FloatingPointDouble /* FD */ ||
        vr == ValueRepresentation_IntegerString       /* IS */ ||
        vr == ValueRepresentation_LongString          /* LO */ ||
        vr == ValueRepresentation_LongText            /* LT */ ||
        vr == ValueRepresentation_PersonName          /* PN */ ||
        vr == ValueRepresentation_ShortString         /* SH */ ||
        vr == ValueRepresentation_SignedLong          /* SL */ ||
        vr == ValueRepresentation_SignedShort         /* SS */ ||
        vr == ValueRepresentation_ShortText           /* ST */ ||
        vr == ValueRepresentation_Time                /* TM */ ||
        vr == ValueRepresentation_UniqueIdentifier    /* UI */ ||
        vr == ValueRepresentation_UnsignedLong        /* UL */ ||
        vr == ValueRepresentation_UnsignedShort       /* US */)
    {
      /**
       * This is Table 7.1-2. "Data Element with Explicit VR of AE,
       * AS, AT, CS, DA, DS, DT, FL, FD, IS, LO, LT, PN, SH, SL, SS,
       * ST, TM, UI, UL and US"
       **/
      if (position + 8 > size)
      {
        return false;
      }

      uint16_t length = ReadLittleEndianUint16(dicom + position + 6);
      if (position + 8 + length > size)
      {
        return false;
      }

      value.assign(dicom + position + 8, length);
      position += (8 + length);
    }
    else
    {
      /**
       * This is Table 7.1-1. "Data Element with Explicit VR other
       * than as shown in Table 7.1-2"
       **/
      if (position + 12 > size)
      {
        return false;
      }
      
      uint16_t reserved = ReadLittleEndianUint16(dicom + position + 6);
      if (reserved != 0)
      {
        return false;
      }

      uint32_t length = ReadLittleEndianUint32(dicom + position + 8);
      if (position + 12 + length > size)
      {
        return false;
      }

      value.assign(dicom + position + 12, length);
      position += (12 + length);
    }

    if (!ValidateTag(vr, value))
    {
      return false;
    }

    RemoveTagPadding(value, vr);

    return true;
  }


  bool DicomMap::IsDicomFile(const void* dicom,
                             size_t size)
  {
    /**
     * http://dicom.nema.org/medical/dicom/current/output/chtml/part10/chapter_7.html
     * According to Table 7.1-1, besides the "DICM" DICOM prefix, the
     * file preamble (i.e. dicom[0..127]) should not be taken into
     * account to determine whether the file is or is not a DICOM file.
     **/

    const uint8_t* p = reinterpret_cast<const uint8_t*>(dicom);

    return (size >= 132 &&
            p[128] == 'D' &&
            p[129] == 'I' &&
            p[130] == 'C' &&
            p[131] == 'M');
  }
    

  bool DicomMap::ParseDicomMetaInformation(DicomMap& result,
                                           const void* dicom,
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
    if (!ReadNextTag(tag, vr, value, reinterpret_cast<const char*>(dicom), size, position) ||
        tag.GetGroup() != 0x0002 ||
        tag.GetElement() != 0x0000 ||
        vr != ValueRepresentation_UnsignedLong ||
        value.size() != 4)
    {
      return false;
    }

    size_t stopPosition = position + ReadLittleEndianUint32(value.c_str());
    if (stopPosition > size)
    {
      return false;
    }

    while (position < stopPosition)
    {
      if (ReadNextTag(tag, vr, value, reinterpret_cast<const char*>(dicom), size, position))
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
    std::string patientId, studyInstanceUid, seriesInstanceUid, sopInstanceUid;
    
    if (HasTag(DICOM_TAG_PATIENT_ID))
    {
      patientId = ValueAsString(*this, DICOM_TAG_PATIENT_ID);
    }

    if (HasTag(DICOM_TAG_STUDY_INSTANCE_UID))
    {
      studyInstanceUid = ValueAsString(*this, DICOM_TAG_STUDY_INSTANCE_UID);
    }

    if (HasTag(DICOM_TAG_SERIES_INSTANCE_UID))
    {
      seriesInstanceUid = ValueAsString(*this, DICOM_TAG_SERIES_INSTANCE_UID);
    }

    if (HasTag(DICOM_TAG_SOP_INSTANCE_UID))
    {
      sopInstanceUid = ValueAsString(*this, DICOM_TAG_SOP_INSTANCE_UID);
    }

    LogMissingTagsForStore(patientId, studyInstanceUid, seriesInstanceUid, sopInstanceUid);
  }

  
  void DicomMap::LogMissingTagsForStore(const std::string& patientId,
                                        const std::string& studyInstanceUid,
                                        const std::string& seriesInstanceUid,
                                        const std::string& sopInstanceUid)
  {
    std::string s, t;

    if (!patientId.empty())
    {
      if (t.size() > 0)
        t += ", ";
      t += "PatientID=" + patientId;
    }
    else
    {
      if (s.size() > 0)
        s += ", ";
      s += "PatientID";
    }

    if (!studyInstanceUid.empty())
    {
      if (t.size() > 0)
        t += ", ";
      t += "StudyInstanceUID=" + studyInstanceUid;
    }
    else
    {
      if (s.size() > 0)
        s += ", ";
      s += "StudyInstanceUID";
    }

    if (!seriesInstanceUid.empty())
    {
      if (t.size() > 0)
        t += ", ";
      t += "SeriesInstanceUID=" + seriesInstanceUid;
    }
    else
    {
      if (s.size() > 0)
        s += ", ";
      s += "SeriesInstanceUID";
    }

    if (!sopInstanceUid.empty())
    {
      if (t.size() > 0)
        t += ", ";
      t += "SOPInstanceUID=" + sopInstanceUid;
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

  
  void DicomMap::FromDicomAsJson(const Json::Value& dicomAsJson, bool append, bool parseSequences)
  {
    if (dicomAsJson.type() != Json::objectValue)
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }
    
    if (!append)
    {
      Clear();
    }
    
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
      else if (value["Type"] == "Sequence" && parseSequences)
      {
        if (value["Value"].type() != Json::arrayValue)
        {
          throw OrthancException(ErrorCode_CorruptedFile);
        }
        else
        {
          SetSequenceValue(tag, value["Value"]);
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
    std::set<DicomTag> mainDicomTags;
    DicomMap::MainDicomTagsConfiguration::GetInstance().GetMainDicomTagsByLevel(mainDicomTags, level);

    for (std::set<DicomTag>::const_iterator itmt = mainDicomTags.begin();
         itmt != mainDicomTags.end(); ++itmt)
    {
      Content::const_iterator found = other.content_.find(*itmt);

      if (found != other.content_.end() &&
          content_.find(*itmt) == content_.end())
      {
        assert(found->second != NULL);
        content_[*itmt] = found->second->Clone();
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
    for (Content::const_iterator it = content_.begin(); it != content_.end(); ++it)
    {
      if (!DicomMap::MainDicomTagsConfiguration::GetInstance().IsMainDicomTag(it->first))
      {
        return false;
      }
    }

    return true;
  }

  void DicomMap::ExtractSequences(DicomMap& result) const
  {
    result.Clear();

    for (Content::const_iterator it = content_.begin(); it != content_.end(); ++it)
    {
      if (it->second->IsSequence())
      {
        result.SetSequenceValue(it->first, it->second->GetSequenceContent());
      }
    }
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
          for (Json::Value::ArrayIndex j = 0; j < value.size() && supported; j++)
          {
            if (!s.empty())
            {
              s += '\\';
            }

            switch (value[j].type())
            {
              case Json::objectValue:
                if (vr == ValueRepresentation_PersonName &&
                    value[j].type() == Json::objectValue)
                {
                  if (value[j].isMember(ALPHABETIC) &&
                      value[j][ALPHABETIC].type() == Json::stringValue)
                  {
                    s += value[j][ALPHABETIC].asString();
                  }

                  bool hasIdeographic = false;
                  
                  if (value[j].isMember(IDEOGRAPHIC) &&
                      value[j][IDEOGRAPHIC].type() == Json::stringValue)
                  {
                    s += '=' + value[j][IDEOGRAPHIC].asString();
                    hasIdeographic = true;
                  }
                  
                  if (value[j].isMember(PHONETIC) &&
                      value[j][PHONETIC].type() == Json::stringValue)
                  {
                    if (!hasIdeographic)
                    {
                      s += '=';
                    }
                      
                    s += '=' + value[j][PHONETIC].asString();
                  }
                }
                else
                {
                  // This is the case of sequences
                  supported = false;
                }

                break;
            
              case Json::stringValue:
                s += value[j].asString();
                break;
              
              case Json::intValue:
                s += boost::lexical_cast<std::string>(value[j].asInt64());
                break;
              
              case Json::uintValue:
                s += boost::lexical_cast<std::string>(value[j].asUInt64());
                break;
              
              case Json::realValue:
                s += boost::lexical_cast<std::string>(value[j].asDouble());
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


  void DicomMap::RemoveSequences()
  {
    Content kept;

    for (Content::iterator it = content_.begin(); it != content_.end(); ++it)
    {
      assert(it->second != NULL);

      if (!it->second->IsSequence())
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
    std::set<DicomTag> mainDicomTags;
    DicomMap::MainDicomTagsConfiguration::GetInstance().GetMainDicomTagsByLevel(mainDicomTags, level);
    
    target = Json::objectValue;

    for (Content::const_iterator it = content_.begin(); it != content_.end(); ++it)
    {
      assert(it->second != NULL);
      
      if (!it->second->IsBinary() &&
          !it->second->IsNull())
      {
        std::set<DicomTag>::const_iterator found = mainDicomTags.find(it->first);

        if (found != mainDicomTags.end())
        {
#if ORTHANC_ENABLE_DCMTK == 1
          target[FromDcmtkBridge::GetTagName(*found, "")] = it->second->GetContent();
#else
          target[found->Format()] = it->second->GetContent();
#endif
        }
      }
    }    
  }
  

  ValueRepresentation DicomMap::GuessPixelDataValueRepresentation(DicomTransferSyntax transferSyntax) const
  {
    const DicomValue* value = TestAndGetValue(DICOM_TAG_BITS_ALLOCATED);

    uint32_t bitsAllocated;
    if (value == NULL ||
        !value->ParseUnsignedInteger32(bitsAllocated))
    {
      bitsAllocated = 8;
    }

    return DicomImageInformation::GuessPixelDataValueRepresentation(transferSyntax, bitsAllocated);
  }
  

  void DicomMap::Print(FILE* fp) const
  {
    DicomArray a(*this);
    a.Print(fp);
  }
}
