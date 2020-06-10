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
    DatabaseConstraint(const OrthancPluginDatabaseConstraint& constraint);
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
