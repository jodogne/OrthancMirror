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

#include "../../../OrthancFramework/Sources/DicomFormat/DicomMap.h"
#include "../ServerEnumerations.h"
#include "IDatabaseConstraint.h"

#if ORTHANC_ENABLE_PLUGINS == 1
#  include "../../Plugins/Include/orthanc/OrthancCDatabasePlugin.h"
#endif

namespace Orthanc
{
  class DatabaseDicomTagConstraint : public IDatabaseConstraint
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
    DatabaseDicomTagConstraint(ResourceType level,
                               const DicomTag& tag,
                               bool isIdentifier,
                               ConstraintType type,
                               const std::vector<std::string>& values,
                               bool caseSensitive,
                               bool mandatory);
    
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

    virtual ConstraintType GetConstraintType() const ORTHANC_OVERRIDE
    {
      return constraintType_;
    }

    virtual size_t GetValuesCount() const ORTHANC_OVERRIDE
    {
      return values_.size();
    }

    virtual const std::string& GetValue(size_t index) const ORTHANC_OVERRIDE;

    virtual const std::string& GetSingleValue() const ORTHANC_OVERRIDE;

    virtual bool IsCaseSensitive() const ORTHANC_OVERRIDE
    {
      return caseSensitive_;
    }

    virtual bool IsMandatory() const ORTHANC_OVERRIDE
    {
      return mandatory_;
    }


#if ORTHANC_ENABLE_PLUGINS == 1
    void EncodeForPlugins(OrthancPluginDatabaseConstraint& constraint,
                          std::vector<const char*>& tmpValues) const;
#endif    
  };
}
