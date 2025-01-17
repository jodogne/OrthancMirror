/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2025 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2025 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#include "PrecompiledHeadersServer.h"
#include "ResourceFinder.h"

#include "../../OrthancFramework/Sources/DicomParsing/FromDcmtkBridge.h"
#include "../../OrthancFramework/Sources/Logging.h"
#include "../../OrthancFramework/Sources/OrthancException.h"
#include "../../OrthancFramework/Sources/SerializationToolbox.h"
#include "OrthancConfiguration.h"
#include "Search/DatabaseLookup.h"
#include "ServerContext.h"
#include "ServerIndex.h"


namespace Orthanc
{
  static bool IsComputedTag(const DicomTag& tag)
  {
    return (tag == DICOM_TAG_NUMBER_OF_PATIENT_RELATED_STUDIES ||
            tag == DICOM_TAG_NUMBER_OF_PATIENT_RELATED_SERIES ||
            tag == DICOM_TAG_NUMBER_OF_PATIENT_RELATED_INSTANCES ||
            tag == DICOM_TAG_NUMBER_OF_STUDY_RELATED_SERIES ||
            tag == DICOM_TAG_NUMBER_OF_STUDY_RELATED_INSTANCES ||
            tag == DICOM_TAG_NUMBER_OF_SERIES_RELATED_INSTANCES ||
            tag == DICOM_TAG_SOP_CLASSES_IN_STUDY ||
            tag == DICOM_TAG_MODALITIES_IN_STUDY ||
            tag == DICOM_TAG_INSTANCE_AVAILABILITY);
  }

  void ResourceFinder::ConfigureChildrenCountComputedTag(DicomTag tag,
                                                         ResourceType parentLevel,
                                                         ResourceType childLevel)
  {
    if (request_.GetLevel() == parentLevel)
    {
      requestedComputedTags_.insert(tag);
      request_.GetChildrenSpecification(childLevel).SetRetrieveCount(true);
    }
  }


  void ResourceFinder::InjectChildrenCountComputedTag(DicomMap& requestedTags,
                                                      DicomTag tag,
                                                      const FindResponse::Resource& resource,
                                                      ResourceType level) const
  {
    if (IsRequestedComputedTag(tag))
    {
      requestedTags.SetValue(tag, boost::lexical_cast<std::string>(resource.GetChildrenCount(level)), false);
    }
  }


  void ResourceFinder::InjectComputedTags(DicomMap& requestedTags,
                                          const FindResponse::Resource& resource) const
  {
    switch (resource.GetLevel())
    {
      case ResourceType_Patient:
        InjectChildrenCountComputedTag(requestedTags, DICOM_TAG_NUMBER_OF_PATIENT_RELATED_STUDIES, resource, ResourceType_Study);
        InjectChildrenCountComputedTag(requestedTags, DICOM_TAG_NUMBER_OF_PATIENT_RELATED_SERIES, resource, ResourceType_Series);
        InjectChildrenCountComputedTag(requestedTags, DICOM_TAG_NUMBER_OF_PATIENT_RELATED_INSTANCES, resource, ResourceType_Instance);
        break;

      case ResourceType_Study:
        InjectChildrenCountComputedTag(requestedTags, DICOM_TAG_NUMBER_OF_STUDY_RELATED_SERIES, resource, ResourceType_Series);
        InjectChildrenCountComputedTag(requestedTags, DICOM_TAG_NUMBER_OF_STUDY_RELATED_INSTANCES, resource, ResourceType_Instance);

        if (IsRequestedComputedTag(DICOM_TAG_MODALITIES_IN_STUDY))
        {
          std::set<std::string> modalities;
          resource.GetChildrenMainDicomTagValues(modalities, ResourceType_Series, DICOM_TAG_MODALITY);

          std::string s;
          Toolbox::JoinStrings(s, modalities, "\\");

          requestedTags.SetValue(DICOM_TAG_MODALITIES_IN_STUDY, s, false);
        }

        if (IsRequestedComputedTag(DICOM_TAG_SOP_CLASSES_IN_STUDY))
        {
          std::set<std::string> classes;
          resource.GetChildrenMetadataValues(classes, ResourceType_Instance, MetadataType_Instance_SopClassUid);

          std::string s;
          Toolbox::JoinStrings(s, classes, "\\");

          requestedTags.SetValue(DICOM_TAG_SOP_CLASSES_IN_STUDY, s, false);
        }

        break;

      case ResourceType_Series:
        InjectChildrenCountComputedTag(requestedTags, DICOM_TAG_NUMBER_OF_SERIES_RELATED_INSTANCES, resource, ResourceType_Instance);
        break;

      case ResourceType_Instance:
        if (IsRequestedComputedTag(DICOM_TAG_INSTANCE_AVAILABILITY))
        {
          requestedTags.SetValue(DICOM_TAG_INSTANCE_AVAILABILITY, "ONLINE", false);
        }
        break;

      default:
        throw OrthancException(ErrorCode_InternalError);
    }
  }


  SeriesStatus ResourceFinder::GetSeriesStatus(uint32_t& expectedNumberOfInstances,
                                               const FindResponse::Resource& resource)
  {
    if (resource.GetLevel() != ResourceType_Series)
    {
      throw OrthancException(ErrorCode_BadParameterType);
    }

    std::string s;
    if (!resource.LookupMetadata(s, ResourceType_Series, MetadataType_Series_ExpectedNumberOfInstances) ||
        !SerializationToolbox::ParseUnsignedInteger32(expectedNumberOfInstances, s))
    {
      return SeriesStatus_Unknown;
    }

    std::set<std::string> values;
    resource.GetChildrenMetadataValues(values, ResourceType_Instance, MetadataType_Instance_IndexInSeries);

    std::set<int64_t> instances;

    for (std::set<std::string>::const_iterator
           it = values.begin(); it != values.end(); ++it)
    {
      int64_t index;

      if (!SerializationToolbox::ParseInteger64(index, *it))
      {
        return SeriesStatus_Unknown;
      }

      if (index <= 0 ||
          index > static_cast<int64_t>(expectedNumberOfInstances))
      {
        // Out-of-range instance index
        return SeriesStatus_Inconsistent;
      }

      if (instances.find(index) != instances.end())
      {
        // Twice the same instance index
        return SeriesStatus_Inconsistent;
      }

      instances.insert(index);
    }

    if (instances.size() == static_cast<size_t>(expectedNumberOfInstances))
    {
      return SeriesStatus_Complete;
    }
    else
    {
      return SeriesStatus_Missing;
    }
  }

