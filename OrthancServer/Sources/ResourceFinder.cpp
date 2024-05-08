/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2024 Osimis S.A., Belgium
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
#include "../../OrthancFramework/Sources/OrthancException.h"
#include "../../OrthancFramework/Sources/SerializationToolbox.h"
#include "ServerContext.h"
#include "ServerIndex.h"


namespace Orthanc
{
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

    std::list<std::string> values;
    if (!resource.LookupChildrenMetadata(values, MetadataType_Instance_IndexInSeries))
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    std::set<int64_t> instances;

    for (std::list<std::string>::const_iterator
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

    if (!requestedTags_.empty())
    {
      throw OrthancException(ErrorCode_NotImplemented);
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
      const std::set<std::string>& children = resource.GetChildrenIdentifiers();

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

        static const char* EXPECTED_NUMBER_OF_INSTANCES = "ExpectedNumberOfInstances";

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

        static const char* INDEX_IN_SERIES = "IndexInSeries";

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
      static const char* const MAIN_DICOM_TAGS = "MainDicomTags";
      static const char* const PATIENT_MAIN_DICOM_TAGS = "PatientMainDicomTags";

      // TODO-FIND : (expandFlags & ExpandResourceFlags_IncludeMainDicomTags)
      DicomMap allMainDicomTags;
      resource.GetMainDicomTags(allMainDicomTags, resource.GetLevel());

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

      /*
        TODO-FIND

        if (!requestedTags_.empty())
        {
        static const char* const REQUESTED_TAGS = "RequestedTags";

        DicomMap tags;
        resource.GetMainDicomTags().ExtractTags(tags, requestedTags);

        target[REQUESTED_TAGS] = Json::objectValue;
        FromDcmtkBridge::ToJson(target[REQUESTED_TAGS], tags, format);
        }
      */
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
    includeAllMetadata_(false)
  {
    if (expand)
    {
      request_.SetRetrieveMainDicomTags(level, true);
      request_.SetRetrieveMetadata(level, true);
      request_.SetRetrieveLabels(true);

      if (level == ResourceType_Series)
      {
        request_.AddRetrieveChildrenMetadata(MetadataType_Instance_IndexInSeries); // required for the SeriesStatus
      }

      if (level == ResourceType_Instance)
      {
        request_.SetRetrieveAttachments(true); // for FileSize & FileUuid
      }
      else
      {
        request_.SetRetrieveChildrenIdentifiers(true);
      }

      if (level != ResourceType_Patient)
      {
        request_.SetRetrieveParentIdentifier(true);
      }
    }
  }


  void ResourceFinder::Execute(Json::Value& target,
                               ServerContext& context)
  {
    FindResponse response;
    context.GetIndex().ExecuteFind(response, request_);

    target = Json::arrayValue;

    if (expand_)
    {
      for (size_t i = 0; i < response.GetSize(); i++)
      {
        Json::Value item;
        Expand(item, response.GetResource(i), context.GetIndex());
        target.append(item);
      }
    }
    else
    {
      for (size_t i = 0; i < response.GetSize(); i++)
      {
        target.append(response.GetResource(i).GetIdentifier());
      }
    }
  }
}
