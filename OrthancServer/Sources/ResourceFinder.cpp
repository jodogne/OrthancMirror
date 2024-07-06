/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2024 Orthanc Team SRL, Belgium
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


#include "PrecompiledHeadersServer.h"
#include "ResourceFinder.h"

#include "../../OrthancFramework/Sources/DicomParsing/FromDcmtkBridge.h"
#include "../../OrthancFramework/Sources/Logging.h"
#include "../../OrthancFramework/Sources/OrthancException.h"
#include "../../OrthancFramework/Sources/SerializationToolbox.h"
#include "OrthancConfiguration.h"
#include "ServerContext.h"
#include "ServerIndex.h"


namespace Orthanc
{
  void ResourceFinder::ConfigureChildrenCountComputedTag(DicomTag tag,
                                                         ResourceType parentLevel,
                                                         ResourceType childLevel)
  {
    if (request_.GetLevel() == parentLevel)
    {
      requestedComputedTags_.insert(tag);
      hasRequestedTags_ = true;
      request_.GetChildrenRetrieveSpecification(childLevel).SetRetrieveIdentifiers(true);
    }
  }


  void ResourceFinder::InjectChildrenCountComputedTag(DicomMap& requestedTags,
                                                      DicomTag tag,
                                                      const FindResponse::Resource& resource,
                                                      ResourceType level) const
  {
    if (IsRequestedComputedTag(tag))
    {
      const std::set<std::string>& children = resource.GetChildrenIdentifiers(level);
      requestedTags.SetValue(tag, boost::lexical_cast<std::string>(children.size()), false);
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
                                               const FindResponse::Resource& resource) const
  {
    if (request_.GetLevel() != ResourceType_Series)
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


  void ResourceFinder::Expand(Json::Value& target,
                              const FindResponse::Resource& resource,
                              ServerIndex& index) const
  {
    /**
     * This method closely follows "SerializeExpandedResource()" in
     * "ServerContext.cpp" from Orthanc 1.12.3.
     **/

    if (resource.GetLevel() != request_.GetLevel())
    {
      throw OrthancException(ErrorCode_InternalError);
    }

    target = Json::objectValue;

    target["Type"] = GetResourceTypeText(resource.GetLevel(), false, true);
    target["ID"] = resource.GetIdentifier();

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

    if (resource.GetLevel() != ResourceType_Instance)
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
        uint32_t expectedNumberOfInstances;
        SeriesStatus status = GetSeriesStatus(expectedNumberOfInstances, resource);

        target["Status"] = EnumerationToString(status);

        static const char* const EXPECTED_NUMBER_OF_INSTANCES = "ExpectedNumberOfInstances";

        if (status == SeriesStatus_Unknown)
        {
          target[EXPECTED_NUMBER_OF_INSTANCES] = Json::nullValue;
        }
        else
        {
          target[EXPECTED_NUMBER_OF_INSTANCES] = expectedNumberOfInstances;
        }

        break;
      }

      case ResourceType_Instance:
      {
        FileInfo info;
        if (resource.LookupAttachment(info, FileContentType_Dicom))
        {
          target["FileSize"] = static_cast<Json::UInt64>(info.GetUncompressedSize());
          target["FileUuid"] = info.GetUuid();
        }
        else
        {
          throw OrthancException(ErrorCode_InternalError);
        }

        static const char* const INDEX_IN_SERIES = "IndexInSeries";

        std::string s;
        uint32_t index;
        if (resource.LookupMetadata(s, ResourceType_Instance, MetadataType_Instance_IndexInSeries) &&
            SerializationToolbox::ParseUnsignedInteger32(index, s))
        {
          target[INDEX_IN_SERIES] = index;
        }
        else
        {
          target[INDEX_IN_SERIES] = Json::nullValue;
        }

        break;
      }

      default:
        throw OrthancException(ErrorCode_InternalError);
    }

    std::string s;
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
      target["IsStable"] = !index.IsUnstableResource(resource.GetLevel(), resource.GetInternalId());

      if (resource.LookupMetadata(s, resource.GetLevel(), MetadataType_LastUpdate))
      {
        target["LastUpdate"] = s;
      }
    }

