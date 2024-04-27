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


#pragma once

#include "../../../OrthancFramework/Sources/DicomFormat/DicomMap.h"
#include "../../../OrthancFramework/Sources/Enumerations.h"
#include "../../../OrthancFramework/Sources/FileStorage/FileInfo.h"
#include "../ServerEnumerations.h"
#include "OrthancIdentifiers.h"
#include "FindRequest.h"

#include <boost/noncopyable.hpp>
#include <deque>
#include <map>
#include <set>
#include <list>


namespace Orthanc
{
  class FindResponse : public boost::noncopyable
  {
  public:

    // TODO-FIND: does it actually make sense to retrieve revisions for metadata and attachments ?
    class StringWithRevision
    {
    private:
      std::string         value_;
      int64_t             revision_;
    public:
      StringWithRevision(const std::string& value,
                          int64_t revision) :
        value_(value),
        revision_(revision)
      {
      }

      StringWithRevision(const StringWithRevision& other) :
        value_(other.value_),
        revision_(other.revision_)
      {
      }

      StringWithRevision() :
        revision_(-1)
      {
      }

      const std::string& GetValue() const
      {
        return value_;
      }

      int64_t GetRevision() const
      {
        return revision_;
      }
    };


    class Item : public boost::noncopyable
    {
    private:
      FindRequest::ResponseContent          responseContent_;  // TODO REMOVE  // what has been requested
      ResourceType                          level_;   // TODO REMOVE
      OrthancIdentifiers                    identifiers_;
      std::unique_ptr<DicomMap>             dicomMap_;
      std::list<std::string>                children_;
      std::string                           childInstanceId_;
      std::set<std::string>                 labels_;      
      std::map<MetadataType, std::string>   metadata_;
      std::map<FileContentType, FileInfo>   attachments_;

    public:
      Item(FindRequest::ResponseContent responseContent,
           ResourceType level,
           const OrthancIdentifiers& identifiers) :
        responseContent_(responseContent),
        level_(level),
        identifiers_(identifiers)
      {
      }

      Item(FindRequest::ResponseContent responseContent,
           ResourceType level,
           DicomMap* dicomMap /* takes ownership */);

      ResourceType GetLevel() const
      {
        return level_;
      }

      const OrthancIdentifiers& GetIdentifiers() const
      {
        return identifiers_;
      }

      FindRequest::ResponseContent GetResponseContent() const
      {
        return responseContent_;
      }

      bool HasResponseContent(FindRequest::ResponseContent content) const
      {
        return (responseContent_ & content) == content;
      }

      void AddDicomTag(uint16_t group, uint16_t element, const std::string& value, bool isBinary);

      void AddMetadata(MetadataType metadata,
                       const std::string& value);
                       //int64_t revision);

      const std::map<MetadataType, std::string>& GetMetadata() const
      {
        return metadata_;
      }

      bool HasMetadata(MetadataType metadata) const
      {
        return metadata_.find(metadata) != metadata_.end();
      }

      bool LookupMetadata(std::string& value, /* int64_t revision, */
                          MetadataType metadata) const;

      void ListMetadata(std::set<MetadataType>& metadata) const;

      bool HasDicomMap() const
      {
        return dicomMap_.get() != NULL;
      }

      const DicomMap& GetDicomMap() const;

      void AddChild(const std::string& childId);

      const std::list<std::string>& GetChildren() const
      {
        return children_;
      }

      void AddLabel(const std::string& label)
      {
        labels_.insert(label);
      }

      const std::set<std::string>& GetLabels() const
      {
        return labels_;
      }

      void AddAttachment(const FileInfo& attachment)
      {
        attachments_[attachment.GetContentType()] = attachment;
      }

      const std::map<FileContentType, FileInfo>& GetAttachments() const
      {
        return attachments_;
      }

      bool LookupAttachment(FileInfo& target, FileContentType type) const
      {
        std::map<FileContentType, FileInfo>::const_iterator it = attachments_.find(type);
        if (it != attachments_.end())
        {
          target = it->second;
          return true;
        }

        return false;
      }

      void SetIdentifier(ResourceType level,
                         const std::string& id)
      {
        identifiers_.SetLevel(level, id);
      }

      // TODO-FIND: add other getters and setters
    };

  private:
    std::deque<Item*>  items_;

  public:
    ~FindResponse();

    void Add(Item* item /* takes ownership */);

    size_t GetSize() const
    {
      return items_.size();
    }

    const Item& GetItem(size_t index) const;
  };
}
