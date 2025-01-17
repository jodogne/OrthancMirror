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
#include "DatabaseMetadataConstraint.h"

#include "../../../OrthancFramework/Sources/OrthancException.h"

#include <boost/lexical_cast.hpp>
#include <cassert>


namespace Orthanc
{
  DatabaseMetadataConstraint::DatabaseMetadataConstraint(MetadataType metadata,
                                                         ConstraintType type,
                                                         const std::vector<std::string>& values,
                                                         bool caseSensitive) :
    metadata_(metadata),
    constraintType_(type),
    values_(values),
    caseSensitive_(caseSensitive)
  {
    if (type != ConstraintType_List &&
        values_.size() != 1)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }      


  DatabaseMetadataConstraint::DatabaseMetadataConstraint(MetadataType metadata,
                                                         ConstraintType type,
                                                         const std::string& value,
                                                         bool caseSensitive) :
    metadata_(metadata),
    constraintType_(type),
    caseSensitive_(caseSensitive)
  {
    if (type == ConstraintType_List)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    values_.push_back(value);
  }      

  const std::string& DatabaseMetadataConstraint::GetValue(size_t index) const
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


  const std::string& DatabaseMetadataConstraint::GetSingleValue() const
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

}
