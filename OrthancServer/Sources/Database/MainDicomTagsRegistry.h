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


#pragma once

#include "../Search/DatabaseLookup.h"
#include "../Search/DatabaseDicomTagConstraints.h"

#include <boost/noncopyable.hpp>


namespace Orthanc
{
  class MainDicomTagsRegistry : public boost::noncopyable
  {
  private:
    class TagInfo
    {
    private:
      ResourceType  level_;
      DicomTagType  type_;

    public:
      TagInfo()
      {
      }

      TagInfo(ResourceType level,
              DicomTagType type) :
        level_(level),
        type_(type)
      {
      }

      ResourceType GetLevel() const
      {
        return level_;
      }

      DicomTagType GetType() const
      {
        return type_;
      }
    };

    typedef std::map<DicomTag, TagInfo>   Registry;

    Registry  registry_;

    void LoadTags(ResourceType level);

  public:
    MainDicomTagsRegistry();

    void LookupTag(ResourceType& level,
                   DicomTagType& type,
                   const DicomTag& tag) const;

    /**
     * Returns "true" iff. the normalized lookup is the same as the
     * original DatabaseLookup. If "false" is returned, the target
     * constraints are less strict than the original DatabaseLookup,
     * so more resources will match them.
     **/
    bool NormalizeLookup(bool& canBeFullyPerformedInDb,
                         DatabaseDicomTagConstraints& target,
                         const DatabaseLookup& source,
                         ResourceType queryLevel,
                         bool allowChildrenExistsQueries) const;
  };
}