  static void GetMainDicomSequencesFromMetadata(DicomMap& target, const FindResponse::Resource& resource, ResourceType level)
  {
    // read all main sequences from DB
    std::string serializedSequences;
    if (resource.LookupMetadata(serializedSequences, level, MetadataType_MainDicomSequences))
    {
      Json::Value jsonMetadata;
      Toolbox::ReadJson(jsonMetadata, serializedSequences);

      if (jsonMetadata["Version"].asInt() == 1)
      {
        target.FromDicomAsJson(jsonMetadata["Sequences"], true /* append */, true /* parseSequences */);
      }
      else
      {
        throw OrthancException(ErrorCode_NotImplemented);
      }
    }
  }


  void ResourceFinder::Expand(Json::Value& target,
                              const FindResponse::Resource& resource,
                              ServerIndex& index,
                              DicomToJsonFormat format) const
  {
    /**
     * This method closely follows "SerializeExpandedResource()" in
     * "ServerContext.cpp" from Orthanc 1.12.4.
     **/

    if (responseContent_ == ResponseContentFlags_ID)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    if (resource.GetLevel() != request_.GetLevel())
    {
      throw OrthancException(ErrorCode_InternalError);
    }

    target = Json::objectValue;

    target["Type"] = GetResourceTypeText(resource.GetLevel(), false, true);
    target["ID"] = resource.GetIdentifier();

    if (responseContent_ & ResponseContentFlags_Parent)
    {
      switch (resource.GetLevel())
      {
        case ResourceType_Patient:
          break;

        case ResourceType_Study:
          target["ParentPatient"] = resource.GetParentIdentifier();
          break;

        case ResourceType_Series:
          target["ParentStudy"] = resource.GetParentIdentifier();
          break;

        case ResourceType_Instance:
          target["ParentSeries"] = resource.GetParentIdentifier();
          break;

        default:
          throw OrthancException(ErrorCode_InternalError);
      }
    }

    if ((responseContent_ & ResponseContentFlags_Children) && (resource.GetLevel() != ResourceType_Instance))
    {
      const std::set<std::string>& children = resource.GetChildrenIdentifiers(GetChildResourceType(resource.GetLevel()));

      Json::Value c = Json::arrayValue;
      for (std::set<std::string>::const_iterator
             it = children.begin(); it != children.end(); ++it)
      {
        c.append(*it);
      }

      switch (resource.GetLevel())
      {
        case ResourceType_Patient:
          target["Studies"] = c;
          break;

        case ResourceType_Study:
          target["Series"] = c;
          break;

        case ResourceType_Series:
          target["Instances"] = c;
          break;

        default:
          throw OrthancException(ErrorCode_InternalError);
      }
    }

    switch (resource.GetLevel())
    {
      case ResourceType_Patient:
      case ResourceType_Study:
        break;

      case ResourceType_Series:
      {
        if ((responseContent_ & ResponseContentFlags_Status) || (responseContent_ & ResponseContentFlags_MetadataLegacy) )
        {
          uint32_t expectedNumberOfInstances;
          SeriesStatus status = GetSeriesStatus(expectedNumberOfInstances, resource);
          
          if (responseContent_ & ResponseContentFlags_Status )
          {
            target["Status"] = EnumerationToString(status);
          }

          if (responseContent_ & ResponseContentFlags_MetadataLegacy)
          {
            static const char* const EXPECTED_NUMBER_OF_INSTANCES = "ExpectedNumberOfInstances";

            if (status == SeriesStatus_Unknown)
            {
              target[EXPECTED_NUMBER_OF_INSTANCES] = Json::nullValue;
            }
            else
            {
              target[EXPECTED_NUMBER_OF_INSTANCES] = expectedNumberOfInstances;
            }
          }
        }
        break;
      }

      case ResourceType_Instance:
      {
        if (responseContent_ & ResponseContentFlags_AttachmentsLegacy)
        {
          FileInfo info;
          int64_t revision;
          if (resource.LookupAttachment(info, revision, FileContentType_Dicom))
          {
            target["FileSize"] = static_cast<Json::UInt64>(info.GetUncompressedSize());
            target["FileUuid"] = info.GetUuid();
          }
          else
          {
            throw OrthancException(ErrorCode_InternalError);
          }
        }

        if (responseContent_ & ResponseContentFlags_MetadataLegacy)
        {
          static const char* const INDEX_IN_SERIES = "IndexInSeries";

          std::string s;
          uint32_t indexInSeries;
          if (resource.LookupMetadata(s, ResourceType_Instance, MetadataType_Instance_IndexInSeries) &&
              SerializationToolbox::ParseUnsignedInteger32(indexInSeries, s))
          {
            target[INDEX_IN_SERIES] = indexInSeries;
          }
          else
          {
            target[INDEX_IN_SERIES] = Json::nullValue;
          }
        }
        break;
      }

      default:
        throw OrthancException(ErrorCode_InternalError);
    }

    std::string s;
    if (responseContent_ & ResponseContentFlags_MetadataLegacy)
    {
      if (resource.LookupMetadata(s, resource.GetLevel(), MetadataType_AnonymizedFrom))
      {
        target["AnonymizedFrom"] = s;
      }

      if (resource.LookupMetadata(s, resource.GetLevel(), MetadataType_ModifiedFrom))
      {
        target["ModifiedFrom"] = s;
      }

      if (resource.GetLevel() == ResourceType_Patient ||
          resource.GetLevel() == ResourceType_Study ||
          resource.GetLevel() == ResourceType_Series)
      {
        if (resource.LookupMetadata(s, resource.GetLevel(), MetadataType_LastUpdate))
        {
          target["LastUpdate"] = s;
        }
      }
    }

    if (responseContent_ & ResponseContentFlags_IsStable)
    {
      if (resource.GetLevel() == ResourceType_Patient ||
          resource.GetLevel() == ResourceType_Study ||
          resource.GetLevel() == ResourceType_Series)
      {
        target["IsStable"] = !index.IsUnstableResource(resource.GetLevel(), resource.GetInternalId());
      }
    }

    if (responseContent_ & ResponseContentFlags_MainDicomTags)
    {
      DicomMap allMainDicomTags;
      resource.GetMainDicomTags(allMainDicomTags, resource.GetLevel());

      // read all main sequences from DB
      GetMainDicomSequencesFromMetadata(allMainDicomTags, resource, resource.GetLevel());

      static const char* const MAIN_DICOM_TAGS = "MainDicomTags";
      static const char* const PATIENT_MAIN_DICOM_TAGS = "PatientMainDicomTags";

      // TODO-FIND : Ignore "null" values

      DicomMap levelMainDicomTags;
      allMainDicomTags.ExtractResourceInformation(levelMainDicomTags, resource.GetLevel());

      target[MAIN_DICOM_TAGS] = Json::objectValue;
      FromDcmtkBridge::ToJson(target[MAIN_DICOM_TAGS], levelMainDicomTags, format);

      if (resource.GetLevel() == ResourceType_Study)
      {
        DicomMap patientMainDicomTags;
        allMainDicomTags.ExtractPatientInformation(patientMainDicomTags);

        target[PATIENT_MAIN_DICOM_TAGS] = Json::objectValue;
        FromDcmtkBridge::ToJson(target[PATIENT_MAIN_DICOM_TAGS], patientMainDicomTags, format);
      }
    }

    if (responseContent_ & ResponseContentFlags_Labels)
    {
      Json::Value labels = Json::arrayValue;

      for (std::set<std::string>::const_iterator
             it = resource.GetLabels().begin(); it != resource.GetLabels().end(); ++it)
      {
        labels.append(*it);
      }

      target["Labels"] = labels;
    }

    if (responseContent_ & ResponseContentFlags_Metadata)  // new in Orthanc 1.12.4
    {
      const std::map<MetadataType, FindResponse::MetadataContent>& m = resource.GetMetadata(resource.GetLevel());

      Json::Value metadata = Json::objectValue;

      for (std::map<MetadataType, FindResponse::MetadataContent>::const_iterator it = m.begin(); it != m.end(); ++it)
      {
        metadata[EnumerationToString(it->first)] = it->second.GetValue();
      }

      target["Metadata"] = metadata;
    }

    if (responseContent_ & ResponseContentFlags_Attachments)  // new in Orthanc 1.12.5
    {
      const std::map<FileContentType, FileInfo>& attachments = resource.GetAttachments();

      target["Attachments"] = Json::arrayValue;

      for (std::map<FileContentType, FileInfo>::const_iterator it = attachments.begin(); it != attachments.end(); ++it)
      {
        Json::Value attachment = Json::objectValue;    
        attachment["Uuid"] = it->second.GetUuid();
        attachment["ContentType"] = it->second.GetContentType();
        attachment["UncompressedSize"] = Json::Value::UInt64(it->second.GetUncompressedSize());
        attachment["CompressedSize"] = Json::Value::UInt64(it->second.GetCompressedSize());
        attachment["UncompressedMD5"] = it->second.GetUncompressedMD5();
        attachment["CompressedMD5"] = it->second.GetCompressedMD5();

        target["Attachments"].append(attachment);
      }
    }
  }


