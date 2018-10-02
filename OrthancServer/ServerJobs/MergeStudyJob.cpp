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


#include "MergeStudyJob.h"

#include "../../Core/DicomParsing/FromDcmtkBridge.h"
#include "../../Core/Logging.h"
#include "../../Core/SerializationToolbox.h"


namespace Orthanc
{
  void MergeStudyJob::AddSourceSeriesInternal(const std::string& series)
  {
    // Generate a target SeriesInstanceUID for this series
    seriesUidMap_[series] = FromDcmtkBridge::GenerateUniqueIdentifier(ResourceType_Series);

    // Add all the instances of the series as to be processed
    std::list<std::string> instances;
    context_.GetIndex().GetChildren(instances, series);

    for (std::list<std::string>::const_iterator
           it = instances.begin(); it != instances.end(); ++it)
    {
      AddInstance(*it);
    }
  }


  void MergeStudyJob::AddSourceStudyInternal(const std::string& study)
  {
    if (study == targetStudy_)
    {
      LOG(ERROR) << "Cannot merge a study into the same study: " << study;
      throw OrthancException(ErrorCode_UnknownResource);
    }
    else
    {
      std::list<std::string> series;
      context_.GetIndex().GetChildren(series, study);

      for (std::list<std::string>::const_iterator
             it = series.begin(); it != series.end(); ++it)
      {
        AddSourceSeriesInternal(*it);
      }
    }
  }


  bool MergeStudyJob::HandleInstance(const std::string& instance)
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

    std::string series = modified->GetHasher().HashSeries();

    SeriesUidMap::const_iterator targetSeriesUid = seriesUidMap_.find(series);

    if (targetSeriesUid == seriesUidMap_.end())
    {
      throw OrthancException(ErrorCode_BadFileFormat);  // Should never happen
    }


