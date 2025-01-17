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


#include "../PrecompiledHeadersServer.h"
#include "DatabaseDicomTagConstraint.h"

#include "../../../OrthancFramework/Sources/OrthancException.h"

#if ORTHANC_ENABLE_PLUGINS == 1
#  include "../../Plugins/Engine/PluginsEnumerations.h"
#endif

#include <boost/lexical_cast.hpp>
#include <cassert>


namespace Orthanc
{
  DatabaseDicomTagConstraint::DatabaseDicomTagConstraint(ResourceType level,
                                                         const DicomTag& tag,
                                                         bool isIdentifier,
                                                         ConstraintType type,
                                                         const std::vector<std::string>& values,
                                                         bool caseSensitive,
                                                         bool mandatory) :
    level_(level),
    tag_(tag),
    isIdentifier_(isIdentifier),
    constraintType_(type),
    values_(values),
    caseSensitive_(caseSensitive),
    mandatory_(mandatory)
  {
    if (type != ConstraintType_List &&
        values_.size() != 1)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }      

    
  const std::string& DatabaseDicomTagConstraint::GetValue(size_t index) const
  {
    if (index >= values_.size())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
    else
    {
      return values_[index];
    }
  }


  const std::string& DatabaseDicomTagConstraint::GetSingleValue() const
  {
    if (values_.size() != 1)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      return values_[0];
    }
  }


#if ORTHANC_ENABLE_PLUGINS == 1
  void DatabaseDicomTagConstraint::EncodeForPlugins(OrthancPluginDatabaseConstraint& constraint,
                                                    std::vector<const char*>& tmpValues) const
  {
    memset(&constraint, 0, sizeof(constraint));
    
    tmpValues.resize(values_.size());

    for (size_t i = 0; i < values_.size(); i++)
    {
      tmpValues[i] = values_[i].c_str();
    }

    constraint.level = Plugins::Convert(level_);
    constraint.tagGroup = tag_.GetGroup();
    constraint.tagElement = tag_.GetElement();
    constraint.isIdentifierTag = isIdentifier_;
    constraint.isCaseSensitive = caseSensitive_;
    constraint.isMandatory = mandatory_;
    constraint.type = Plugins::Convert(constraintType_);
    constraint.valuesCount = values_.size();
    constraint.values = (tmpValues.empty() ? NULL : &tmpValues[0]);
  }
#endif    
}
