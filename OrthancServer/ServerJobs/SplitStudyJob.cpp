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


#include "SplitStudyJob.h"

#include "../../Core/DicomParsing/FromDcmtkBridge.h"
#include "../../Core/Logging.h"
#include "../../Core/SerializationToolbox.h"

namespace Orthanc
{
  void SplitStudyJob::CheckAllowedTag(const DicomTag& tag) const
  {
    if (allowedTags_.find(tag) == allowedTags_.end())
    {
      LOG(ERROR) << "Cannot modify the following tag while splitting a study "
                 << "(not in the patient/study modules): "
                 << FromDcmtkBridge::GetTagName(tag, "") << " (" << tag.Format() << ")";
      throw OrthancException(ErrorCode_ParameterOutOfRange);
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
    /**
     * Retrieve the DICOM instance to be modified
     **/
    
    std::auto_ptr<ParsedDicomFile> modified;

    try
    {
      ServerContext::DicomCacheLocker locker(context_, instance);
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
    
    std::string series;
    if (!modified->GetTagValue(series, DICOM_TAG_SERIES_INSTANCE_UID))
    {
      throw OrthancException(ErrorCode_BadFileFormat);  // Should never happen
    }    

    std::string targetSeriesUid;
    SeriesUidMap::const_iterator found = targetSeries_.find(series);

    if (found == targetSeries_.end())
    {
      // Choose a random SeriesInstanceUID for this series
      targetSeriesUid = FromDcmtkBridge::GenerateUniqueIdentifier(ResourceType_Series);
      targetSeries_[series] = targetSeriesUid;
    }
    else
    {
      targetSeriesUid = found->second;
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
    modified->ReplacePlainString(DICOM_TAG_SERIES_INSTANCE_UID, targetSeriesUid);

    if (targetStudy_.empty())
    {
      targetStudy_ = modified->GetHasher().HashStudy();
    }
    
    DicomInstanceToStore toStore;
    toStore.SetOrigin(origin_);
    toStore.SetParsedDicomFile(*modified);

    std::string modifiedInstance;
    if (context_.Store(modifiedInstance, toStore) != StoreStatus_Success)
    {
      LOG(ERROR) << "Error while storing a modified instance " << instance;
      return false;
    }

    return true;
  }

  
  bool SplitStudyJob::HandleTrailingStep()
  {
    if (!keepSource_)
    {
      const size_t n = GetInstancesCount();

      for (size_t i = 0; i < n; i++)
      {
        Json::Value tmp;
        context_.DeleteResource(tmp, GetInstance(i), ResourceType_Instance);
      }
    }

    return true;
  }

  
  SplitStudyJob::SplitStudyJob(ServerContext& context,
                               const std::string& sourceStudy) :
    SetOfInstancesJob(true /* with trailing step */),
    context_(context),
    keepSource_(false),
    sourceStudy_(sourceStudy),
    targetStudyUid_(FromDcmtkBridge::GenerateUniqueIdentifier(ResourceType_Study))
  {
    Setup();
    
    ResourceType type;
    
    if (!context_.GetIndex().LookupResourceType(type, sourceStudy) ||
        type != ResourceType_Study)
    {
      LOG(ERROR) << "Cannot split unknown study: " << sourceStudy;
      throw OrthancException(ErrorCode_UnknownResource);
    }

    std::list<std::string> children;
    context_.GetIndex().GetChildren(children, sourceStudy);

    for (std::list<std::string>::const_iterator
           it = children.begin(); it != children.end(); ++it)
    {
      sourceSeries_.insert(*it);
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


  void SplitStudyJob::AddSourceSeries(const std::string& series)
  {
    if (IsStarted())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else if (sourceSeries_.find(series) == sourceSeries_.end())
    {
      LOG(ERROR) << "This series does not belong to the study to be split: " << series;
      throw OrthancException(ErrorCode_UnknownResource);
    }
    else
    {
      // Add all the instances of the series as to be processed
      std::list<std::string> instances;
      context_.GetIndex().GetChildren(instances, series);

      for (std::list<std::string>::const_iterator
             it = instances.begin(); it != instances.end(); ++it)
      {
        AddInstance(*it);
      }
    }    
  }


  void SplitStudyJob::SetKeepSource(bool keep)
  {
    if (IsStarted())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    keepSource_ = keep;
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
  
    
  void SplitStudyJob::GetPublicContent(Json::Value& value)
  {
    SetOfInstancesJob::GetPublicContent(value);

    if (!targetStudy_.empty())
    {
      value["TargetStudy"] = targetStudy_;
    }
    
    value["TargetStudyUID"] = targetStudyUid_;
  }


  static const char* SOURCE_STUDY = "SourceStudy";
  static const char* SOURCE_SERIES = "SourceSeries";
  static const char* KEEP_SOURCE = "KeepSource";
  static const char* TARGET_STUDY = "TargetStudy";
  static const char* TARGET_STUDY_UID = "TargetStudyUID";
  static const char* TARGET_SERIES = "TargetSeries";
  static const char* ORIGIN = "Origin";
  static const char* REPLACEMENTS = "Replacements";
  static const char* REMOVALS = "Removals";


  SplitStudyJob::SplitStudyJob(ServerContext& context,
                               const Json::Value& serialized) :
    SetOfInstancesJob(serialized),  // (*)
    context_(context)
  {
    if (!HasTrailingStep())
    {
      // Should have been set by (*)
      throw OrthancException(ErrorCode_InternalError);
    }

    Setup();

    keepSource_ = SerializationToolbox::ReadBoolean(serialized, KEEP_SOURCE);
    sourceStudy_ = SerializationToolbox::ReadString(serialized, SOURCE_STUDY);
    SerializationToolbox::ReadSetOfStrings(sourceSeries_, serialized, SOURCE_SERIES);
    targetStudy_ = SerializationToolbox::ReadString(serialized, TARGET_STUDY);
    targetStudyUid_ = SerializationToolbox::ReadString(serialized, TARGET_STUDY_UID);
    SerializationToolbox::ReadMapOfStrings(targetSeries_, serialized, TARGET_SERIES);
    origin_ = DicomInstanceOrigin(serialized[ORIGIN]);
    SerializationToolbox::ReadMapOfTags(replacements_, serialized, REPLACEMENTS);
    SerializationToolbox::ReadSetOfTags(removals_, serialized, REMOVALS);
  }

  
  bool SplitStudyJob::Serialize(Json::Value& target)
  {
    if (!SetOfInstancesJob::Serialize(target))
    {
      return false;
    }
    else
    {
      target[KEEP_SOURCE] = keepSource_;
      target[SOURCE_STUDY] = sourceStudy_;
      SerializationToolbox::WriteSetOfStrings(target, sourceSeries_, SOURCE_SERIES);
      target[TARGET_STUDY] = targetStudy_;
      target[TARGET_STUDY_UID] = targetStudyUid_;
      SerializationToolbox::WriteMapOfStrings(target, targetSeries_, TARGET_SERIES);
      origin_.Serialize(target[ORIGIN]);
      SerializationToolbox::WriteMapOfTags(target, replacements_, REPLACEMENTS);
      SerializationToolbox::WriteSetOfTags(target, removals_, REMOVALS);

      return true;
    }
  }
}
