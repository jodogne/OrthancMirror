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

#include "ResourceFinder.h"

namespace Orthanc
{
  class DicomFindQuery : public ResourceFinder::IQuery
  {
  private:
    class  IConstraint : public boost::noncopyable
    {
    public:
      virtual ~IConstraint()
      {
      }

      virtual bool IsExactConstraint() const
      {
        return false;
      }

      virtual bool Apply(const std::string& value) const = 0;
    };


    class ValueConstraint;
    class RangeConstraint;
    class ListConstraint;
    class WildcardConstraint;

    typedef std::map<DicomTag, IConstraint*>  Constraints;
    typedef std::map<DicomTag, ResourceType>  MainDicomTags;

    MainDicomTags           mainDicomTags_;
    ResourceType            level_;
    bool                    filterJson_;
    Constraints             constraints_;
    std::set<ResourceType>  filteredLevels_;

    void AssignConstraint(const DicomTag& tag,
                          IConstraint* constraint);

    void PrepareMainDicomTags(ResourceType level);


  public:
    DicomFindQuery();

    virtual ~DicomFindQuery();

    void SetLevel(ResourceType level)
    {
      level_ = level;
    }

    virtual ResourceType GetLevel() const
    {
      return level_;
    }

    void SetConstraint(const DicomTag& tag,
                       const std::string& constraint,
                       bool caseSensitivePN);

    virtual bool RestrictIdentifier(std::string& value,
                                    DicomTag identifier) const;

    virtual bool HasMainDicomTagsFilter(ResourceType level) const;

    virtual bool FilterMainDicomTags(const std::string& resourceId,
                                     ResourceType level,
                                     const DicomMap& mainTags) const;

    virtual bool HasInstanceFilter() const;

    virtual bool FilterInstance(const std::string& instanceId,
                                const Json::Value& content) const;
  };
}
