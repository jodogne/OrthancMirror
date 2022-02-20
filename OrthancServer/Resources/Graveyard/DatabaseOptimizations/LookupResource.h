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

#include "ListConstraint.h"
#include "SetOfResources.h"

#include <memory>

namespace Orthanc
{
  class LookupResource : public boost::noncopyable
  {
  private:
    typedef std::map<DicomTag, IFindConstraint*>  Constraints;
    
    class Level
    {
    private:
      ResourceType        level_;
      std::set<DicomTag>  identifiers_;
      std::set<DicomTag>  mainTags_;
      Constraints         identifiersConstraints_;
      Constraints         mainTagsConstraints_;

    public:
      Level(ResourceType level);

      ~Level();

      bool Add(const DicomTag& tag,
               std::auto_ptr<IFindConstraint>& constraint);

      void Apply(SetOfResources& candidates,
                 IDatabaseWrapper& database) const;

      bool IsMatch(const DicomMap& dicom) const;
    };

    typedef std::map<ResourceType, Level*>  Levels;

    ResourceType                    level_;
    Levels                          levels_;
    Constraints                     unoptimizedConstraints_;   // Constraints on non-main DICOM tags
    std::auto_ptr<ListConstraint>   modalitiesInStudy_;

    bool AddInternal(ResourceType level,
                     const DicomTag& tag,
                     std::auto_ptr<IFindConstraint>& constraint);

    void ApplyLevel(SetOfResources& candidates,
                    ResourceType level,
                    IDatabaseWrapper& database) const;

  public:
    LookupResource(ResourceType level);

    ~LookupResource();

    ResourceType GetLevel() const
    {
      return level_;
    }

    void SetModalitiesInStudy(const std::string& modalities); 

    void Add(const DicomTag& tag,
             IFindConstraint* constraint);   // Takes ownership

    void AddDicomConstraint(const DicomTag& tag,
                            const std::string& dicomQuery,
                            bool caseSensitive);

    void FindCandidates(std::list<int64_t>& result,
                        IDatabaseWrapper& database) const;

    bool HasOnlyMainDicomTags() const
    {
      return unoptimizedConstraints_.empty();
    }

    bool IsMatch(const DicomMap& dicom) const;
  };
}
