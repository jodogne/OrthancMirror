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


#include "FindResponse.h"

#include "../../../OrthancFramework/Sources/DicomFormat/DicomArray.h"
#include "../../../OrthancFramework/Sources/OrthancException.h"
#include "../../../OrthancFramework/Sources/SerializationToolbox.h"

#include <boost/lexical_cast.hpp>
#include <cassert>


namespace Orthanc
{
  class FindResponse::MainDicomTagsAtLevel::DicomValue : public boost::noncopyable
  {
  public:
    enum ValueType
    {
      ValueType_String,
      ValueType_Null
    };

  private:
    ValueType     type_;
    std::string   value_;

  public:
    DicomValue(ValueType type,
               const std::string& value) :
      type_(type),
      value_(value)
    {
    }

    ValueType GetType() const
    {
      return type_;
    }

    const std::string& GetValue() const
    {
      switch (type_)
      {
        case ValueType_Null:
          throw OrthancException(ErrorCode_BadSequenceOfCalls);

        case ValueType_String:
          return value_;

        default:
          throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
    }
  };


  FindResponse::MainDicomTagsAtLevel::~MainDicomTagsAtLevel()
  {
    for (MainDicomTags::iterator it = mainDicomTags_.begin(); it != mainDicomTags_.end(); ++it)
    {
      assert(it->second != NULL);
      delete it->second;
    }
  }


