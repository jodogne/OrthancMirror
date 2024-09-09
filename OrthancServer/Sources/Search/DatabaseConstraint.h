/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2024 Orthanc Team SRL, Belgium
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

#if ORTHANC_ENABLE_PLUGINS == 1
#  include "../../Plugins/Include/orthanc/OrthancCDatabasePlugin.h"
#endif

#include "../../../OrthancFramework/Sources/DicomFormat/DicomMap.h"

#include <deque>

namespace Orthanc
{
  enum ConstraintType
  {
    ConstraintType_Equal,
    ConstraintType_SmallerOrEqual,
    ConstraintType_GreaterOrEqual,
    ConstraintType_Wildcard,
    ConstraintType_List
  };


#if ORTHANC_ENABLE_PLUGINS == 1
  namespace Plugins
  {
    OrthancPluginResourceType Convert(ResourceType type);

    ResourceType Convert(OrthancPluginResourceType type);

    OrthancPluginConstraintType Convert(ConstraintType constraint);

    ConstraintType Convert(OrthancPluginConstraintType constraint);
  }
#endif


  class DatabaseConstraint : public boost::noncopyable
  {
  private:
    ResourceType              level_;
    DicomTag                  tag_;
    bool                      isIdentifier_;
    ConstraintType            constraintType_;
    std::vector<std::string>  values_;
    bool                      caseSensitive_;
    bool                      mandatory_;

  public:
    DatabaseConstraint(ResourceType level,
                       const DicomTag& tag,
                       bool isIdentifier,
                       ConstraintType type,
                       const std::vector<std::string>& values,
                       bool caseSensitive,
                       bool mandatory);

#if ORTHANC_ENABLE_PLUGINS == 1
    explicit DatabaseConstraint(const OrthancPluginDatabaseConstraint& constraint);
#endif
    
    ResourceType GetLevel() const
    {
      return level_;
    }

    const DicomTag& GetTag() const
    {
      return tag_;
    }

    bool IsIdentifier() const
    {
      return isIdentifier_;
    }

    ConstraintType GetConstraintType() const
    {
      return constraintType_;
    }

    size_t GetValuesCount() const
    {
      return values_.size();
    }

    const std::string& GetValue(size_t index) const;

    const std::string& GetSingleValue() const;

    bool IsCaseSensitive() const
    {
      return caseSensitive_;
    }

    bool IsMandatory() const
    {
      return mandatory_;
    }

    bool IsMatch(const DicomMap& dicom) const;

#if ORTHANC_ENABLE_PLUGINS == 1
    void EncodeForPlugins(OrthancPluginDatabaseConstraint& constraint,
                          std::vector<const char*>& tmpValues) const;
#endif    
  };


  class DatabaseConstraints : public boost::noncopyable
  {
  private:
    std::deque<DatabaseConstraint*>  constraints_;

  public:
    ~DatabaseConstraints()
    {
      Clear();
    }

    void Clear();

    void AddConstraint(DatabaseConstraint* constraint);  // Takes ownership

    bool IsEmpty() const
    {
      return constraints_.empty();
    }

    size_t GetSize() const
    {
      return constraints_.size();
    }

    const DatabaseConstraint& GetConstraint(size_t index) const;

    std::string Format() const;
  };
}
