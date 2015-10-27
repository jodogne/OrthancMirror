/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2015 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
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

#include "IFindConstraint.h"
#include "SetOfResources.h"

#include <memory>
#include <boost/thread/mutex.hpp>

namespace Orthanc
{
  class LookupResource : public boost::noncopyable
  {
  private:
    typedef std::list<IFindConstraint*>  Constraints;
    
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

      bool Add(std::auto_ptr<IFindConstraint>& constraint);

      void Apply(SetOfResources& candidates,
                 IDatabaseWrapper& database) const;
    };

    typedef std::map<ResourceType, Level*>  Levels;

    ResourceType level_;
    Levels       levels_;
    Constraints  unoptimizedConstraints_;
    size_t       maxResults_;

    bool AddInternal(ResourceType level,
                     std::auto_ptr<IFindConstraint>& constraint);

    void ApplyLevel(SetOfResources& candidates,
                    ResourceType level,
                    IDatabaseWrapper& database) const;

    bool ApplyUnoptimizedConstraints(std::list<int64_t>& result,
                                     const std::list<int64_t>& candidates,
                                     boost::mutex& databaseMutex,
                                     IDatabaseWrapper& database,
                                     IStorageArea& storageArea) const;

  public:
    LookupResource(ResourceType level);

    ~LookupResource();

    void Add(IFindConstraint* constraint);   // Takes ownership

    void Add(const DicomTag& tag,
             const std::string& dicomQuery,
             bool caseSensitivePN);

    void SetMaxResults(size_t maxResults)
    {
      maxResults_ = maxResults;
    }

    size_t GetMaxResults() const
    {
      return maxResults_;
    }

    bool Apply(std::list<int64_t>& result,
               boost::mutex& databaseMutex,
               IDatabaseWrapper& database,
               IStorageArea& storageArea) const;

    bool Apply(std::list<std::string>& result,
               boost::mutex& databaseMutex,
               IDatabaseWrapper& database,
               IStorageArea& storageArea) const;
  };
}
