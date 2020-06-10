/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
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


#pragma once

#include "../../Core/DicomFormat/DicomMap.h"
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
    ListTags       tags_;
    ListMetadata   metadata_;

  public:
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
                     const std::string& value)
    {
      metadata_.push_back(Metadata(resourceId, metadata, value));
    }

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
