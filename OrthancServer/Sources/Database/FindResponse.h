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
#include "../ServerEnumerations.h"
#include "OrthancIdentifiers.h"

#include <boost/noncopyable.hpp>
#include <deque>
#include <map>
#include <set>


namespace Orthanc
{
  class FindResponse : public boost::noncopyable
  {
  public:
    class Item : public boost::noncopyable
    {
    private:
      ResourceType                         level_;
      OrthancIdentifiers                   identifiers_;
      std::map<MetadataType, std::string>  metadata_;
      std::unique_ptr<DicomMap>            dicomMap_;

    public:
      Item(ResourceType level,
           const OrthancIdentifiers& identifiers) :
        level_(level),
        identifiers_(identifiers)
      {
      }

      Item(ResourceType level,
           DicomMap* dicomMap /* takes ownership */);

      ResourceType GetLevel() const
      {
        return level_;
      }

      const OrthancIdentifiers& GetIdentifiers() const
      {
        return identifiers_;
      }

      void AddMetadata(MetadataType metadata,
                       const std::string& value);

      bool HasMetadata(MetadataType metadata) const
      {
        return metadata_.find(metadata) != metadata_.end();
      }

      bool LookupMetadata(std::string& value,
                          MetadataType metadata) const;

      void ListMetadata(std::set<MetadataType> metadata) const;

      bool HasDicomMap() const
      {
        return dicomMap_.get() != NULL;
      }

      const DicomMap& GetDicomMap() const;
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