    /**
     * Copy the tags from the "Patient Module Attributes" and "General
     * Study Module Attributes" modules of the target study
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
    
    modified->ReplacePlainString(DICOM_TAG_SERIES_INSTANCE_UID, targetSeriesUid->second);

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

  
  bool MergeStudyJob::HandleTrailingStep()
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

  
  MergeStudyJob::MergeStudyJob(ServerContext& context,
                               const std::string& targetStudy) :
    SetOfInstancesJob(true /* with trailing step */),
    context_(context),
    keepSource_(false),
    targetStudy_(targetStudy)
  {
    /**
     * Check the validity of the input ID
     **/
    
    ResourceType type;

    if (!context_.GetIndex().LookupResourceType(type, targetStudy) ||
        type != ResourceType_Study)
    {
      LOG(ERROR) << "Cannot merge into an unknown study: " << targetStudy;
      throw OrthancException(ErrorCode_UnknownResource);
    }


    /**
     * Detect the tags to be removed/replaced by parsing one child
     * instance of the study
     **/

    DicomTag::AddTagsForModule(removals_, DicomModule_Patient);
    DicomTag::AddTagsForModule(removals_, DicomModule_Study);
    
    std::list<std::string> instances;
    context_.GetIndex().GetChildInstances(instances, targetStudy);
    
    if (instances.empty())
    {
      throw OrthancException(ErrorCode_UnknownResource);
    }

    DicomMap dicom;

    {
      ServerContext::DicomCacheLocker locker(context_, instances.front());
      locker.GetDicom().ExtractDicomSummary(dicom);
    }

    const std::set<DicomTag> moduleTags = removals_;
    for (std::set<DicomTag>::const_iterator it = moduleTags.begin();
         it != moduleTags.end(); ++it)
    {
      const DicomValue* value = dicom.TestAndGetValue(*it);
      std::string str;
      
      if (value != NULL &&
          value->CopyToString(str, false))
      {
        removals_.erase(*it);
        replacements_.insert(std::make_pair(*it, str));
      }
    }
  }
  

  void MergeStudyJob::SetOrigin(const DicomInstanceOrigin& origin)
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

  
  void MergeStudyJob::SetOrigin(const RestApiCall& call)
  {
    SetOrigin(DicomInstanceOrigin::FromRest(call));
  }


  void MergeStudyJob::AddSource(const std::string& studyOrSeries)
  {
    ResourceType level;
    
    if (IsStarted())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else if (!context_.GetIndex().LookupResourceType(level, studyOrSeries))
    {
      LOG(ERROR) << "Cannot find this resource: " << studyOrSeries;
      throw OrthancException(ErrorCode_UnknownResource);
    }
    else
    {
      switch (level)
      {
        case ResourceType_Study:
          AddSourceStudyInternal(studyOrSeries);
          break;
          
        case ResourceType_Series:
          AddSourceSeries(studyOrSeries);
          break;
          
        default:
          LOG(ERROR) << "This resource is neither a study, nor a series: "
                     << studyOrSeries << " is a " << EnumerationToString(level);
          throw OrthancException(ErrorCode_UnknownResource);
      }
    }    
  }
  

  void MergeStudyJob::AddSourceSeries(const std::string& series)
  {
    std::string parent;

    if (IsStarted())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else if (!context_.GetIndex().LookupParent(parent, series, ResourceType_Study))
    {
      LOG(ERROR) << "This resource is not a series: " << series;
      throw OrthancException(ErrorCode_UnknownResource);
    }
    else if (parent == targetStudy_)
    {
      LOG(ERROR) << "Cannot merge series " << series
                 << " into its parent study " << targetStudy_;
      throw OrthancException(ErrorCode_UnknownResource);
    }
    else
    {
      AddSourceSeriesInternal(series);
    }    
  }


  void MergeStudyJob::AddSourceStudy(const std::string& study)
  {
    ResourceType actualLevel;

    if (IsStarted())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else if (!context_.GetIndex().LookupResourceType(actualLevel, study) ||
             actualLevel != ResourceType_Study)
    {
      LOG(ERROR) << "This resource is not a study: " << study;
      throw OrthancException(ErrorCode_UnknownResource);
    }
    else
    {
      AddSourceStudyInternal(study);
    }    
  }


  void MergeStudyJob::SetKeepSource(bool keep)
  {
    if (IsStarted())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    keepSource_ = keep;
  }


  void MergeStudyJob::GetPublicContent(Json::Value& value)
  {
    SetOfInstancesJob::GetPublicContent(value);
    value["TargetStudy"] = targetStudy_;
  }


  static const char* KEEP_SOURCE = "KeepSource";
  static const char* TARGET_STUDY = "TargetStudy";
  static const char* REPLACEMENTS = "Replacements";
  static const char* REMOVALS = "Removals";
  static const char* SERIES_UID_MAP = "SeriesUIDMap";
  static const char* ORIGIN = "Origin";


  MergeStudyJob::MergeStudyJob(ServerContext& context,
                               const Json::Value& serialized) :
    SetOfInstancesJob(serialized),  // (*)
    context_(context)
  {
    if (!HasTrailingStep())
    {
      // Should have been set by (*)
      throw OrthancException(ErrorCode_InternalError);
    }

    keepSource_ = SerializationToolbox::ReadBoolean(serialized, KEEP_SOURCE);
    targetStudy_ = SerializationToolbox::ReadString(serialized, TARGET_STUDY);
    SerializationToolbox::ReadMapOfTags(replacements_, serialized, REPLACEMENTS);
    SerializationToolbox::ReadSetOfTags(removals_, serialized, REMOVALS);
    SerializationToolbox::ReadMapOfStrings(seriesUidMap_, serialized, SERIES_UID_MAP);
    origin_ = DicomInstanceOrigin(serialized[ORIGIN]);
  }

  
  bool MergeStudyJob::Serialize(Json::Value& target)
  {
    if (!SetOfInstancesJob::Serialize(target))
    {
      return false;
    }
    else
    {
      target[KEEP_SOURCE] = keepSource_;
      target[TARGET_STUDY] = targetStudy_;
      SerializationToolbox::WriteMapOfTags(target, replacements_, REPLACEMENTS);
      SerializationToolbox::WriteSetOfTags(target, removals_, REMOVALS);
      SerializationToolbox::WriteMapOfStrings(target, seriesUidMap_, SERIES_UID_MAP);
      origin_.Serialize(target[ORIGIN]);

      return true;
    }
  }
}
