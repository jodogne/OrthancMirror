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


  void FindResponse::Resource::AddChildIdentifier(const std::string& identifier)
  {
    if (childrenIdentifiers_.find(identifier) == childrenIdentifiers_.end())
    {
      childrenIdentifiers_.insert(identifier);
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
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


  void FindResponse::Resource::AddMetadata(ResourceType level,
                                           MetadataType metadata,
                                           const std::string& value)
  {
    std::map<MetadataType, std::string>& m = GetMetadata(level);

    if (m.find(metadata) != m.end())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);  // Metadata already present
    }
    else
    {
      m[metadata] = value;
    }
  }


  std::map<MetadataType, std::string>& FindResponse::Resource::GetMetadata(ResourceType level)
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
    const std::map<MetadataType, std::string>& m = GetMetadata(level);

    std::map<MetadataType, std::string>::const_iterator found = m.find(metadata);

    if (found == m.end())
    {
      return false;
    }
    else
    {
      value = found->second;
      return true;
    }
  }


  FindResponse::Resource::~Resource()
  {
    for (ChildrenMetadata::iterator it = childrenMetadata_.begin(); it != childrenMetadata_.end(); ++it)
    {
      assert(it->second != NULL);
      delete it->second;
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


  void FindResponse::Resource::AddAttachment(const FileInfo& attachment)
  {
    if (attachments_.find(attachment.GetContentType()) == attachments_.end())
    {
      attachments_[attachment.GetContentType()] = attachment;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }


  bool FindResponse::Resource::LookupAttachment(FileInfo& target, FileContentType type) const
  {
    std::map<FileContentType, FileInfo>::const_iterator it = attachments_.find(type);
    if (it != attachments_.end())
    {
      target = it->second;
      return true;
    }
    else
    {
      return false;
    }
  }


  void FindResponse::Resource::AddChildrenMetadata(MetadataType metadata,
                                                   const std::list<std::string>& values)
  {
    if (childrenMetadata_.find(metadata) == childrenMetadata_.end())
    {
      childrenMetadata_[metadata] = new std::list<std::string>(values);
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }


  bool FindResponse::Resource::LookupChildrenMetadata(std::list<std::string>& values,
                                                      MetadataType metadata) const
  {
    ChildrenMetadata::const_iterator found = childrenMetadata_.find(metadata);
    if (found == childrenMetadata_.end())
    {
      return false;
    }
    else
    {
      assert(found->second != NULL);
      values = *found->second;
      return true;
    }
  }


  void FindResponse::Resource::SetOneInstanceIdentifier(const std::string& id)
  {
    if (level_ == ResourceType_Instance)
    {
      throw OrthancException(ErrorCode_BadParameterType);
    }
    else if (HasOneInstanceIdentifier())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      oneInstanceIdentifier_.reset(new std::string(id));
    }
  }


  const std::string& FindResponse::Resource::GetOneInstanceIdentifier() const
  {
    if (level_ == ResourceType_Instance)
    {
      throw OrthancException(ErrorCode_BadParameterType);
    }
    else if (HasOneInstanceIdentifier())
    {
      return *oneInstanceIdentifier_;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }


  bool FindResponse::Resource::HasOneInstanceIdentifier() const
  {
    if (level_ == ResourceType_Instance)
    {
      throw OrthancException(ErrorCode_BadParameterType);
    }
    else
    {
      return oneInstanceIdentifier_.get() != NULL;
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


  static void DebugAddAttachment(Json::Value& target,
                                 const FileInfo& info)
  {
    Json::Value u = Json::arrayValue;
    u.append(info.GetUuid());
    u.append(static_cast<Json::UInt64>(info.GetUncompressedSize()));
    target[EnumerationToString(info.GetContentType())] = u;
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

    if (request.IsRetrieveMainDicomTags(ResourceType_Patient))
    {
      DicomMap m;
      GetMainDicomTags(m, ResourceType_Patient);
      DebugDicomMap(target["Patient"]["MainDicomTags"], m);
    }

    if (request.IsRetrieveMetadata(ResourceType_Patient))
    {
      DebugMetadata(target["Patient"]["Metadata"], GetMetadata(ResourceType_Patient));
    }

    if (request.GetLevel() != ResourceType_Patient)
    {
      if (request.IsRetrieveMainDicomTags(ResourceType_Study))
      {
        DicomMap m;
        GetMainDicomTags(m, ResourceType_Study);
        DebugDicomMap(target["Study"]["MainDicomTags"], m);
      }

      if (request.IsRetrieveMetadata(ResourceType_Study))
      {
        DebugMetadata(target["Study"]["Metadata"], GetMetadata(ResourceType_Study));
      }
    }

    if (request.GetLevel() != ResourceType_Patient &&
        request.GetLevel() != ResourceType_Study)
    {
      if (request.IsRetrieveMainDicomTags(ResourceType_Series))
      {
        DicomMap m;
        GetMainDicomTags(m, ResourceType_Series);
        DebugDicomMap(target["Series"]["MainDicomTags"], m);
      }

      if (request.IsRetrieveMetadata(ResourceType_Series))
      {
        DebugMetadata(target["Series"]["Metadata"], GetMetadata(ResourceType_Series));
      }
    }

    if (request.GetLevel() != ResourceType_Patient &&
        request.GetLevel() != ResourceType_Study &&
        request.GetLevel() != ResourceType_Series)
    {
      if (request.IsRetrieveMainDicomTags(ResourceType_Instance))
      {
        DicomMap m;
        GetMainDicomTags(m, ResourceType_Instance);
        DebugDicomMap(target["Instance"]["MainDicomTags"], m);
      }

      if (request.IsRetrieveMetadata(ResourceType_Instance))
      {
        DebugMetadata(target["Instance"]["Metadata"], GetMetadata(ResourceType_Instance));
      }
    }

    if (request.IsRetrieveChildrenIdentifiers())
    {
      Json::Value v = Json::arrayValue;
      for (std::set<std::string>::const_iterator it = childrenIdentifiers_.begin();
           it != childrenIdentifiers_.end(); ++it)
      {
        v.append(*it);
      }
      target["Children"] = v;
    }

    if (request.IsRetrieveLabels())
    {
      Json::Value v = Json::arrayValue;
      for (std::set<std::string>::const_iterator it = labels_.begin();
           it != labels_.end(); ++it)
      {
        v.append(*it);
      }
      target["Labels"] = v;
    }

    if (request.IsRetrieveAttachments())
    {
      Json::Value v = Json::objectValue;
      for (std::map<FileContentType, FileInfo>::const_iterator it = attachments_.begin();
           it != attachments_.end(); ++it)
      {
        if (it->first != it->second.GetContentType())
        {
          throw OrthancException(ErrorCode_DatabasePlugin);
        }
        else
        {
          DebugAddAttachment(v, it->second);
        }
      }
      target["Attachments"] = v;
    }

    for (std::set<MetadataType>::const_iterator it = request.GetRetrieveChildrenMetadata().begin();
         it != request.GetRetrieveChildrenMetadata().end(); ++it)
    {
      std::list<std::string> l;
      if (LookupChildrenMetadata(l, *it))
      {
        Json::Value v = Json::arrayValue;
        for (std::list<std::string>::const_iterator it2 = l.begin(); it2 != l.end(); ++it2)
        {
          v.append(*it2);
        }
        target["ChildrenMetadata"][EnumerationToString(*it)] = v;
      }
      else
      {
        throw OrthancException(ErrorCode_DatabasePlugin);
      }
    }

    if (request.IsRetrieveOneInstanceIdentifier())
    {
      target["OneInstance"] = GetOneInstanceIdentifier();
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

      if (index_.find(id) == index_.end())
      {
        items_.push_back(protection.release());
        index_[id] = item;
      }
      else
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls, "This resource has already been added: " + id);
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
    Index::const_iterator found = index_.find(id);

    if (found == index_.end())
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
