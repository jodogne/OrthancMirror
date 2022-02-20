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


#pragma once

#include "../../../OrthancFramework/Sources/DicomFormat/DicomMap.h"
#include "../ServerEnumerations.h"

#include <boost/noncopyable.hpp>
#include <list>


namespace Orthanc
{
  namespace Compatibility
  {
    class ISetResourcesContent;
  }
  
  class ResourcesContent : public boost::noncopyable
  {
  public:
    struct TagValue
    {
      int64_t      resourceId_;
      bool         isIdentifier_;
      DicomTag     tag_;
      std::string  value_;

      TagValue(int64_t resourceId,
               bool isIdentifier,
               const DicomTag& tag,
               const std::string& value) :
        resourceId_(resourceId),
        isIdentifier_(isIdentifier),
        tag_(tag),
        value_(value)
      {
      }
    };

    struct Metadata
    {
      int64_t       resourceId_;
      MetadataType  metadata_;
      std::string   value_;

      Metadata(int64_t  resourceId,
               MetadataType metadata,
               const std::string& value) :
        resourceId_(resourceId),
        metadata_(metadata),
        value_(value)
      {
      }
    };

    typedef std::list<TagValue>  ListTags;
    typedef std::list<Metadata>  ListMetadata;
    
  private:
    bool           isNewResource_;
    ListTags       tags_;
    ListMetadata   metadata_;

  public:
    explicit ResourcesContent(bool isNewResource) :
      isNewResource_(isNewResource)
    {
    }
    
    void AddMainDicomTag(int64_t resourceId,
                         const DicomTag& tag,
                         const std::string& value)
    {
      tags_.push_back(TagValue(resourceId, false, tag, value));
    }

    void AddIdentifierTag(int64_t resourceId,
                          const DicomTag& tag,
                          const std::string& value)
    {
      tags_.push_back(TagValue(resourceId, true, tag, value));
    }

    void AddMetadata(int64_t resourceId,
                     MetadataType metadata,
                     const std::string& value);

    void AddResource(int64_t resource,
                     ResourceType level,
                     const DicomMap& dicomSummary);

    // WARNING: The database should be locked with a transaction!
    void Store(Compatibility::ISetResourcesContent& target) const;

    const ListTags& GetListTags() const
    {
      return tags_;
    }

    const ListMetadata& GetListMetadata() const
    {
      return metadata_;
    }
  };
}