    {
      DicomMap allMainDicomTags;
      resource.GetMainDicomTags(allMainDicomTags, resource.GetLevel());

      /**
       * This section was part of "StatelessDatabaseOperations::ExpandResource()"
       * in Orthanc <= 1.12.3
       **/

      // read all main sequences from DB
      std::string serializedSequences;
      if (resource.LookupMetadata(serializedSequences, resource.GetLevel(), MetadataType_MainDicomSequences))
      {
        Json::Value jsonMetadata;
        Toolbox::ReadJson(jsonMetadata, serializedSequences);

        if (jsonMetadata["Version"].asInt() == 1)
        {
          allMainDicomTags.FromDicomAsJson(jsonMetadata["Sequences"], true /* append */, true /* parseSequences */);
        }
        else
        {
          throw OrthancException(ErrorCode_NotImplemented);
        }
      }

      /**
       * End of section from StatelessDatabaseOperations
       **/


      static const char* const MAIN_DICOM_TAGS = "MainDicomTags";
      static const char* const PATIENT_MAIN_DICOM_TAGS = "PatientMainDicomTags";

      // TODO-FIND : Ignore "null" values

      DicomMap levelMainDicomTags;
      allMainDicomTags.ExtractResourceInformation(levelMainDicomTags, resource.GetLevel());

      target[MAIN_DICOM_TAGS] = Json::objectValue;
      FromDcmtkBridge::ToJson(target[MAIN_DICOM_TAGS], levelMainDicomTags, format_);

      if (resource.GetLevel() == ResourceType_Study)
      {
        DicomMap patientMainDicomTags;
        allMainDicomTags.ExtractPatientInformation(patientMainDicomTags);

        target[PATIENT_MAIN_DICOM_TAGS] = Json::objectValue;
        FromDcmtkBridge::ToJson(target[PATIENT_MAIN_DICOM_TAGS], patientMainDicomTags, format_);
      }
    }

    {
      Json::Value labels = Json::arrayValue;

      for (std::set<std::string>::const_iterator
             it = resource.GetLabels().begin(); it != resource.GetLabels().end(); ++it)
      {
        labels.append(*it);
      }

      target["Labels"] = labels;
    }