  void ResourceFinder::UpdateRequestLimits(ServerContext& context)
  {
    if (context.GetIndex().HasFindSupport())  // in this case, limits are fully implemented in DB
    {
      pagingMode_ = PagingMode_FullDatabase;

      if (hasLimitsSince_ || hasLimitsCount_)
      {
        pagingMode_ = PagingMode_FullDatabase;
        if (databaseLimits_ != 0 && limitsCount_ > databaseLimits_)
        {
          LOG(WARNING) << "ResourceFinder: \"Limit\" is larger than LimitFindResults/LimitFindInstances configurations, using limit from the configuration file";
          limitsCount_ = databaseLimits_;
        }

        request_.SetLimits(limitsSince_, limitsCount_);
      }
      else if (databaseLimits_ != 0)
      {
        request_.SetLimits(0, databaseLimits_);
      }

    }
    else
    {
      // By default, use manual paging
      pagingMode_ = PagingMode_FullManual;

      if (databaseLimits_ != 0)
      {
        request_.SetLimits(0, databaseLimits_ + 1);
      }
      else
      {
        request_.ClearLimits();
      }

      if (lookup_.get() == NULL &&
          (hasLimitsSince_ || hasLimitsCount_))
      {
        pagingMode_ = PagingMode_FullDatabase;
        request_.SetLimits(limitsSince_, limitsCount_);
      }

      if (lookup_.get() != NULL &&
          canBeFullyPerformedInDb_ &&
          (hasLimitsSince_ || hasLimitsCount_))
      {
        /**
         * TODO-FIND: "IDatabaseWrapper::ApplyLookupResources()" only
         * accept the "limit" argument.  The "since" must be implemented
         * manually.
         **/

        if (hasLimitsSince_ &&
            limitsSince_ != 0)
        {
          pagingMode_ = PagingMode_ManualSkip;
          request_.SetLimits(0, limitsCount_ + limitsSince_);
        }
        else
        {
          pagingMode_ = PagingMode_FullDatabase;
          request_.SetLimits(0, limitsCount_);
        }
      }
    }
  }


