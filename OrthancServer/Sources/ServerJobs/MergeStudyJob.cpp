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


#include "MergeStudyJob.h"

#include "../../../OrthancFramework/Sources/DicomParsing/FromDcmtkBridge.h"
#include "../../../OrthancFramework/Sources/Logging.h"
#include "../../../OrthancFramework/Sources/SerializationToolbox.h"
#include "../OrthancConfiguration.h"
#include "../ServerContext.h"


namespace Orthanc
{
  static void RegisterSeries(std::map<std::string, std::string>& target,
                             const std::string& series)
  {
    // Generate a target SeriesInstanceUID for this series
    if (target.find(series) == target.end())
    {
      target[series] = FromDcmtkBridge::GenerateUniqueIdentifier(ResourceType_Series);
    }
  }
  

  void MergeStudyJob::AddSourceSeriesInternal(const std::string& series)
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


  void MergeStudyJob::AddSourceStudyInternal(const std::string& study)
  {
    if (study == targetStudy_)
    {
      throw OrthancException(ErrorCode_UnknownResource,
                             "Cannot merge a study into the same study: " + study);
    }
    else
    {
      std::list<std::string> series;
      GetContext().GetIndex().GetChildren(series, study);

      for (std::list<std::string>::const_iterator
             it = series.begin(); it != series.end(); ++it)
      {
        AddSourceSeriesInternal(*it);
      }
    }
  }


  bool MergeStudyJob::HandleInstance(const std::string& instance)
  {
    if (!HasTrailingStep())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls,
                             "AddTrailingStep() should have been called after AddSourceXXX()");
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

    // Fix since Orthanc 1.5.8: Assign new "SOPInstanceUID", as the instance has been modified
    modified->ReplacePlainString(DICOM_TAG_SOP_INSTANCE_UID, FromDcmtkBridge::GenerateUniqueIdentifier(ResourceType_Instance));

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

  
  MergeStudyJob::MergeStudyJob(ServerContext& context,
                               const std::string& targetStudy) :
    CleaningInstancesJob(context, false /* by default, remove source instances */),
    targetStudy_(targetStudy)
  {
    /**
     * Check the validity of the input ID
     **/
    
    ResourceType type;

    if (!GetContext().GetIndex().LookupResourceType(type, targetStudy) ||
        type != ResourceType_Study)
    {
      throw OrthancException(ErrorCode_UnknownResource,
                             "Cannot merge into an unknown study: " + targetStudy);
    }


    /**
     * Detect the tags to be removed/replaced by parsing one child
     * instance of the study
     **/

    DicomTag::AddTagsForModule(removals_, DicomModule_Patient);
    DicomTag::AddTagsForModule(removals_, DicomModule_Study);
    
    std::list<std::string> instances;
    GetContext().GetIndex().GetChildInstances(instances, targetStudy);
    
    if (instances.empty())
    {
      throw OrthancException(ErrorCode_UnknownResource);
    }

    DicomMap dicom;

    {
      ServerContext::DicomCacheLocker locker(GetContext(), instances.front());
      OrthancConfiguration::DefaultExtractDicomSummary(dicom, locker.GetDicom());
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


  void MergeStudyJob::AddSource(const std::string& publicId)
  {
    ResourceType level;
    
    if (IsStarted())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else if (!GetContext().GetIndex().LookupResourceType(level, publicId))
    {
      throw OrthancException(ErrorCode_UnknownResource,
                             "Cannot find this resource: " + publicId);
    }
    else
    {
      switch (level)
      {
        case ResourceType_Study:
          AddSourceStudyInternal(publicId);
          break;
          
        case ResourceType_Series:
          AddSourceSeries(publicId);
          break;
          
        case ResourceType_Instance:
          AddSourceInstance(publicId);
          break;
          
        default:
          throw OrthancException(ErrorCode_UnknownResource,
                                 "This resource is neither a study, nor a series, nor an instance: " +
                                 publicId + " is a " + std::string(EnumerationToString(level)));
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
    else if (!GetContext().GetIndex().LookupParent(parent, series, ResourceType_Study))
    {
      throw OrthancException(ErrorCode_UnknownResource,
                             "This resource is not a series: " + series);
    }
    else if (parent == targetStudy_)
    {
      throw OrthancException(ErrorCode_UnknownResource,
                             "Cannot merge series " + series +
                             " into its parent study " + targetStudy_);
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
    else if (!GetContext().GetIndex().LookupResourceType(actualLevel, study) ||
             actualLevel != ResourceType_Study)
    {
      throw OrthancException(ErrorCode_UnknownResource,
                             "This resource is not a study: " + study);
    }
    else
    {
      AddSourceStudyInternal(study);
    }    
  }


  void MergeStudyJob::AddSourceInstance(const std::string& instance)
  {
    std::string parentStudy, parentSeries;

    if (IsStarted())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else if (!GetContext().GetIndex().LookupParent(parentSeries, instance, ResourceType_Series) ||
             !GetContext().GetIndex().LookupParent(parentStudy, parentSeries, ResourceType_Study))
    {
      throw OrthancException(ErrorCode_UnknownResource,
                             "This resource is not an instance: " + instance);
    }
    else if (parentStudy == targetStudy_)
    {
      throw OrthancException(ErrorCode_UnknownResource,
                             "Cannot merge instance " + instance +
                             " into its parent study " + targetStudy_);
    }
    else
    {
      RegisterSeries(seriesUidMap_, parentSeries);
      AddInstance(instance);
    }    
  }
  

  void MergeStudyJob::GetPublicContent(Json::Value& value)
  {
    CleaningInstancesJob::GetPublicContent(value);
    value["TargetStudy"] = targetStudy_;
  }


  static const char* TARGET_STUDY = "TargetStudy";
  static const char* REPLACEMENTS = "Replacements";
  static const char* REMOVALS = "Removals";
  static const char* SERIES_UID_MAP = "SeriesUIDMap";
  static const char* ORIGIN = "Origin";


  MergeStudyJob::MergeStudyJob(ServerContext& context,
                               const Json::Value& serialized) :
    CleaningInstancesJob(context, serialized,
                         false /* by default, remove source instances */)  // (*)
  {
    if (!HasTrailingStep())
    {
      // Should have been set by (*)
      throw OrthancException(ErrorCode_InternalError);
    }

    targetStudy_ = SerializationToolbox::ReadString(serialized, TARGET_STUDY);
    SerializationToolbox::ReadMapOfTags(replacements_, serialized, REPLACEMENTS);
    SerializationToolbox::ReadSetOfTags(removals_, serialized, REMOVALS);
    SerializationToolbox::ReadMapOfStrings(seriesUidMap_, serialized, SERIES_UID_MAP);
    origin_ = DicomInstanceOrigin(serialized[ORIGIN]);
  }

  
  bool MergeStudyJob::Serialize(Json::Value& target)
  {
    if (!CleaningInstancesJob::Serialize(target))
    {
      return false;
    }
    else
    {
      target[TARGET_STUDY] = targetStudy_;
      SerializationToolbox::WriteMapOfTags(target, replacements_, REPLACEMENTS);
      SerializationToolbox::WriteSetOfTags(target, removals_, REMOVALS);
      SerializationToolbox::WriteMapOfStrings(target, seriesUidMap_, SERIES_UID_MAP);
      origin_.Serialize(target[ORIGIN]);

      return true;
    }
  }
}
