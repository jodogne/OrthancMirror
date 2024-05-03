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

#include "../../../OrthancFramework/Sources/DicomFormat/DicomInstanceHasher.h"
#include "../../../OrthancFramework/Sources/OrthancException.h"

#include <cassert>


namespace Orthanc
{
  class FindResponse::DicomTagsAtLevel::DicomValue : public boost::noncopyable
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


  FindResponse::DicomTagsAtLevel::~DicomTagsAtLevel()
  {
    for (Content::iterator it = content_.begin(); it != content_.end(); ++it)
    {
      assert(it->second != NULL);
      delete it->second;
    }
  }


  void FindResponse::DicomTagsAtLevel::AddNullValue(uint16_t group,
                                                    uint16_t element)
  {
    const DicomTag tag(group, element);

    if (content_.find(tag) == content_.end())
    {
      content_[tag] = new DicomValue(DicomValue::ValueType_Null, "");
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }


  void FindResponse::DicomTagsAtLevel::AddStringValue(uint16_t group,
                                                      uint16_t element,
                                                      const std::string& value)
  {
    const DicomTag tag(group, element);

    if (content_.find(tag) == content_.end())
    {
      content_[tag] = new DicomValue(DicomValue::ValueType_String, value);
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }


  void FindResponse::DicomTagsAtLevel::Fill(DicomMap& target) const
  {
    for (Content::const_iterator it = content_.begin(); it != content_.end(); ++it)
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


  void FindResponse::ChildrenAtLevel::AddIdentifier(const std::string& identifier)
  {
    if (identifiers_.find(identifier) == identifiers_.end())
    {
      identifiers_.insert(identifier);
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }


  FindResponse::DicomTagsAtLevel& FindResponse::Item::GetDicomTagsAtLevel(ResourceType level)
  {
    switch (level)
    {
      case ResourceType_Patient:
        return patientTags_;

      case ResourceType_Study:
        return studyTags_;

      case ResourceType_Series:
        return seriesTags_;

      case ResourceType_Instance:
        return instanceTags_;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }


  FindResponse::ChildrenAtLevel& FindResponse::Item::GetChildrenAtLevel(ResourceType level)
  {
    switch (level)
    {
      case ResourceType_Study:
        if (level_ == ResourceType_Patient)
        {
          return childrenStudies_;
        }
        else
        {
          throw OrthancException(ErrorCode_BadParameterType);
        }

      case ResourceType_Series:
        if (level_ == ResourceType_Patient ||
            level_ == ResourceType_Study)
        {
          return childrenSeries_;
        }
        else
        {
          throw OrthancException(ErrorCode_BadParameterType);
        }

      case ResourceType_Instance:
        if (level_ == ResourceType_Patient ||
            level_ == ResourceType_Study ||
            level_ == ResourceType_Series)
        {
          return childrenInstances_;
        }
        else
        {
          throw OrthancException(ErrorCode_BadParameterType);
        }

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }


  void FindResponse::Item::AddMetadata(MetadataType metadata,
                                       const std::string& value)
  {
    if (metadata_.find(metadata) != metadata_.end())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);  // Metadata already present
    }
    else
    {
      metadata_[metadata] = value;
    }
  }


  bool FindResponse::Item::LookupMetadata(std::string& value,
                                          MetadataType metadata) const
  {
    std::map<MetadataType, std::string>::const_iterator found = metadata_.find(metadata);

    if (found == metadata_.end())
    {
      return false;
    }
    else
    {
      value = found->second;
      return true;
    }
  }


  void FindResponse::Item::ListMetadata(std::set<MetadataType>& target) const
  {
    target.clear();

    for (std::map<MetadataType, std::string>::const_iterator it = metadata_.begin(); it != metadata_.end(); ++it)
    {
      target.insert(it->first);
    }
  }

  const std::string& FindResponse::Item::GetParentIdentifier() const
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


  void FindResponse::Item::SetParentIdentifier(const std::string& id)
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


  bool FindResponse::Item::HasParentIdentifier() const
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


  void FindResponse::Item::AddLabel(const std::string& label)
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


  void FindResponse::Item::AddAttachment(const FileInfo& attachment)
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


  bool FindResponse::Item::LookupAttachment(FileInfo& target, FileContentType type) const
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


  FindResponse::~FindResponse()
  {
    for (size_t i = 0; i < items_.size(); i++)
    {
      assert(items_[i] != NULL);
      delete items_[i];
    }
  }


  void FindResponse::Add(Item* item /* takes ownership */)
  {
    std::unique_ptr<Item> protection(item);

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


  const FindResponse::Item& FindResponse::GetItem(size_t index) const
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


  FindResponse::Item& FindResponse::GetItem(const std::string& id)
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