  ResourceFinder::ResourceFinder(ResourceType level,
                                 ResponseContentFlags responseContent,
                                 FindStorageAccessMode storageAccessMode,
                                 bool supportsChildExistQueries) :
    request_(level),
    databaseLimits_(0),
    isSimpleLookup_(true),
    canBeFullyPerformedInDb_(true),
    pagingMode_(PagingMode_FullManual),
    hasLimitsSince_(false),
    hasLimitsCount_(false),
    limitsSince_(0),
    limitsCount_(0),
    responseContent_(responseContent),
    storageAccessMode_(storageAccessMode),
    supportsChildExistQueries_(supportsChildExistQueries),
    isWarning002Enabled_(false),
    isWarning004Enabled_(false),
    isWarning005Enabled_(false)
  {
    {
      OrthancConfiguration::ReaderLock lock;
      isWarning002Enabled_ = lock.GetConfiguration().IsWarningEnabled(Warnings_002_InconsistentDicomTagsInDb);
      isWarning004Enabled_ = lock.GetConfiguration().IsWarningEnabled(Warnings_004_NoMainDicomTagsSignature);
      isWarning005Enabled_ = lock.GetConfiguration().IsWarningEnabled(Warnings_005_RequestingTagFromLowerResourceLevel);
    }

    request_.SetRetrieveMainDicomTags(responseContent_ & ResponseContentFlags_MainDicomTags);
    request_.SetRetrieveMetadata((responseContent_ & ResponseContentFlags_Metadata) || (responseContent_ & ResponseContentFlags_MetadataLegacy));
    request_.SetRetrieveLabels(responseContent_ & ResponseContentFlags_Labels);

    switch (level)
    {
      case ResourceType_Patient:
        request_.GetChildrenSpecification(ResourceType_Study).SetRetrieveIdentifiers(responseContent_ & ResponseContentFlags_Children);
        request_.SetRetrieveAttachments(responseContent_ & ResponseContentFlags_Attachments); 
        break;

      case ResourceType_Study:
        request_.GetChildrenSpecification(ResourceType_Series).SetRetrieveIdentifiers(responseContent_ & ResponseContentFlags_Children);
        request_.SetRetrieveParentIdentifier(responseContent_ & ResponseContentFlags_Parent);
        request_.SetRetrieveAttachments(responseContent_ & ResponseContentFlags_Attachments); 
        break;

      case ResourceType_Series:
        if (responseContent_ & ResponseContentFlags_Status)
        {
          request_.GetChildrenSpecification(ResourceType_Instance).AddMetadata(MetadataType_Instance_IndexInSeries); // required for the SeriesStatus
        }
        request_.GetChildrenSpecification(ResourceType_Instance).SetRetrieveIdentifiers(responseContent_ & ResponseContentFlags_Children);
        request_.SetRetrieveParentIdentifier(responseContent_ & ResponseContentFlags_Parent);
        request_.SetRetrieveAttachments(responseContent_ & ResponseContentFlags_Attachments); 
        break;

      case ResourceType_Instance:
        request_.SetRetrieveAttachments((responseContent_ & ResponseContentFlags_AttachmentsLegacy) // for FileSize & FileUuid
                                        || (responseContent_ & ResponseContentFlags_Attachments)); 
        request_.SetRetrieveParentIdentifier(true);
        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }


  void ResourceFinder::SetDatabaseLimits(uint64_t limits)
  {
    databaseLimits_ = limits;
  }


  void ResourceFinder::SetLimitsSince(uint64_t since)
  {
    if (hasLimitsSince_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      hasLimitsSince_ = true;
      limitsSince_ = since;
    }
  }


  void ResourceFinder::SetLimitsCount(uint64_t count)
  {
    if (hasLimitsCount_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      hasLimitsCount_ = true;
      limitsCount_ = count;
    }
  }


  void ResourceFinder::SetDatabaseLookup(const DatabaseLookup& lookup)
  {
    MainDicomTagsRegistry registry;

    lookup_.reset(lookup.Clone());

    for (size_t i = 0; i < lookup.GetConstraintsCount(); i++)
    {
      DicomTag tag = lookup.GetConstraint(i).GetTag();
      if (IsComputedTag(tag))
      {
        AddRequestedTag(tag);
      }
      else
      {
        ResourceType level;
        DicomTagType tagType;
        registry.LookupTag(level, tagType, tag);
        if (tagType == DicomTagType_Generic)
        {
          AddRequestedTag(tag);
        }
      }
    }

    isSimpleLookup_ = registry.NormalizeLookup(canBeFullyPerformedInDb_, request_.GetDicomTagConstraints(), lookup, request_.GetLevel(), supportsChildExistQueries_);

    // "request_.GetDicomTagConstraints()" only contains constraints on main DICOM tags

    for (size_t i = 0; i < request_.GetDicomTagConstraints().GetSize(); i++)
    {
      const DatabaseDicomTagConstraint& constraint = request_.GetDicomTagConstraints().GetConstraint(i);
      if (constraint.GetLevel() == request_.GetLevel())
      {
        request_.SetRetrieveMainDicomTags(true);
      }
      else if (IsResourceLevelAboveOrEqual(constraint.GetLevel(), request_.GetLevel()))
      {
        request_.GetParentSpecification(constraint.GetLevel()).SetRetrieveMainDicomTags(true);
      }
      else
      {
        if (!supportsChildExistQueries_ || (constraint.GetLevel() != ResourceType_Series && constraint.GetTag() != DICOM_TAG_MODALITIES_IN_STUDY))
        {
          LOG(WARNING) << "Executing a database lookup at level " << EnumerationToString(request_.GetLevel())
                      << " on main DICOM tag " << constraint.GetTag().Format() << " from an inferior level ("
                      << EnumerationToString(constraint.GetLevel()) << "), this will return no result";
        }
      }

      if (IsComputedTag(constraint.GetTag()) && constraint.GetTag() != DICOM_TAG_MODALITIES_IN_STUDY)
      {
        // Sanity check
        throw OrthancException(ErrorCode_InternalError);
      }
    }
  }


  void ResourceFinder::AddRequestedTag(const DicomTag& tag)
  {
    requestedTags_.insert(tag);

    if (DicomMap::IsMainDicomTag(tag, ResourceType_Patient))
    {
      if (request_.GetLevel() == ResourceType_Patient)
      {
        request_.SetRetrieveMainDicomTags(true);
      }
      else
      {
        /**
         * This comes from the fact that patient-level tags are copied
         * at the study level, as implemented by "ResourcesContent::AddResource()".
         **/
        if (request_.GetLevel() == ResourceType_Study)
        {
          request_.SetRetrieveMainDicomTags(true);
        }
        else
        {
          request_.GetParentSpecification(ResourceType_Study).SetRetrieveMainDicomTags(true);
          request_.GetParentSpecification(ResourceType_Study).SetRetrieveMetadata(true);  // to get the MainDicomSequences
        }
      }
    }
    else if (DicomMap::IsMainDicomTag(tag, ResourceType_Study))
    {
      if (request_.GetLevel() == ResourceType_Patient)
      {
        if (isWarning005Enabled_)
        {
          LOG(WARNING) << "W005: Requested tag " << tag.Format()
                       << " should only be read at the study, series, or instance level";
        }
        request_.SetRetrieveOneInstanceMetadataAndAttachments(true); // we might need to get it from one instance
      }
      else
      {
        if (request_.GetLevel() == ResourceType_Study)
        {
          request_.SetRetrieveMainDicomTags(true);
        }
        else
        {
          request_.GetParentSpecification(ResourceType_Study).SetRetrieveMainDicomTags(true);
          request_.GetParentSpecification(ResourceType_Study).SetRetrieveMetadata(true);  // to get the MainDicomSequences
        }
      }
    }
    else if (DicomMap::IsMainDicomTag(tag, ResourceType_Series))
    {
      if (request_.GetLevel() == ResourceType_Patient ||
          request_.GetLevel() == ResourceType_Study)
      {
        if (isWarning005Enabled_)
        {
          LOG(WARNING) << "W005: Requested tag " << tag.Format()
                      << " should only be read at the series or instance level";
        }
        request_.SetRetrieveOneInstanceMetadataAndAttachments(true); // we might need to get it from one instance
      }
      else
      {
        if (request_.GetLevel() == ResourceType_Series)
        {
          request_.SetRetrieveMainDicomTags(true);
        }
        else
        {
          request_.GetParentSpecification(ResourceType_Series).SetRetrieveMainDicomTags(true);
          request_.GetParentSpecification(ResourceType_Series).SetRetrieveMetadata(true);  // to get the MainDicomSequences
        }
      }
    }
    else if (DicomMap::IsMainDicomTag(tag, ResourceType_Instance))
    {
      if (request_.GetLevel() == ResourceType_Patient ||
          request_.GetLevel() == ResourceType_Study ||
          request_.GetLevel() == ResourceType_Series)
      {
        if (isWarning005Enabled_)
        {
          LOG(WARNING) << "W005: Requested tag " << tag.Format()
                       << " should only be read at the instance level";
        }
        request_.SetRetrieveOneInstanceMetadataAndAttachments(true); // we might need to get it from one instance
      }
      else
      {
        request_.SetRetrieveMainDicomTags(true);
      }
    }
    else if (tag == DICOM_TAG_NUMBER_OF_PATIENT_RELATED_STUDIES)
    {
      ConfigureChildrenCountComputedTag(tag, ResourceType_Patient, ResourceType_Study);
    }
    else if (tag == DICOM_TAG_NUMBER_OF_PATIENT_RELATED_SERIES)
    {
      ConfigureChildrenCountComputedTag(tag, ResourceType_Patient, ResourceType_Series);
    }
    else if (tag == DICOM_TAG_NUMBER_OF_PATIENT_RELATED_INSTANCES)
    {
      ConfigureChildrenCountComputedTag(tag, ResourceType_Patient, ResourceType_Instance);
    }
    else if (tag == DICOM_TAG_NUMBER_OF_STUDY_RELATED_SERIES)
    {
      ConfigureChildrenCountComputedTag(tag, ResourceType_Study, ResourceType_Series);
    }
    else if (tag == DICOM_TAG_NUMBER_OF_STUDY_RELATED_INSTANCES)
    {
      ConfigureChildrenCountComputedTag(tag, ResourceType_Study, ResourceType_Instance);
    }
    else if (tag == DICOM_TAG_NUMBER_OF_SERIES_RELATED_INSTANCES)
    {
      ConfigureChildrenCountComputedTag(tag, ResourceType_Series, ResourceType_Instance);
    }
    else if (tag == DICOM_TAG_SOP_CLASSES_IN_STUDY)
    {
      requestedComputedTags_.insert(tag);
      request_.GetChildrenSpecification(ResourceType_Instance).AddMetadata(MetadataType_Instance_SopClassUid);
    }
    else if (tag == DICOM_TAG_MODALITIES_IN_STUDY)
    {
      requestedComputedTags_.insert(tag);
      if (request_.GetLevel() < ResourceType_Series)
      {
        request_.GetChildrenSpecification(ResourceType_Series).AddMainDicomTag(DICOM_TAG_MODALITY);
      }
      else if (request_.GetLevel() == ResourceType_Instance)  // this happens in QIDO-RS when searching for instances without specifying a StudyInstanceUID -> all Study level tags must be included in the response
      {
        request_.GetParentSpecification(ResourceType_Series).SetRetrieveMainDicomTags(true);
      }
    }
    else if (tag == DICOM_TAG_INSTANCE_AVAILABILITY)
    {
      requestedComputedTags_.insert(tag);
    }
    else
    {
      // This is neither a main DICOM tag, nor a computed DICOM tag:
      // We might need to access a DICOM file or the MainDicomSequences metadata
      
      request_.SetRetrieveMetadata(true);

      if (request_.GetLevel() != ResourceType_Instance)
      {
        request_.SetRetrieveOneInstanceMetadataAndAttachments(true);
      }
    }
  }


  void ResourceFinder::AddRequestedTags(const std::set<DicomTag>& tags)
  {
    for (std::set<DicomTag>::const_iterator it = tags.begin(); it != tags.end(); ++it)
    {
      AddRequestedTag(*it);
    }
  }


  static void InjectRequestedTags(DicomMap& target,
                                  std::set<DicomTag>& remainingRequestedTags /* in & out */,
                                  const FindResponse::Resource& resource,
                                  ResourceType level/*,
                                  const std::set<DicomTag>& tags*/)
  {
    if (!remainingRequestedTags.empty() && level <= resource.GetLevel())
    {
      std::set<DicomTag> savedMainDicomTags;

      DicomMap m;
      resource.GetMainDicomTags(m, level);                          // read DicomTags from DB

      if (resource.GetMetadata(level).size() > 0)
      {
        GetMainDicomSequencesFromMetadata(m, resource, level);        // read DicomSequences from metadata
      
        // check which tags have been saved in DB; that's the way to know if they are missing because they were not saved or because they have no value
        
        std::string signature = DicomMap::GetDefaultMainDicomTagsSignatureFrom1_11(level); // default signature in case it's not in the metadata (= the signature for 1.11.0)
        if (resource.LookupMetadata(signature, level, MetadataType_MainDicomTagsSignature))
        {
          if (level == ResourceType_Study) // when we retrieve the study tags, we actually also get the patient tags that are also saved at study level but not included in the signature
          {
            signature += ";" + DicomMap::GetDefaultMainDicomTagsSignatureFrom1_11(ResourceType_Patient); // append the default signature (from before 1.11.0)
          }

          FromDcmtkBridge::ParseListOfTags(savedMainDicomTags, signature);
        }
      }

      std::set<DicomTag> copiedTags;
      for (std::set<DicomTag>::const_iterator it = remainingRequestedTags.begin(); it != remainingRequestedTags.end(); ++it)
      {
        if (target.CopyTagIfExists(m, *it))
        {
          copiedTags.insert(*it);
        }
        else if (savedMainDicomTags.find(*it) != savedMainDicomTags.end())  // the tag should have been saved in DB but has no value so we consider it has been copied
        {
          copiedTags.insert(*it);
        }
      }

      Toolbox::RemoveSets(remainingRequestedTags, copiedTags);
    }
  }


  static void ConvertMetadata(std::map<MetadataType, std::string>& converted,
                              const FindResponse::Resource& resource)
  {
    const std::map<MetadataType, FindResponse::MetadataContent> metadata = resource.GetMetadata(ResourceType_Instance);

    for (std::map<MetadataType, FindResponse::MetadataContent>::const_iterator
           it = metadata.begin(); it != metadata.end(); ++it)
    {
      converted[it->first] = it->second.GetValue();
    }
  }


  static void ReadMissingTagsFromStorageArea(DicomMap& requestedTags,
                                             ServerContext& context,
                                             const FindRequest& request,
                                             const FindResponse::Resource& resource,
                                             const std::set<DicomTag>& missingTags)
  {
    OrthancConfiguration::ReaderLock lock;
    if (lock.GetConfiguration().IsWarningEnabled(Warnings_001_TagsBeingReadFromStorage))
    {
      std::string missings;
      FromDcmtkBridge::FormatListOfTags(missings, missingTags);

      LOG(WARNING) << "W001: Accessing DICOM tags from storage when accessing "
                   << Orthanc::GetResourceTypeText(resource.GetLevel(), false, false)
                   << " " << resource.GetIdentifier()
                   << ": " << missings;
    }

    // TODO-FIND: What do we do if the DICOM has been removed since the request?
    // Do we fail, or do we skip the resource?

    Json::Value tmpDicomAsJson;

    if (request.GetLevel() == ResourceType_Instance &&
        request.IsRetrieveMetadata() &&
        request.IsRetrieveAttachments())
    {
      LOG(INFO) << "Will retrieve missing DICOM tags from instance: " << resource.GetIdentifier();

      std::map<MetadataType, std::string> converted;
      ConvertMetadata(converted, resource);

      context.ReadDicomAsJson(tmpDicomAsJson, resource.GetIdentifier(), converted,
                              resource.GetAttachments(), missingTags /* ignoreTagLength */);
    }
    else if (request.GetLevel() != ResourceType_Instance &&
             request.IsRetrieveOneInstanceMetadataAndAttachments())
    {
      LOG(INFO) << "Will retrieve missing DICOM tags from instance: " << resource.GetOneInstancePublicId();

      context.ReadDicomAsJson(tmpDicomAsJson, resource.GetOneInstancePublicId(), resource.GetOneInstanceMetadata(),
                              resource.GetOneInstanceAttachments(), missingTags /* ignoreTagLength */);
    }
    else
    {
      // TODO-FIND: This fallback shouldn't be necessary

      FindRequest requestDicomAttachment(request.GetLevel());
      requestDicomAttachment.SetOrthancId(request.GetLevel(), resource.GetIdentifier());

      if (request.GetLevel() == ResourceType_Instance)
      {
        requestDicomAttachment.SetRetrieveMetadata(true);
        requestDicomAttachment.SetRetrieveAttachments(true);
      }
      else
      {
        requestDicomAttachment.SetRetrieveOneInstanceMetadataAndAttachments(true);
      }

      FindResponse responseDicomAttachment;
      context.GetIndex().ExecuteFind(responseDicomAttachment, requestDicomAttachment);

      if (responseDicomAttachment.GetSize() != 1)
      {
        throw OrthancException(ErrorCode_InexistentFile);
      }
      else
      {
        const FindResponse::Resource& response = responseDicomAttachment.GetResourceByIndex(0);
        const std::string instancePublicId = response.GetIdentifier();
        LOG(INFO) << "Will retrieve missing DICOM tags from instance: " << instancePublicId;

        if (request.GetLevel() == ResourceType_Instance)
        {
          std::map<MetadataType, std::string> converted;
          ConvertMetadata(converted, resource);

          context.ReadDicomAsJson(tmpDicomAsJson, response.GetIdentifier(), converted,
                                  response.GetAttachments(), missingTags /* ignoreTagLength */);
        }
        else
        {
          context.ReadDicomAsJson(tmpDicomAsJson, response.GetOneInstancePublicId(), response.GetOneInstanceMetadata(),
                                  response.GetOneInstanceAttachments(), missingTags /* ignoreTagLength */);
        }
      }
    }

    DicomMap tmpDicomMap;
    tmpDicomMap.FromDicomAsJson(tmpDicomAsJson, false /* append */, true /* parseSequences*/);

    for (std::set<DicomTag>::const_iterator it = missingTags.begin(); it != missingTags.end(); ++it)
    {
      assert(!requestedTags.HasTag(*it));
      if (tmpDicomMap.HasTag(*it))
      {
        requestedTags.SetValue(*it, tmpDicomMap.GetValue(*it));
      }
      else
      {
        requestedTags.SetNullValue(*it);  // TODO-FIND: Is this compatible with Orthanc <= 1.12.3?
      }
    }
  }

  uint64_t ResourceFinder::Count(ServerContext& context) const
  {
    if (!canBeFullyPerformedInDb_)
    {
      throw OrthancException(ErrorCode_BadRequest,
                            "Unable to count resources when querying tags that are not stored as MainDicomTags in the Database or when using case sensitive queries.");
    }

    uint64_t count = 0;
    context.GetIndex().ExecuteCount(count, request_);
    return count;
  }


  void ResourceFinder::Execute(IVisitor& visitor,
                               ServerContext& context)
  {
    UpdateRequestLimits(context);

    if ((request_.HasLimits() && request_.GetLimitsSince() > 0) &&
        !canBeFullyPerformedInDb_)
    {
      throw OrthancException(ErrorCode_BadRequest,
                             "nable to use 'Since' when finding resources when querying against Dicom Tags that are not in the MainDicomTags or when using CaseSenstive queries.");
    }

    bool isWarning002Enabled = false;
    bool isWarning004Enabled = false;
    bool isWarning006Enabled = false;
    bool isWarning007Enabled = false;

    {
      OrthancConfiguration::ReaderLock lock;
      isWarning002Enabled = lock.GetConfiguration().IsWarningEnabled(Warnings_002_InconsistentDicomTagsInDb);
      isWarning004Enabled = lock.GetConfiguration().IsWarningEnabled(Warnings_004_NoMainDicomTagsSignature);
      isWarning006Enabled = lock.GetConfiguration().IsWarningEnabled(Warnings_006_RequestingTagFromMetaHeader);
      isWarning007Enabled = lock.GetConfiguration().IsWarningEnabled(Warnings_007_MissingRequestedTagsNotReadFromDisk);
    }

    FindResponse response;
    context.GetIndex().ExecuteFind(response, request_);

    bool complete;

    switch (pagingMode_)
    {
      case PagingMode_FullDatabase:
      case PagingMode_ManualSkip:
        complete = true;
        break;

      case PagingMode_FullManual:
        complete = (databaseLimits_ == 0 ||
                    response.GetSize() <= databaseLimits_);
        break;

      default:
        throw OrthancException(ErrorCode_InternalError);
    }

    if (lookup_.get() != NULL)
    {
      LOG(INFO) << "Number of candidate resources after fast DB filtering on main DICOM tags: " << response.GetSize();
    }

    size_t countResults = 0;
    size_t skipped = 0;

    for (size_t i = 0; i < response.GetSize(); i++)
    {
      const FindResponse::Resource& resource = response.GetResourceByIndex(i);

#if 0
      {
        Json::Value v;
        resource.DebugExport(v, request_);
        std::cout << v.toStyledString();
      }
#endif

      DicomMap outRequestedTags;

      if (HasRequestedTags())
      {
        std::set<DicomTag> remainingRequestedTags = requestedTags_; // at this point, all requested tags are "missing"

        InjectComputedTags(outRequestedTags, resource);
        Toolbox::RemoveSets(remainingRequestedTags, requestedComputedTags_);

        InjectRequestedTags(outRequestedTags, remainingRequestedTags, resource, ResourceType_Patient);
        InjectRequestedTags(outRequestedTags, remainingRequestedTags, resource, ResourceType_Study);
        InjectRequestedTags(outRequestedTags, remainingRequestedTags, resource, ResourceType_Series);
        InjectRequestedTags(outRequestedTags, remainingRequestedTags, resource, ResourceType_Instance);

        if (DicomMap::HasMetaInformationTags(remainingRequestedTags)) // we are not able to retrieve meta information in RequestedTags
        {
          std::set<DicomTag> metaTagsToRemove;
          for (std::set<DicomTag>::const_iterator it = remainingRequestedTags.begin(); it != remainingRequestedTags.end(); ++it)
          {
            if (it->GetGroup() == 0x0002)
            {
              metaTagsToRemove.insert(*it);
            }
          }

          if (isWarning006Enabled)
          {
            std::string joinedMetaTags;
            FromDcmtkBridge::FormatListOfTags(joinedMetaTags, metaTagsToRemove);
            LOG(WARNING) << "W006: Unable to include tags from the Meta Header in \"RequestedTags\".  Skipping them: " << joinedMetaTags;
          }

          Toolbox::RemoveSets(remainingRequestedTags, metaTagsToRemove);
        }


        if (!remainingRequestedTags.empty() && 
            !DicomMap::HasOnlyComputedTags(remainingRequestedTags)) // if the only remaining tags are computed tags, it is worthless to read them from disk
        {
          // If a lookup tag is not available from DB, it is included in remainingRequestedTags and it will always be included in the answer too
          // -> from 1.12.5, "StorageAccessOnFind": "Always" is actually equivalent to "StorageAccessOnFind": "Answers"
          if (IsStorageAccessAllowed())
          {
            ReadMissingTagsFromStorageArea(outRequestedTags, context, request_, resource, remainingRequestedTags);
          }
          else if (isWarning007Enabled)
          {
            std::string joinedTags;
            FromDcmtkBridge::FormatListOfTags(joinedTags, remainingRequestedTags);
            LOG(WARNING) << "W007: Unable to include requested tags since \"StorageAccessOnFind\" does not allow accessing the storage to build answers: " << joinedTags;
          }
        }

        std::string mainDicomTagsSignature;
        if (isWarning002Enabled &&
            resource.LookupMetadata(mainDicomTagsSignature, resource.GetLevel(), MetadataType_MainDicomTagsSignature) &&
            mainDicomTagsSignature != DicomMap::GetMainDicomTagsSignature(resource.GetLevel()))
        {
          LOG(WARNING) << "W002: " << Orthanc::GetResourceTypeText(resource.GetLevel(), false , false)
                       << " has been stored with another version of Main Dicom Tags list, you should POST to /"
                       << Orthanc::GetResourceTypeText(resource.GetLevel(), true, false)
                       << "/" << resource.GetIdentifier()
                       << "/reconstruct to update the list of tags saved in DB or run the Housekeeper plugin.  Some MainDicomTags might be missing from this answer.";
        }
        else if (isWarning004Enabled && 
                 request_.IsRetrieveMetadata() &&
                 !resource.LookupMetadata(mainDicomTagsSignature, resource.GetLevel(), MetadataType_MainDicomTagsSignature))
        {
          LOG(WARNING) << "W004: " << Orthanc::GetResourceTypeText(resource.GetLevel(), false , false)
                       << " has been stored with an old Orthanc version and does not have a MainDicomTagsSignature, you should POST to /"
                       << Orthanc::GetResourceTypeText(resource.GetLevel(), true, false)
                       << "/" << resource.GetIdentifier()
                       << "/reconstruct to update the list of tags saved in DB or run the Housekeeper plugin.  Some MainDicomTags might be missing from this answer.";
        }

      }

      bool match = true;

      if (lookup_.get() != NULL)
      {
        DicomMap tags;
        resource.GetAllMainDicomTags(tags);
        tags.Merge(outRequestedTags);
        match = lookup_->IsMatch(tags);
      }

      if (match)
      {
        if (pagingMode_ == PagingMode_FullDatabase)
        {
          visitor.Apply(resource, outRequestedTags);
        }
        else
        {
          if (hasLimitsSince_ &&
              skipped < limitsSince_)
          {
            skipped++;
          }
          else if (hasLimitsCount_ &&
                   countResults >= limitsCount_)
          {
            // Too many results, don't mark as complete
            complete = false;
            break;
          }
          else
          {
            visitor.Apply(resource, outRequestedTags);
            countResults++;
          }
        }
      }
    }

    if (complete)
    {
      visitor.MarkAsComplete();
    }
  }

  bool ResourceFinder::IsStorageAccessAllowed()
  {
    switch (storageAccessMode_)
    {
      case FindStorageAccessMode_DiskOnAnswer:
      case FindStorageAccessMode_DiskOnLookupAndAnswer:
        return true;
      case FindStorageAccessMode_DatabaseOnly:
        return false;
      default:
        throw OrthancException(ErrorCode_InternalError);
    }
  }

  void ResourceFinder::Execute(Json::Value& target,
                               ServerContext& context,
                               DicomToJsonFormat format,
                               bool includeAllMetadata)
  {
    class Visitor : public IVisitor
    {
    private:
      const ResourceFinder&  that_;
      ServerIndex&           index_;
      Json::Value&           target_;
      DicomToJsonFormat      format_;
      bool                   hasRequestedTags_;
      bool                   includeAllMetadata_;

    public:
      Visitor(const ResourceFinder& that,
              ServerIndex& index,
              Json::Value& target,
              DicomToJsonFormat format,
              bool hasRequestedTags,
              bool includeAllMetadata) :
        that_(that),
        index_(index),
        target_(target),
        format_(format),
        hasRequestedTags_(hasRequestedTags),
        includeAllMetadata_(includeAllMetadata)
      {
      }

      virtual void Apply(const FindResponse::Resource& resource,
                         const DicomMap& requestedTags) ORTHANC_OVERRIDE
      {
        if (that_.responseContent_ != ResponseContentFlags_ID)
        {
          Json::Value item;
          that_.Expand(item, resource, index_, format_);

          if (hasRequestedTags_)
          {
            static const char* const REQUESTED_TAGS = "RequestedTags";
            item[REQUESTED_TAGS] = Json::objectValue;
            FromDcmtkBridge::ToJson(item[REQUESTED_TAGS], requestedTags, format_);
          }

          target_.append(item);
        }
        else
        {
          target_.append(resource.GetIdentifier());
        }
      }

      virtual void MarkAsComplete() ORTHANC_OVERRIDE
      {
      }
    };

    UpdateRequestLimits(context);

    target = Json::arrayValue;

    Visitor visitor(*this, context.GetIndex(), target, format, HasRequestedTags(), includeAllMetadata);
    Execute(visitor, context);
  }


  bool ResourceFinder::ExecuteOneResource(Json::Value& target,
                                          ServerContext& context,
                                          DicomToJsonFormat format,
                                          bool includeAllMetadata)
  {
    Json::Value answer;
    Execute(answer, context, format, includeAllMetadata);

    if (answer.type() != Json::arrayValue)
    {
      throw OrthancException(ErrorCode_InternalError);
    }
    else if (answer.size() > 1)
    {
      throw OrthancException(ErrorCode_DatabasePlugin);
    }
    else if (answer.empty())
    {
      // Inexistent resource (or was deleted between the first and second phases)
      return false;
    }
    else
    {
      target = answer[0];
      return true;
    }
  }
}
