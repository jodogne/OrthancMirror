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

#if !defined(ORTHANC_BUILDING_SERVER_LIBRARY)
#  error Macro ORTHANC_BUILDING_SERVER_LIBRARY must be defined
#endif

#if ORTHANC_BUILDING_SERVER_LIBRARY == 1
#  include "../../../OrthancFramework/Sources/DicomFormat/DicomMap.h"
#else
// This is for the "orthanc-databases" project to reuse this file
#  include <DicomFormat/DicomMap.h>
#endif

#define ORTHANC_PLUGINS_HAS_DATABASE_CONSTRAINT 0

#if ORTHANC_ENABLE_PLUGINS == 1
#  include <orthanc/OrthancCDatabasePlugin.h>
#  if defined(ORTHANC_PLUGINS_VERSION_IS_ABOVE)      // Macro introduced in 1.3.1
#    if ORTHANC_PLUGINS_VERSION_IS_ABOVE(1, 5, 2)
#      undef  ORTHANC_PLUGINS_HAS_DATABASE_CONSTRAINT
#      define ORTHANC_PLUGINS_HAS_DATABASE_CONSTRAINT 1
#    endif
#  endif
#endif

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

  namespace Plugins
  {
#if ORTHANC_ENABLE_PLUGINS == 1
    OrthancPluginResourceType Convert(ResourceType type);
#endif

#if ORTHANC_ENABLE_PLUGINS == 1
    ResourceType Convert(OrthancPluginResourceType type);
#endif

#if ORTHANC_PLUGINS_HAS_DATABASE_CONSTRAINT == 1
    OrthancPluginConstraintType Convert(ConstraintType constraint);
#endif

#if ORTHANC_PLUGINS_HAS_DATABASE_CONSTRAINT == 1
    ConstraintType Convert(OrthancPluginConstraintType constraint);
#endif
  }


  // This class is also used by the "orthanc-databases" project
  class DatabaseConstraint
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

#if ORTHANC_PLUGINS_HAS_DATABASE_CONSTRAINT == 1
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

#if ORTHANC_PLUGINS_HAS_DATABASE_CONSTRAINT == 1
    void EncodeForPlugins(OrthancPluginDatabaseConstraint& constraint,
                          std::vector<const char*>& tmpValues) const;
#endif    
  };
}
