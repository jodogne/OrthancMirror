/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#include "SplitStudyJob.h"

#include "../../../OrthancFramework/Sources/DicomParsing/FromDcmtkBridge.h"
#include "../../../OrthancFramework/Sources/Logging.h"
#include "../../../OrthancFramework/Sources/SerializationToolbox.h"
#include "../ServerContext.h"


namespace Orthanc
{
  void SplitStudyJob::CheckAllowedTag(const DicomTag& tag) const
  {
    if (allowedTags_.find(tag) == allowedTags_.end())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange,
                             "Cannot modify the following tag while splitting a study "
                             "(not in the patient/study modules): " +
                             FromDcmtkBridge::GetTagName(tag, "") +
                             " (" + tag.Format() + ")");
    }
  }

  
  void SplitStudyJob::Setup()
  {
    SetPermissive(false);
    
    DicomTag::AddTagsForModule(allowedTags_, DicomModule_Patient);
    DicomTag::AddTagsForModule(allowedTags_, DicomModule_Study);
    allowedTags_.erase(DICOM_TAG_STUDY_INSTANCE_UID);
    allowedTags_.erase(DICOM_TAG_SERIES_INSTANCE_UID);
  }

  
  bool SplitStudyJob::HandleInstance(const std::string& instance)
  {
    if (!HasTrailingStep())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls,
                             "AddTrailingStep() should have been called after AddSourceSeries()");
    }
    
    /**
     * Retrieve the DICOM instance to be modified
     **/
    
    std::unique_ptr<ParsedDicomFile> modified;

    try
    {
      ServerContext::DicomCacheLocker locker(GetContext(), instance);
      modified.reset(locker.GetDicom().Clone(true));
    }
    catch (OrthancException&)
    {
      LOG(WARNING) << "An instance was removed after the job was issued: " << instance;
      return false;
    }


    /**
     * Chose the target UIDs
     **/

    assert(modified->GetHasher().HashStudy() == sourceStudy_);

    std::string series = modified->GetHasher().HashSeries();

    SeriesUidMap::const_iterator targetSeriesUid = seriesUidMap_.find(series);

    if (targetSeriesUid == seriesUidMap_.end())
    {
      throw OrthancException(ErrorCode_BadFileFormat);  // Should never happen
    }


    /**
     * Apply user-specified modifications
     **/

    for (std::set<DicomTag>::const_iterator it = removals_.begin();
         it != removals_.end(); ++it)
    {
      modified->Remove(*it);
    }
    
    for (Replacements::const_iterator it = replacements_.begin();
         it != replacements_.end(); ++it)
    {
      modified->ReplacePlainString(it->first, it->second);
    }


    /**
     * Store the new instance into Orthanc
     **/
    
    modified->ReplacePlainString(DICOM_TAG_STUDY_INSTANCE_UID, targetStudyUid_);
    modified->ReplacePlainString(DICOM_TAG_SERIES_INSTANCE_UID, targetSeriesUid->second);

    // Fix since Orthanc 1.5.8: Assign new "SOPInstanceUID", as the instance has been modified
    modified->ReplacePlainString(DICOM_TAG_SOP_INSTANCE_UID, FromDcmtkBridge::GenerateUniqueIdentifier(ResourceType_Instance));

    if (targetStudy_.empty())
    {
      targetStudy_ = modified->GetHasher().HashStudy();
    }
    
    std::unique_ptr<DicomInstanceToStore> toStore(DicomInstanceToStore::CreateFromParsedDicomFile(*modified));
    toStore->SetOrigin(origin_);

    std::string modifiedInstance;
    ServerContext::StoreResult result = GetContext().Store(modifiedInstance, *toStore, StoreInstanceMode_Default);
    if (result.GetStatus() != StoreStatus_Success)
    {
      LOG(ERROR) << "Error while storing a modified instance " << instance;
      return false;
    }

    return true;
  }

  
  SplitStudyJob::SplitStudyJob(ServerContext& context,
                               const std::string& sourceStudy) :
    CleaningInstancesJob(context, false /* by default, remove source instances */),
    sourceStudy_(sourceStudy),
    targetStudyUid_(FromDcmtkBridge::GenerateUniqueIdentifier(ResourceType_Study))
  {
    Setup();
    
    ResourceType type;
    
    if (!GetContext().GetIndex().LookupResourceType(type, sourceStudy) ||
        type != ResourceType_Study)
    {
      throw OrthancException(ErrorCode_UnknownResource,
                             "Cannot split unknown study " + sourceStudy);
    }
  }
  

  void SplitStudyJob::SetOrigin(const DicomInstanceOrigin& origin)
  {
    if (IsStarted())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      origin_ = origin;
    }
  }

  
  void SplitStudyJob::SetOrigin(const RestApiCall& call)
  {
    SetOrigin(DicomInstanceOrigin::FromRest(call));
  }


  static void RegisterSeries(std::map<std::string, std::string>& target,
                             const std::string& series)
  {
    // Generate a target SeriesInstanceUID for this series
    if (target.find(series) == target.end())
    {
      target[series] = FromDcmtkBridge::GenerateUniqueIdentifier(ResourceType_Series);
    }
  }
  

  void SplitStudyJob::AddSourceSeries(const std::string& series)
  {
    std::string parent;

    if (IsStarted())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else if (!GetContext().GetIndex().LookupParent(parent, series, ResourceType_Study) ||
             parent != sourceStudy_)
    {
      throw OrthancException(ErrorCode_UnknownResource,
                             "This series does not belong to the study to be split: " + series);
    }
    else
    {
      RegisterSeries(seriesUidMap_, series);

      // Add all the instances of the series as to be processed
      std::list<std::string> instances;
      GetContext().GetIndex().GetChildren(instances, series);

      for (std::list<std::string>::const_iterator
             it = instances.begin(); it != instances.end(); ++it)
      {
        AddInstance(*it);
      }
    }    
  }


  void SplitStudyJob::AddSourceInstance(const std::string& instance)
  {
    std::string study, series;

    if (IsStarted())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else if (!GetContext().GetIndex().LookupParent(series, instance, ResourceType_Series) ||
             !GetContext().GetIndex().LookupParent(study, series, ResourceType_Study) ||
             study != sourceStudy_)
    {
      throw OrthancException(ErrorCode_UnknownResource,
                             "This instance does not belong to the study to be split: " + instance);
    }
    else
    {
      RegisterSeries(seriesUidMap_, series);
      AddInstance(instance);
    }    
  }


  bool SplitStudyJob::LookupTargetSeriesUid(std::string& uid,
                                            const std::string& series) const
  {
    SeriesUidMap::const_iterator found = seriesUidMap_.find(series);

    if (found == seriesUidMap_.end())
    {
      return false;
    }
    else
    {
      uid = found->second;
      return true;
    }
  }


  void SplitStudyJob::Remove(const DicomTag& tag)
  {
    if (IsStarted())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    CheckAllowedTag(tag);
    removals_.insert(tag);
  }

  
  void SplitStudyJob::Replace(const DicomTag& tag,
                              const std::string& value)
  {
    if (IsStarted())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    CheckAllowedTag(tag);
    replacements_[tag] = value;
  }


  bool SplitStudyJob::LookupReplacement(std::string& value,
                                        const DicomTag& tag) const
  {
    Replacements::const_iterator found = replacements_.find(tag);

    if (found == replacements_.end())
    {
      return false;
    }
    else
    {
      value = found->second;
      return true;
    }
  }
  
    
  void SplitStudyJob::GetPublicContent(Json::Value& value)
  {
    CleaningInstancesJob::GetPublicContent(value);

    if (!targetStudy_.empty())
    {
      value["TargetStudy"] = targetStudy_;
    }
    
    value["TargetStudyUID"] = targetStudyUid_;
  }


  static const char* SOURCE_STUDY = "SourceStudy";
  static const char* TARGET_STUDY = "TargetStudy";
  static const char* TARGET_STUDY_UID = "TargetStudyUID";
  static const char* SERIES_UID_MAP = "SeriesUIDMap";
  static const char* ORIGIN = "Origin";
  static const char* REPLACEMENTS = "Replacements";
  static const char* REMOVALS = "Removals";


  SplitStudyJob::SplitStudyJob(ServerContext& context,
                               const Json::Value& serialized) :
    CleaningInstancesJob(context, serialized,
                         false /* by default, remove source instances */)  // (*)
  {
    if (!HasTrailingStep())
    {
      // Should have been set by (*)
      throw OrthancException(ErrorCode_InternalError);
    }

    Setup();

    sourceStudy_ = SerializationToolbox::ReadString(serialized, SOURCE_STUDY);
    targetStudy_ = SerializationToolbox::ReadString(serialized, TARGET_STUDY);
    targetStudyUid_ = SerializationToolbox::ReadString(serialized, TARGET_STUDY_UID);
    SerializationToolbox::ReadMapOfStrings(seriesUidMap_, serialized, SERIES_UID_MAP);
    origin_ = DicomInstanceOrigin(serialized[ORIGIN]);
    SerializationToolbox::ReadMapOfTags(replacements_, serialized, REPLACEMENTS);
    SerializationToolbox::ReadSetOfTags(removals_, serialized, REMOVALS);
  }

  
  bool SplitStudyJob::Serialize(Json::Value& target)
  {
    if (!CleaningInstancesJob::Serialize(target))
    {
      return false;
    }
    else
    {
      target[SOURCE_STUDY] = sourceStudy_;
      target[TARGET_STUDY] = targetStudy_;
      target[TARGET_STUDY_UID] = targetStudyUid_;
      SerializationToolbox::WriteMapOfStrings(target, seriesUidMap_, SERIES_UID_MAP);
      origin_.Serialize(target[ORIGIN]);
      SerializationToolbox::WriteMapOfTags(target, replacements_, REPLACEMENTS);
      SerializationToolbox::WriteSetOfTags(target, removals_, REMOVALS);

      return true;
    }
  }
}
