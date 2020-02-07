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


#include "../PrecompiledHeadersServer.h"
#include "DatabaseConstraint.h"

#include "../../Core/OrthancException.h"


namespace Orthanc
{
  namespace Plugins
  {
#if ORTHANC_ENABLE_PLUGINS == 1
    OrthancPluginResourceType Convert(ResourceType type)
    {
      switch (type)
      {
        case ResourceType_Patient:
          return OrthancPluginResourceType_Patient;

        case ResourceType_Study:
          return OrthancPluginResourceType_Study;

        case ResourceType_Series:
          return OrthancPluginResourceType_Series;

        case ResourceType_Instance:
          return OrthancPluginResourceType_Instance;

        default:
          throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
    }
#endif


#if ORTHANC_ENABLE_PLUGINS == 1
    ResourceType Convert(OrthancPluginResourceType type)
    {
      switch (type)
      {
        case OrthancPluginResourceType_Patient:
          return ResourceType_Patient;

        case OrthancPluginResourceType_Study:
          return ResourceType_Study;

        case OrthancPluginResourceType_Series:
          return ResourceType_Series;

        case OrthancPluginResourceType_Instance:
          return ResourceType_Instance;

        default:
          throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
    }
#endif


#if ORTHANC_PLUGINS_HAS_DATABASE_CONSTRAINT == 1
    OrthancPluginConstraintType Convert(ConstraintType constraint)
    {
      switch (constraint)
      {
        case ConstraintType_Equal:
          return OrthancPluginConstraintType_Equal;

        case ConstraintType_GreaterOrEqual:
          return OrthancPluginConstraintType_GreaterOrEqual;

        case ConstraintType_SmallerOrEqual:
          return OrthancPluginConstraintType_SmallerOrEqual;

        case ConstraintType_Wildcard:
          return OrthancPluginConstraintType_Wildcard;

        case ConstraintType_List:
          return OrthancPluginConstraintType_List;

        default:
          throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
    }
#endif    

    
#if ORTHANC_PLUGINS_HAS_DATABASE_CONSTRAINT == 1
    ConstraintType Convert(OrthancPluginConstraintType constraint)
    {
      switch (constraint)
      {
        case OrthancPluginConstraintType_Equal:
          return ConstraintType_Equal;

        case OrthancPluginConstraintType_GreaterOrEqual:
          return ConstraintType_GreaterOrEqual;

        case OrthancPluginConstraintType_SmallerOrEqual:
          return ConstraintType_SmallerOrEqual;

        case OrthancPluginConstraintType_Wildcard:
          return ConstraintType_Wildcard;

        case OrthancPluginConstraintType_List:
          return ConstraintType_List;

        default:
          throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
    }
#endif
  }

  DatabaseConstraint::DatabaseConstraint(ResourceType level,
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

    
#if ORTHANC_PLUGINS_HAS_DATABASE_CONSTRAINT == 1
  DatabaseConstraint::DatabaseConstraint(const OrthancPluginDatabaseConstraint& constraint) :
    level_(Plugins::Convert(constraint.level)),
    tag_(constraint.tagGroup, constraint.tagElement),
    isIdentifier_(constraint.isIdentifierTag),
    constraintType_(Plugins::Convert(constraint.type)),
    caseSensitive_(constraint.isCaseSensitive),
    mandatory_(constraint.isMandatory)
  {
    if (constraintType_ != ConstraintType_List &&
        constraint.valuesCount != 1)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    values_.resize(constraint.valuesCount);

    for (uint32_t i = 0; i < constraint.valuesCount; i++)
    {
      assert(constraint.values[i] != NULL);
      values_[i].assign(constraint.values[i]);
    }
  }
#endif
    

  const std::string& DatabaseConstraint::GetValue(size_t index) const
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


  const std::string& DatabaseConstraint::GetSingleValue() const
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


#if ORTHANC_PLUGINS_HAS_DATABASE_CONSTRAINT == 1
  void DatabaseConstraint::EncodeForPlugins(OrthancPluginDatabaseConstraint& constraint,
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
