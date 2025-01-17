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
#include "DatabaseDicomTagConstraints.h"

#include "../../../OrthancFramework/Sources/OrthancException.h"

#include <boost/lexical_cast.hpp>
#include <cassert>


namespace Orthanc
{
  void DatabaseDicomTagConstraints::Clear()
  {
    for (size_t i = 0; i < constraints_.size(); i++)
    {
      assert(constraints_[i] != NULL);
      delete constraints_[i];
    }

    constraints_.clear();
  }


  void DatabaseDicomTagConstraints::AddConstraint(DatabaseDicomTagConstraint* constraint)
  {
    if (constraint == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
    else
    {
      constraints_.push_back(constraint);
    }
  }


  const DatabaseDicomTagConstraint& DatabaseDicomTagConstraints::GetConstraint(size_t index) const
  {
    if (index >= constraints_.size())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
    else
    {
      assert(constraints_[index] != NULL);
      return *constraints_[index];
    }
  }


  std::string DatabaseDicomTagConstraints::Format() const
  {
    std::string s;

    for (size_t i = 0; i < constraints_.size(); i++)
    {
      assert(constraints_[i] != NULL);
      const DatabaseDicomTagConstraint& constraint = *constraints_[i];
      s += "Constraint " + boost::lexical_cast<std::string>(i) + " at " + EnumerationToString(constraint.GetLevel()) +
        ": " + constraint.GetTag().Format();

      switch (constraint.GetConstraintType())
      {
        case ConstraintType_Equal:
          s += " == " + constraint.GetSingleValue();
          break;

        case ConstraintType_SmallerOrEqual:
          s += " <= " + constraint.GetSingleValue();
          break;

        case ConstraintType_GreaterOrEqual:
          s += " >= " + constraint.GetSingleValue();
          break;

        case ConstraintType_Wildcard:
          s += " ~~ " + constraint.GetSingleValue();
          break;

        case ConstraintType_List:
        {
          s += " in [ ";
          bool first = true;
          for (size_t j = 0; j < constraint.GetValuesCount(); j++)
          {
            if (first)
            {
              first = false;
            }
            else
            {
              s += ", ";
            }
            s += constraint.GetValue(j);
          }
          s += "]";
          break;
        }

        default:
          throw OrthancException(ErrorCode_InternalError);
      }

      s += "\n";
    }

    return s;
  }
}
