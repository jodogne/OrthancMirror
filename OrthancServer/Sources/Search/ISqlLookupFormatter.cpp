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
#include "ISqlLookupFormatter.h"

#include "../../Core/OrthancException.h"
#include "DatabaseConstraint.h"

namespace Orthanc
{
  static std::string FormatLevel(ResourceType level)
  {
    switch (level)
    {
      case ResourceType_Patient:
        return "patients";
        
      case ResourceType_Study:
        return "studies";
        
      case ResourceType_Series:
        return "series";
        
      case ResourceType_Instance:
        return "instances";

      default:
        throw OrthancException(ErrorCode_InternalError);
    }
  }      
  

  static bool FormatComparison(std::string& target,
                               ISqlLookupFormatter& formatter,
                               const DatabaseConstraint& constraint,
                               size_t index)
  {
    std::string tag = "t" + boost::lexical_cast<std::string>(index);

    std::string comparison;
    
    switch (constraint.GetConstraintType())
    {
      case ConstraintType_Equal:
      case ConstraintType_SmallerOrEqual:
      case ConstraintType_GreaterOrEqual:
      {
        std::string op;
        switch (constraint.GetConstraintType())
        {
          case ConstraintType_Equal:
            op = "=";
            break;
          
          case ConstraintType_SmallerOrEqual:
            op = "<=";
            break;
          
          case ConstraintType_GreaterOrEqual:
            op = ">=";
            break;
          
          default:
            throw OrthancException(ErrorCode_InternalError);
        }

        std::string parameter = formatter.GenerateParameter(constraint.GetSingleValue());

        if (constraint.IsCaseSensitive())
        {
          comparison = tag + ".value " + op + " " + parameter;
        }
        else
        {
          comparison = "lower(" + tag + ".value) " + op + " lower(" + parameter + ")";
        }

        break;
      }

      case ConstraintType_List:
      {
        for (size_t i = 0; i < constraint.GetValuesCount(); i++)
        {
          if (!comparison.empty())
          {
            comparison += ", ";
          }
            
          std::string parameter = formatter.GenerateParameter(constraint.GetValue(i));

          if (constraint.IsCaseSensitive())
          {
            comparison += parameter;
          }
          else
          {
            comparison += "lower(" + parameter + ")";
          }
        }

        if (constraint.IsCaseSensitive())
        {
          comparison = tag + ".value IN (" + comparison + ")";
        }
        else
        {
          comparison = "lower(" +  tag + ".value) IN (" + comparison + ")";
        }
            
        break;
      }

      case ConstraintType_Wildcard:
      {
        const std::string value = constraint.GetSingleValue();

        if (value == "*")
        {
          if (!constraint.IsMandatory())
          {
            // Universal constraint on an optional tag, ignore it
            return false;
          }
        }
        else
        {
          std::string escaped;
          escaped.reserve(value.size());

          for (size_t i = 0; i < value.size(); i++)
          {
            if (value[i] == '*')
            {
              escaped += "%";
            }
            else if (value[i] == '?')
            {
              escaped += "_";
            }
            else if (value[i] == '%')
            {
              escaped += "\\%";
            }
            else if (value[i] == '_')
            {
              escaped += "\\_";
            }
            else if (value[i] == '\\')
            {
              escaped += "\\\\";
            }
            else
            {
              escaped += value[i];
            }               
          }

          std::string parameter = formatter.GenerateParameter(escaped);

          if (constraint.IsCaseSensitive())
          {
            comparison = (tag + ".value LIKE " + parameter + " " +
                          formatter.FormatWildcardEscape());
          }
          else
          {
            comparison = ("lower(" + tag + ".value) LIKE lower(" +
                          parameter + ") " + formatter.FormatWildcardEscape());
          }
        }
          
        break;
      }

      default:
        return false;
    }

    if (constraint.IsMandatory())
    {
      target = comparison;
    }
    else if (comparison.empty())
    {
      target = tag + ".value IS NULL";
    }
    else
    {
      target = tag + ".value IS NULL OR " + comparison;
    }

    return true;
  }


  static void FormatJoin(std::string& target,
                         const DatabaseConstraint& constraint,
                         size_t index)
  {
    std::string tag = "t" + boost::lexical_cast<std::string>(index);

    if (constraint.IsMandatory())
    {
      target = " INNER JOIN ";
    }
    else
    {
      target = " LEFT JOIN ";
    }

    if (constraint.IsIdentifier())
    {
      target += "DicomIdentifiers ";
    }
    else
    {
      target += "MainDicomTags ";
    }

    target += (tag + " ON " + tag + ".id = " + FormatLevel(constraint.GetLevel()) +
               ".internalId AND " + tag + ".tagGroup = " +
               boost::lexical_cast<std::string>(constraint.GetTag().GetGroup()) +
               " AND " + tag + ".tagElement = " +
               boost::lexical_cast<std::string>(constraint.GetTag().GetElement()));
  }
  

  void ISqlLookupFormatter::Apply(std::string& sql,
                                  ISqlLookupFormatter& formatter,
                                  const std::vector<DatabaseConstraint>& lookup,
                                  ResourceType queryLevel,
                                  size_t limit)
  {
    assert(ResourceType_Patient < ResourceType_Study &&
           ResourceType_Study < ResourceType_Series &&
           ResourceType_Series < ResourceType_Instance);
    
    ResourceType upperLevel = queryLevel;
    ResourceType lowerLevel = queryLevel;

    for (size_t i = 0; i < lookup.size(); i++)
    {
      ResourceType level = lookup[i].GetLevel();

      if (level < upperLevel)
      {
        upperLevel = level;
      }

      if (level > lowerLevel)
      {
        lowerLevel = level;
      }
    }
    
    assert(upperLevel <= queryLevel &&
           queryLevel <= lowerLevel);

    std::string joins, comparisons;

    size_t count = 0;
    
    for (size_t i = 0; i < lookup.size(); i++)
    {
      std::string comparison;
      
      if (FormatComparison(comparison, formatter, lookup[i], count))
      {
        std::string join;
        FormatJoin(join, lookup[i], count);
        joins += join;

        if (!comparison.empty())
        {
          comparisons += " AND " + comparison;
        }
        
        count ++;
      }
    }

    sql = ("SELECT " +
           FormatLevel(queryLevel) + ".publicId, " +
           FormatLevel(queryLevel) + ".internalId" +
           " FROM Resources AS " + FormatLevel(queryLevel));

    for (int level = queryLevel - 1; level >= upperLevel; level--)
    {
      sql += (" INNER JOIN Resources " +
              FormatLevel(static_cast<ResourceType>(level)) + " ON " +
              FormatLevel(static_cast<ResourceType>(level)) + ".internalId=" +
              FormatLevel(static_cast<ResourceType>(level + 1)) + ".parentId");
    }
      
    for (int level = queryLevel + 1; level <= lowerLevel; level++)
    {
      sql += (" INNER JOIN Resources " +
              FormatLevel(static_cast<ResourceType>(level)) + " ON " +
              FormatLevel(static_cast<ResourceType>(level - 1)) + ".internalId=" +
              FormatLevel(static_cast<ResourceType>(level)) + ".parentId");
    }
      
    sql += (joins + " WHERE " + FormatLevel(queryLevel) + ".resourceType = " +
            formatter.FormatResourceType(queryLevel) + comparisons);

    if (limit != 0)
    {
      sql += " LIMIT " + boost::lexical_cast<std::string>(limit);
    }
  }
}