  void FindResponse::MainDicomTagsAtLevel::AddNullDicomTag(uint16_t group,
                                                           uint16_t element)
  {
    const DicomTag tag(group, element);

    if (mainDicomTags_.find(tag) == mainDicomTags_.end())
    {
      mainDicomTags_[tag] = new DicomValue(DicomValue::ValueType_Null, "");
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }


  void FindResponse::MainDicomTagsAtLevel::AddStringDicomTag(uint16_t group,
                                                             uint16_t element,
                                                             const std::string& value)
  {
    const DicomTag tag(group, element);

    if (mainDicomTags_.find(tag) == mainDicomTags_.end())
    {
      mainDicomTags_[tag] = new DicomValue(DicomValue::ValueType_String, value);
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }


  void FindResponse::MainDicomTagsAtLevel::Export(DicomMap& target) const
  {
    for (MainDicomTags::const_iterator it = mainDicomTags_.begin(); it != mainDicomTags_.end(); ++it)
    {
      assert(it->second != NULL);

      switch (it->second->GetType())
      {
        case DicomValue::ValueType_String:
          target.SetValue(it->first, it->second->GetValue(), false /* not binary */);
          break;

        case DicomValue::ValueType_Null:
          target.SetNullValue(it->first);
          break;

        default:
          throw OrthancException(ErrorCode_InternalError);
      }
    }
  }


  FindResponse::ChildrenInformation::~ChildrenInformation()
  {
    for (MetadataValues::iterator it = metadataValues_.begin(); it != metadataValues_.end(); ++it)
    {
      assert(it->second != NULL);
      delete it->second;
    }

    for (MainDicomTagValues::iterator it = mainDicomTagValues_.begin(); it != mainDicomTagValues_.end(); ++it)
    {
      assert(it->second != NULL);
      delete it->second;
    }
  }


  void FindResponse::ChildrenInformation::AddIdentifier(const std::string& identifier)
  {
    // The same identifier can be added through AddChildIdentifier and through AddOneInstanceIdentifier
    identifiers_.insert(identifier);
  }


  void FindResponse::ChildrenInformation::AddMetadataValue(MetadataType metadata,
                                                           const std::string& value)
  {
    MetadataValues::iterator found = metadataValues_.find(metadata);

    if (found == metadataValues_.end())
    {
      std::set<std::string> s;
      s.insert(value);
      metadataValues_[metadata] = new std::set<std::string>(s);
    }
    else
    {
      assert(found->second != NULL);
      found->second->insert(value);
    }
  }


  void FindResponse::ChildrenInformation::GetMetadataValues(std::set<std::string>& values,
                                                            MetadataType metadata) const
  {
    MetadataValues::const_iterator found = metadataValues_.find(metadata);

    if (found == metadataValues_.end())
    {
      values.clear();
    }
    else
    {
      assert(found->second != NULL);
      values = *found->second;
    }
  }


  void FindResponse::ChildrenInformation::AddMainDicomTagValue(const DicomTag& tag,
                                                               const std::string& value)
  {
    MainDicomTagValues::iterator found = mainDicomTagValues_.find(tag);

    if (found == mainDicomTagValues_.end())
    {
      std::set<std::string> s;
      s.insert(value);
      mainDicomTagValues_[tag] = new std::set<std::string>(s);
    }
    else
    {
      assert(found->second != NULL);
      found->second->insert(value);
    }
  }


  void FindResponse::ChildrenInformation::GetMainDicomTagValues(std::set<std::string>& values,
                                                                const DicomTag& tag) const
  {
    MainDicomTagValues::const_iterator found = mainDicomTagValues_.find(tag);

    if (found == mainDicomTagValues_.end())
    {
      values.clear();
    }
    else
    {
      assert(found->second != NULL);
      values = *found->second;
    }
  }


  FindResponse::ChildrenInformation& FindResponse::Resource::GetChildrenInformation(ResourceType level)
  {
    switch (level)
    {
      case ResourceType_Study:
        if (level_ == ResourceType_Patient)
        {
          return childrenStudiesInformation_;
        }
        else
        {
          throw OrthancException(ErrorCode_ParameterOutOfRange);
        }

      case ResourceType_Series:
        if (level_ == ResourceType_Patient ||
            level_ == ResourceType_Study)
        {
          return childrenSeriesInformation_;
        }
        else
        {
          throw OrthancException(ErrorCode_ParameterOutOfRange);
        }

      case ResourceType_Instance:
        if (level_ == ResourceType_Patient ||
            level_ == ResourceType_Study ||
            level_ == ResourceType_Series)
        {
          return childrenInstancesInformation_;
        }
        else
        {
          throw OrthancException(ErrorCode_ParameterOutOfRange);
        }

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }


  FindResponse::MainDicomTagsAtLevel& FindResponse::Resource::GetMainDicomTagsAtLevel(ResourceType level)
  {
    if (!IsResourceLevelAboveOrEqual(level, level_))
    {
      throw OrthancException(ErrorCode_BadParameterType);
    }

    switch (level)
    {
      case ResourceType_Patient:
        return mainDicomTagsPatient_;

      case ResourceType_Study:
        return mainDicomTagsStudy_;

      case ResourceType_Series:
        return mainDicomTagsSeries_;

      case ResourceType_Instance:
        return mainDicomTagsInstance_;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }


  void FindResponse::Resource::GetAllMainDicomTags(DicomMap& target) const
  {
    switch (level_)
    {
      // Don't reorder or add "break" below
      case ResourceType_Instance:
        mainDicomTagsInstance_.Export(target);

      case ResourceType_Series:
        mainDicomTagsSeries_.Export(target);

      case ResourceType_Study:
        mainDicomTagsStudy_.Export(target);

      case ResourceType_Patient:
        mainDicomTagsPatient_.Export(target);
        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }


  void FindResponse::Resource::AddMetadata(ResourceType level,
                                           MetadataType metadata,
                                           const std::string& value,
                                           int64_t revision)
  {
    std::map<MetadataType, MetadataContent>& m = GetMetadata(level);

    if (m.find(metadata) != m.end())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);  // Metadata already present
    }
    else
    {
      m[metadata] = MetadataContent(value, revision);
    }
  }


  std::map<MetadataType, FindResponse::MetadataContent>& FindResponse::Resource::GetMetadata(ResourceType level)
  {
    if (!IsResourceLevelAboveOrEqual(level, level_))
    {
      throw OrthancException(ErrorCode_BadParameterType);
    }

    switch (level)
    {
      case ResourceType_Patient:
        return metadataPatient_;

      case ResourceType_Study:
        return metadataStudy_;

      case ResourceType_Series:
        return metadataSeries_;

      case ResourceType_Instance:
        return metadataInstance_;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }


  bool FindResponse::Resource::LookupMetadata(std::string& value,
                                              ResourceType level,
                                              MetadataType metadata) const
  {
    const std::map<MetadataType, MetadataContent>& m = GetMetadata(level);

    std::map<MetadataType, MetadataContent>::const_iterator found = m.find(metadata);

    if (found == m.end())
    {
      return false;
    }
    else
    {
      value = found->second.GetValue();
      return true;
    }
  }


  bool FindResponse::Resource::LookupMetadata(std::string& value,
                                              int64_t& revision,
                                              ResourceType level,
                                              MetadataType metadata) const
  {
    const std::map<MetadataType, MetadataContent>& m = GetMetadata(level);

    std::map<MetadataType, MetadataContent>::const_iterator found = m.find(metadata);

    if (found == m.end())
    {
      return false;
    }
    else
    {
      value = found->second.GetValue();
      revision = found->second.GetRevision();
      return true;
    }
  }


  void FindResponse::Resource::SetParentIdentifier(const std::string& id)
  {
    if (level_ == ResourceType_Patient)
    {
      throw OrthancException(ErrorCode_BadParameterType);
    }
    else if (HasParentIdentifier())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      parentIdentifier_.reset(new std::string(id));
    }
  }


  const std::string& FindResponse::Resource::GetParentIdentifier() const
  {
    if (level_ == ResourceType_Patient)
    {
      throw OrthancException(ErrorCode_BadParameterType);
    }
    else if (HasParentIdentifier())
    {
      return *parentIdentifier_;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }


  bool FindResponse::Resource::HasParentIdentifier() const
  {
    if (level_ == ResourceType_Patient)
    {
      throw OrthancException(ErrorCode_BadParameterType);
    }
    else
    {
      return parentIdentifier_.get() != NULL;
    }
  }


  void FindResponse::Resource::AddLabel(const std::string& label)
  {
    if (labels_.find(label) == labels_.end())
    {
      labels_.insert(label);
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }


  void FindResponse::Resource::AddAttachment(const FileInfo& attachment,
                                             int64_t revision)
  {
    if (attachments_.find(attachment.GetContentType()) == attachments_.end() &&
        revisions_.find(attachment.GetContentType()) == revisions_.end())
    {
      attachments_[attachment.GetContentType()] = attachment;
      revisions_[attachment.GetContentType()] = revision;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }


  bool FindResponse::Resource::LookupAttachment(FileInfo& target,
                                                int64_t& revision,
                                                FileContentType type) const
  {
    std::map<FileContentType, FileInfo>::const_iterator it = attachments_.find(type);
    std::map<FileContentType, int64_t>::const_iterator it2 = revisions_.find(type);

    if (it != attachments_.end())
    {
      if (it2 != revisions_.end())
      {
        target = it->second;
        revision = it2->second;
        return true;
      }
      else
      {
        throw OrthancException(ErrorCode_InternalError);
      }
    }
    else
    {
      if (it2 == revisions_.end())
      {
        return false;
      }
      else
      {
        throw OrthancException(ErrorCode_InternalError);
      }
    }
  }


  void FindResponse::Resource::ListAttachments(std::set<FileContentType>& target) const
  {
    target.clear();

    for (std::map<FileContentType, FileInfo>::const_iterator
           it = attachments_.begin(); it != attachments_.end(); ++it)
    {
      target.insert(it->first);
    }
  }


  void FindResponse::Resource::SetOneInstanceMetadataAndAttachments(const std::string& instancePublicId,
                                                                    const std::map<MetadataType, std::string>& metadata,
                                                                    const std::map<FileContentType, FileInfo>& attachments)
  {
    if (hasOneInstanceMetadataAndAttachments_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else if (instancePublicId.empty())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
    else
    {
      hasOneInstanceMetadataAndAttachments_ = true;
      oneInstancePublicId_ = instancePublicId;
      oneInstanceMetadata_ = metadata;
      oneInstanceAttachments_ = attachments;
    }
  }


  void FindResponse::Resource::SetOneInstancePublicId(const std::string& instancePublicId)
  {
    SetOneInstanceMetadataAndAttachments(instancePublicId, std::map<MetadataType, std::string>(),
                                         std::map<FileContentType, FileInfo>());
  }


  void FindResponse::Resource::AddOneInstanceMetadata(MetadataType metadata,
                                                      const std::string& value)
  {
    if (hasOneInstanceMetadataAndAttachments_)
    {
      if (oneInstanceMetadata_.find(metadata) == oneInstanceMetadata_.end())
      {
        oneInstanceMetadata_[metadata] = value;
      }
      else
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls, "Metadata already exists");
      }
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }


  void FindResponse::Resource::AddOneInstanceAttachment(const FileInfo& attachment)
  {
    if (hasOneInstanceMetadataAndAttachments_)
    {
      if (oneInstanceAttachments_.find(attachment.GetContentType()) == oneInstanceAttachments_.end())
      {
        oneInstanceAttachments_[attachment.GetContentType()] = attachment;
      }
      else
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls, "Attachment already exists");
      }
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }


  const std::string& FindResponse::Resource::GetOneInstancePublicId() const
  {
    if (hasOneInstanceMetadataAndAttachments_)
    {
      return oneInstancePublicId_;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }


  const std::map<MetadataType, std::string>& FindResponse::Resource::GetOneInstanceMetadata() const
  {
    if (hasOneInstanceMetadataAndAttachments_)
    {
      return oneInstanceMetadata_;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }


  const std::map<FileContentType, FileInfo>& FindResponse::Resource::GetOneInstanceAttachments() const
  {
    if (hasOneInstanceMetadataAndAttachments_)
    {
      return oneInstanceAttachments_;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }


  static void DebugDicomMap(Json::Value& target,
                            const DicomMap& m)
  {
    DicomArray a(m);
    for (size_t i = 0; i < a.GetSize(); i++)
    {
      if (a.GetElement(i).GetValue().IsNull())
      {
        target[a.GetElement(i).GetTag().Format()] = Json::nullValue;
      }
      else if (a.GetElement(i).GetValue().IsString())
      {
        target[a.GetElement(i).GetTag().Format()] = a.GetElement(i).GetValue().GetContent();
      }
      else
      {
        throw OrthancException(ErrorCode_InternalError);
      }
    }
  }


  static void DebugMetadata(Json::Value& target,
                            const std::map<MetadataType, std::string>& m)
  {
    target = Json::objectValue;

    for (std::map<MetadataType, std::string>::const_iterator it = m.begin(); it != m.end(); ++it)
    {
      target[EnumerationToString(it->first)] = it->second;
    }
  }


  static void DebugMetadata(Json::Value& target,
                            const std::map<MetadataType, FindResponse::MetadataContent>& m)
  {
    target = Json::objectValue;

    for (std::map<MetadataType, FindResponse::MetadataContent>::const_iterator it = m.begin(); it != m.end(); ++it)
    {
      target[EnumerationToString(it->first)] = it->second.GetValue();
    }
  }


  static void DebugAddAttachment(Json::Value& target,
                                 const FileInfo& info)
  {
    Json::Value u = Json::arrayValue;
    u.append(info.GetUuid());
    u.append(static_cast<Json::UInt64>(info.GetUncompressedSize()));
    target[EnumerationToString(info.GetContentType())] = u;
  }


  static void DebugAttachments(Json::Value& target,
                               const std::map<FileContentType, FileInfo>& attachments)
  {
    target = Json::objectValue;
    for (std::map<FileContentType, FileInfo>::const_iterator it = attachments.begin();
         it != attachments.end(); ++it)
    {
      if (it->first != it->second.GetContentType())
      {
        throw OrthancException(ErrorCode_DatabasePlugin);
      }
      else
      {
        DebugAddAttachment(target, it->second);
      }
    }
  }


  static void DebugSetOfStrings(Json::Value& target,
                                const std::set<std::string>& values)
  {
    target = Json::arrayValue;
    for (std::set<std::string>::const_iterator it = values.begin(); it != values.end(); ++it)
    {
      target.append(*it);
    }
  }


  void FindResponse::Resource::DebugExport(Json::Value& target,
                                           const FindRequest& request) const
  {
    target = Json::objectValue;

    target["Level"] = EnumerationToString(GetLevel());
    target["ID"] = GetIdentifier();

    if (request.IsRetrieveParentIdentifier())
    {
      target["ParentID"] = GetParentIdentifier();
    }

    if (request.IsRetrieveMainDicomTags())
    {
      DicomMap m;
      GetMainDicomTags(m, request.GetLevel());
      DebugDicomMap(target[EnumerationToString(GetLevel())]["MainDicomTags"], m);
    }

    if (request.IsRetrieveMetadata())
    {
      DebugMetadata(target[EnumerationToString(GetLevel())]["Metadata"], GetMetadata(request.GetLevel()));
    }

    static const ResourceType levels[4] = { ResourceType_Patient, ResourceType_Study, ResourceType_Series, ResourceType_Instance };

    for (size_t i = 0; i < 4; i++)
    {
      const char* level = EnumerationToString(levels[i]);

      if (levels[i] != request.GetLevel() &&
          IsResourceLevelAboveOrEqual(levels[i], request.GetLevel()))
      {
        if (request.GetParentSpecification(levels[i]).IsRetrieveMainDicomTags())
        {
          DicomMap m;
          GetMainDicomTags(m, levels[i]);
          DebugDicomMap(target[level]["MainDicomTags"], m);
        }

        if (request.GetParentSpecification(levels[i]).IsRetrieveMetadata())
        {
          DebugMetadata(target[level]["Metadata"], GetMetadata(levels[i]));
        }
      }

      if (levels[i] != request.GetLevel() &&
          IsResourceLevelAboveOrEqual(request.GetLevel(), levels[i]))
      {
        if (request.GetChildrenSpecification(levels[i]).IsRetrieveIdentifiers())
        {
          DebugSetOfStrings(target[level]["Identifiers"], GetChildrenInformation(levels[i]).GetIdentifiers());
        }

        const std::set<MetadataType>& metadata = request.GetChildrenSpecification(levels[i]).GetMetadata();
        for (std::set<MetadataType>::const_iterator it = metadata.begin(); it != metadata.end(); ++it)
        {
          std::set<std::string> values;
          GetChildrenInformation(levels[i]).GetMetadataValues(values, *it);
          DebugSetOfStrings(target[level]["Metadata"][EnumerationToString(*it)], values);
        }

        const std::set<DicomTag>& tags = request.GetChildrenSpecification(levels[i]).GetMainDicomTags();
        for (std::set<DicomTag>::const_iterator it = tags.begin(); it != tags.end(); ++it)
        {
          std::set<std::string> values;
          GetChildrenInformation(levels[i]).GetMainDicomTagValues(values, *it);
          DebugSetOfStrings(target[level]["MainDicomTags"][it->Format()], values);
        }
      }
    }

    if (request.IsRetrieveLabels())
    {
      DebugSetOfStrings(target["Labels"], labels_);
    }

    if (request.IsRetrieveAttachments())
    {
      DebugAttachments(target["Attachments"], attachments_);
    }

    if (request.GetLevel() != ResourceType_Instance &&
        request.IsRetrieveOneInstanceMetadataAndAttachments())
    {
      DebugMetadata(target["OneInstance"]["Metadata"], GetOneInstanceMetadata());
      DebugAttachments(target["OneInstance"]["Attachments"], GetOneInstanceAttachments());
    }
  }


  FindResponse::~FindResponse()
  {
    for (size_t i = 0; i < items_.size(); i++)
    {
      assert(items_[i] != NULL);
      delete items_[i];
    }
  }


  void FindResponse::Add(Resource* item /* takes ownership */)
  {
    std::unique_ptr<Resource> protection(item);

    if (item == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
    else if (!items_.empty() &&
             items_[0]->GetLevel() != item->GetLevel())
    {
      throw OrthancException(ErrorCode_BadParameterType, "A find response must only contain resources of the same type");
    }
    else
    {
      const std::string& id = item->GetIdentifier();
      int64_t internalId = item->GetInternalId();

      if (identifierIndex_.find(id) == identifierIndex_.end() && internalIdIndex_.find(internalId) == internalIdIndex_.end())
      {
        items_.push_back(protection.release());
        identifierIndex_[id] = item;
        internalIdIndex_[internalId] = item;
      }
      else
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls, "This resource has already been added: " + id + "/" + boost::lexical_cast<std::string>(internalId));
      }
    }
  }


  const FindResponse::Resource& FindResponse::GetResourceByIndex(size_t index) const
  {
    if (index >= items_.size())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
    else
    {
      assert(items_[index] != NULL);
      return *items_[index];
    }
  }


  FindResponse::Resource& FindResponse::GetResourceByIdentifier(const std::string& id)
  {
    IdentifierIndex::const_iterator found = identifierIndex_.find(id);

    if (found == identifierIndex_.end())
    {
      throw OrthancException(ErrorCode_InexistentItem);
    }
    else
    {
      assert(found->second != NULL);
      return *found->second;
    }
  }


  FindResponse::Resource& FindResponse::GetResourceByInternalId(int64_t internalId)
  {
    InternalIdIndex::const_iterator found = internalIdIndex_.find(internalId);

    if (found == internalIdIndex_.end())
    {
      throw OrthancException(ErrorCode_InexistentItem);
    }
    else
    {
      assert(found->second != NULL);
      return *found->second;
    }
  }
}