    if (includeAllMetadata_)  // new in Orthanc 1.12.4
    {
      const std::map<MetadataType, std::string>& m = resource.GetMetadata(resource.GetLevel());

      Json::Value metadata = Json::objectValue;

      for (std::map<MetadataType, std::string>::const_iterator it = m.begin(); it != m.end(); ++it)
      {
        metadata[EnumerationToString(it->first)] = it->second;
      }

      target["Metadata"] = metadata;
    }
  }


  ResourceFinder::ResourceFinder(ResourceType level,
                                 bool expand) :
    request_(level),
    expand_(expand),
    format_(DicomToJsonFormat_Human),
    allowStorageAccess_(true),
    hasRequestedTags_(false),
    includeAllMetadata_(false)
  {
    if (expand)
    {
      request_.SetRetrieveMainDicomTags(true);
      request_.SetRetrieveMetadata(true);
      request_.SetRetrieveLabels(true);

      switch (level)
      {
        case ResourceType_Patient:
          request_.GetChildrenRetrieveSpecification(ResourceType_Study).SetRetrieveIdentifiers(true);
          break;

        case ResourceType_Study:
          request_.GetChildrenRetrieveSpecification(ResourceType_Series).SetRetrieveIdentifiers(true);
          request_.SetRetrieveParentIdentifier(true);
          break;

        case ResourceType_Series:
          request_.GetChildrenRetrieveSpecification(ResourceType_Instance).AddMetadata(MetadataType_Instance_IndexInSeries); // required for the SeriesStatus
          request_.GetChildrenRetrieveSpecification(ResourceType_Instance).SetRetrieveIdentifiers(true);
          request_.SetRetrieveParentIdentifier(true);
          break;

        case ResourceType_Instance:
          request_.SetRetrieveAttachments(true); // for FileSize & FileUuid
          request_.SetRetrieveParentIdentifier(true);
          break;

        default:
          throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
    }
  }


  void ResourceFinder::AddRequestedTags(const DicomTag& tag)
  {
    if (DicomMap::IsMainDicomTag(tag, ResourceType_Patient))
    {
      if (request_.GetLevel() == ResourceType_Patient)
      {
        request_.GetParentRetrieveSpecification(ResourceType_Patient).SetRetrieveMainDicomTags(true);
        request_.GetParentRetrieveSpecification(ResourceType_Patient).SetRetrieveMetadata(true);
        requestedPatientTags_.insert(tag);
      }
      else
      {
        /**
         * This comes from the fact that patient-level tags are copied
         * at the study level, as implemented by "ResourcesContent::AddResource()".
         **/
        request_.GetParentRetrieveSpecification(ResourceType_Study).SetRetrieveMainDicomTags(true);
        request_.GetParentRetrieveSpecification(ResourceType_Study).SetRetrieveMetadata(true);
        requestedStudyTags_.insert(tag);
      }

      hasRequestedTags_ = true;
    }
    else if (DicomMap::IsMainDicomTag(tag, ResourceType_Study))
    {
      if (request_.GetLevel() == ResourceType_Patient)
      {
        LOG(WARNING) << "Requested tag " << tag.Format()
                     << " should only be read at the study, series, or instance level";
        requestedTagsFromFileStorage_.insert(tag);
        request_.SetRetrieveOneInstanceIdentifier(true);
      }
      else
      {
        request_.GetParentRetrieveSpecification(ResourceType_Study).SetRetrieveMainDicomTags(true);
        request_.GetParentRetrieveSpecification(ResourceType_Study).SetRetrieveMetadata(true);
        requestedStudyTags_.insert(tag);
      }

      hasRequestedTags_ = true;
    }
    else if (DicomMap::IsMainDicomTag(tag, ResourceType_Series))
    {
      if (request_.GetLevel() == ResourceType_Patient ||
          request_.GetLevel() == ResourceType_Study)
      {
        LOG(WARNING) << "Requested tag " << tag.Format()
                     << " should only be read at the series or instance level";
        requestedTagsFromFileStorage_.insert(tag);
        request_.SetRetrieveOneInstanceIdentifier(true);
      }
      else
      {
        request_.GetParentRetrieveSpecification(ResourceType_Series).SetRetrieveMainDicomTags(true);
        request_.GetParentRetrieveSpecification(ResourceType_Series).SetRetrieveMetadata(true);
        requestedSeriesTags_.insert(tag);
      }

      hasRequestedTags_ = true;
    }
    else if (DicomMap::IsMainDicomTag(tag, ResourceType_Instance))
    {
      if (request_.GetLevel() == ResourceType_Patient ||
          request_.GetLevel() == ResourceType_Study ||
          request_.GetLevel() == ResourceType_Series)
      {
        LOG(WARNING) << "Requested tag " << tag.Format()
                     << " should only be read at the instance level";
        requestedTagsFromFileStorage_.insert(tag);
        request_.SetRetrieveOneInstanceIdentifier(true);
      }
      else
      {
        // Main DICOM tags from the instance level will be retrieved anyway
        assert(request_.IsRetrieveMainDicomTags());
        assert(request_.IsRetrieveMetadata());
        requestedInstanceTags_.insert(tag);
      }

      hasRequestedTags_ = true;
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
      hasRequestedTags_ = true;
      request_.GetChildrenRetrieveSpecification(ResourceType_Instance).AddMetadata(MetadataType_Instance_SopClassUid);
    }
    else if (tag == DICOM_TAG_MODALITIES_IN_STUDY)
    {
      requestedComputedTags_.insert(tag);
      hasRequestedTags_ = true;
      request_.GetChildrenRetrieveSpecification(ResourceType_Series).AddMainDicomTag(DICOM_TAG_MODALITY);
    }
    else if (tag == DICOM_TAG_INSTANCE_AVAILABILITY)
    {
      requestedComputedTags_.insert(tag);
      hasRequestedTags_ = true;
    }
    else
    {
      // This is neither a main DICOM tag, nor a computed DICOM tag:
      // We will be forced to access the DICOM file anyway
      requestedTagsFromFileStorage_.insert(tag);

      if (request_.GetLevel() != ResourceType_Instance)
      {
        request_.SetRetrieveOneInstanceIdentifier(true);
      }

      hasRequestedTags_ = true;
    }
  }


  void ResourceFinder::AddRequestedTags(const std::set<DicomTag>& tags)
  {
    for (std::set<DicomTag>::const_iterator it = tags.begin(); it != tags.end(); ++it)
    {
      AddRequestedTags(*it);
    }
  }


  static void InjectRequestedTags(DicomMap& requestedTags,
                                  std::set<DicomTag>& missingTags /* out */,
                                  const FindResponse::Resource& resource,
                                  ResourceType level,
                                  const std::set<DicomTag>& tags)
  {
    if (!tags.empty())
    {
      DicomMap m;
      resource.GetMainDicomTags(m, level);

      for (std::set<DicomTag>::const_iterator it = tags.begin(); it != tags.end(); ++it)
      {
        std::string value;
        if (m.LookupStringValue(value, *it, false /* not binary */))
        {
          requestedTags.SetValue(*it, value, false /* not binary */);
        }
        else
        {
          // This is the case where the Housekeeper should be run
          missingTags.insert(*it);
        }
      }
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

      LOG(WARNING) << "W001: Accessing Dicom tags from storage when accessing "
                   << Orthanc::GetResourceTypeText(resource.GetLevel(), false, false)
                   << ": " << missings;
    }

    std::string instancePublicId;

    if (request.IsRetrieveOneInstanceIdentifier())
    {
      instancePublicId = resource.GetOneInstanceIdentifier();
    }
    else if (request.GetLevel() == ResourceType_Instance)
    {
      instancePublicId = resource.GetIdentifier();
    }
    else
    {
      FindRequest requestDicomAttachment(request.GetLevel());
      requestDicomAttachment.SetOrthancId(request.GetLevel(), resource.GetIdentifier());
      requestDicomAttachment.SetRetrieveOneInstanceIdentifier(true);

      FindResponse responseDicomAttachment;
      context.GetIndex().ExecuteFind(responseDicomAttachment, requestDicomAttachment);

      if (responseDicomAttachment.GetSize() != 1 ||
          !responseDicomAttachment.GetResourceByIndex(0).HasOneInstanceIdentifier())
      {
        throw OrthancException(ErrorCode_InexistentFile);
      }
      else
      {
        instancePublicId = responseDicomAttachment.GetResourceByIndex(0).GetOneInstanceIdentifier();
      }
    }

    LOG(INFO) << "Will retrieve missing DICOM tags from instance: " << instancePublicId;

    // TODO-FIND: What do we do if the DICOM has been removed since the request?
    // Do we fail, or do we skip the resource?

    Json::Value tmpDicomAsJson;
    context.ReadDicomAsJson(tmpDicomAsJson, instancePublicId, missingTags /* ignoreTagLength */);

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


  void ResourceFinder::Execute(Json::Value& target,
                               ServerContext& context) const
  {
    FindResponse response;
    context.GetIndex().ExecuteFind(response, request_);

    target = Json::arrayValue;

    for (size_t i = 0; i < response.GetSize(); i++)
    {
      const FindResponse::Resource& resource = response.GetResourceByIndex(i);

      {
        Json::Value v;
        resource.DebugExport(v, request_);
        std::cout << v.toStyledString();
      }

      if (expand_)
      {
        Json::Value item;
        Expand(item, resource, context.GetIndex());

        if (hasRequestedTags_)
        {
          DicomMap requestedTags;
          InjectComputedTags(requestedTags, resource);

          std::set<DicomTag> missingTags = requestedTagsFromFileStorage_;
          InjectRequestedTags(requestedTags, missingTags, resource, ResourceType_Patient, requestedPatientTags_);
          InjectRequestedTags(requestedTags, missingTags, resource, ResourceType_Study, requestedStudyTags_);
          InjectRequestedTags(requestedTags, missingTags, resource, ResourceType_Series, requestedSeriesTags_);
          InjectRequestedTags(requestedTags, missingTags, resource, ResourceType_Instance, requestedInstanceTags_);

          if (!missingTags.empty())
          {
            if (!allowStorageAccess_)
            {
              throw OrthancException(ErrorCode_BadSequenceOfCalls,
                                     "Cannot add missing requested tags, as access to file storage is disallowed");
            }
            else
            {
              ReadMissingTagsFromStorageArea(requestedTags, context, request_, resource, missingTags);
            }
          }

          static const char* const REQUESTED_TAGS = "RequestedTags";
          item[REQUESTED_TAGS] = Json::objectValue;
          FromDcmtkBridge::ToJson(item[REQUESTED_TAGS], requestedTags, format_);
        }

        target.append(item);
      }
      else
      {
        target.append(resource.GetIdentifier());
      }
    }
  }


  bool ResourceFinder::ExecuteOneResource(Json::Value& target,
                                          ServerContext& context) const
  {
    Json::Value answer;
    Execute(answer, context);

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
