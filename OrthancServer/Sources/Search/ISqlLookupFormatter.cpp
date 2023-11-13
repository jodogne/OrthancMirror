/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2021-2023 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#if !defined(ORTHANC_BUILDING_SERVER_LIBRARY)
#  error Macro ORTHANC_BUILDING_SERVER_LIBRARY must be defined
#endif

#if ORTHANC_BUILDING_SERVER_LIBRARY == 1
#  include "../PrecompiledHeadersServer.h"
#endif

#include "ISqlLookupFormatter.h"

#if ORTHANC_BUILDING_SERVER_LIBRARY == 1
#  include "../../../OrthancFramework/Sources/OrthancException.h"
#  include "../../../OrthancFramework/Sources/Toolbox.h"
#else
#  include <OrthancException.h>
#  include <Toolbox.h>
#endif

#include "DatabaseConstraint.h"

#include <boost/lexical_cast.hpp>
#include <list>


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
                               size_t index,
                               bool escapeBrackets)
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
            else if (escapeBrackets && value[i] == '[')
            {
              escaped += "\\[";
            }
            else if (escapeBrackets && value[i] == ']')
            {
              escaped += "\\]";
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


  static std::string Join(const std::list<std::string>& values,
                          const std::string& prefix,
                          const std::string& separator)
  {
    if (values.empty())
    {
      return "";
    }
    else
    {
      std::string s = prefix;

      bool first = true;
      for (std::list<std::string>::const_iterator it = values.begin(); it != values.end(); ++it)
      {
        if (first)
        {
          first = false;
        }
        else
        {
          s += separator;
        }

        s += *it;
      }

      return s;
    }
  }

  static bool FormatComparison2(std::string& target,
                                ISqlLookupFormatter& formatter,
                                const DatabaseConstraint& constraint,
                                bool escapeBrackets)
  {
    std::string comparison;
    std::string tagFilter = ("tagGroup = " + boost::lexical_cast<std::string>(constraint.GetTag().GetGroup())
                              + " AND tagElement = " + boost::lexical_cast<std::string>(constraint.GetTag().GetElement()));

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
          comparison = " AND value " + op + " " + parameter;
        }
        else
        {
          comparison = " AND lower(value) " + op + " lower(" + parameter + ")";
        }

        break;
      }

      case ConstraintType_List:
      {
        std::vector<std::string> comparisonValues;
        for (size_t i = 0; i < constraint.GetValuesCount(); i++)
        {
          std::string parameter = formatter.GenerateParameter(constraint.GetValue(i));

          if (constraint.IsCaseSensitive())
          {
            comparisonValues.push_back(parameter);
          }
          else
          {
            comparisonValues.push_back("lower(" + parameter + ")");
          }
        }

        std::string values;
        Toolbox::JoinStrings(values, comparisonValues, ", ");

        if (constraint.IsCaseSensitive())
        {
          comparison = " AND value IN (" + values + ")";
        }
        else
        {
          comparison = " AND lower(value) IN (" + values + ")";
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
            else if (escapeBrackets && value[i] == '[')
            {
              escaped += "\\[";
            }
            else if (escapeBrackets && value[i] == ']')
            {
              escaped += "\\]";
            }
            else
            {
              escaped += value[i];
            }               
          }

          std::string parameter = formatter.GenerateParameter(escaped);

          if (constraint.IsCaseSensitive())
          {
            comparison = " AND value LIKE " + parameter + " " + formatter.FormatWildcardEscape();
          }
          else
          {
            comparison = " AND lower(value) LIKE lower(" + parameter + ") " + formatter.FormatWildcardEscape();
          }
        }
          
        break;
      }

      default:
        return false;
    }

    if (constraint.IsMandatory())
    {
      target = tagFilter + comparison;
    }
    else if (comparison.empty())
    {
      target = tagFilter + " AND value IS NULL";
    }
    else
    {
      target = tagFilter + " AND value IS NULL OR " + comparison;
    }

    return true;
  }


  void ISqlLookupFormatter::GetLookupLevels(ResourceType& lowerLevel, ResourceType& upperLevel, const ResourceType& queryLevel, const std::vector<DatabaseConstraint>& lookup)
  {
    assert(ResourceType_Patient < ResourceType_Study &&
           ResourceType_Study < ResourceType_Series &&
           ResourceType_Series < ResourceType_Instance);
    
    lowerLevel = queryLevel;
    upperLevel = queryLevel;

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
  }
  

  void ISqlLookupFormatter::Apply(std::string& sql,
                                  ISqlLookupFormatter& formatter,
                                  const std::vector<DatabaseConstraint>& lookup,
                                  ResourceType queryLevel,
                                  const std::set<std::string>& labels,
                                  LabelsConstraint labelsConstraint,
                                  size_t limit)
  {
    ResourceType lowerLevel, upperLevel;
    GetLookupLevels(lowerLevel, upperLevel, queryLevel, lookup);

    assert(upperLevel <= queryLevel &&
           queryLevel <= lowerLevel);

    const bool escapeBrackets = formatter.IsEscapeBrackets();
    
    std::string joins, comparisons;

    size_t count = 0;
    
    for (size_t i = 0; i < lookup.size(); i++)
    {
      std::string comparison;
      
      if (FormatComparison(comparison, formatter, lookup[i], count, escapeBrackets))
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

    std::list<std::string> where;
    where.push_back(FormatLevel(queryLevel) + ".resourceType = " +
                    formatter.FormatResourceType(queryLevel) + comparisons);

    if (!labels.empty())
    {
      /**
       * "In SQL Server, NOT EXISTS and NOT IN predicates are the best
       * way to search for missing values, as long as both columns in
       * question are NOT NULL."
       * https://explainextended.com/2009/09/15/not-in-vs-not-exists-vs-left-join-is-null-sql-server/
       **/

      std::list<std::string> formattedLabels;
      for (std::set<std::string>::const_iterator it = labels.begin(); it != labels.end(); ++it)
      {
        formattedLabels.push_back(formatter.GenerateParameter(*it));
      }

      std::string condition;
      switch (labelsConstraint)
      {
        case LabelsConstraint_Any:
          condition = "> 0";
          break;
          
        case LabelsConstraint_All:
          condition = "= " + boost::lexical_cast<std::string>(labels.size());
          break;
          
        case LabelsConstraint_None:
          condition = "= 0";
          break;
          
        default:
          throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
      
      where.push_back("(SELECT COUNT(1) FROM Labels AS selectedLabels WHERE selectedLabels.id = " + FormatLevel(queryLevel) +
                      ".internalId AND selectedLabels.label IN (" + Join(formattedLabels, "", ", ") + ")) " + condition);
    }

    sql += joins + Join(where, " WHERE ", " AND ");

    if (limit != 0)
    {
      sql += " LIMIT " + boost::lexical_cast<std::string>(limit);
    }
  }


  void ISqlLookupFormatter::ApplySingleLevel(std::string& sql,
                                             ISqlLookupFormatter& formatter,
                                             const std::vector<DatabaseConstraint>& lookup,
                                             ResourceType queryLevel,
                                             const std::set<std::string>& labels,
                                             LabelsConstraint labelsConstraint,
                                             size_t limit
                                             )
  {
    ResourceType lowerLevel, upperLevel;
    GetLookupLevels(lowerLevel, upperLevel, queryLevel, lookup);
    
    assert(upperLevel == queryLevel &&
           queryLevel == lowerLevel);

    const bool escapeBrackets = formatter.IsEscapeBrackets();
    
    std::vector<std::string> mainDicomTagsComparisons, dicomIdentifiersComparisons;

    for (size_t i = 0; i < lookup.size(); i++)
    {
      std::string comparison;
      
      if (FormatComparison2(comparison, formatter, lookup[i], escapeBrackets))
      {
        if (!comparison.empty())
        {
          if (lookup[i].IsIdentifier())
          {
            dicomIdentifiersComparisons.push_back(comparison);
          }
          else
          {
            mainDicomTagsComparisons.push_back(comparison);
          }
        }
      }
    }

    sql = ("SELECT publicId, internalId "
           "FROM Resources "
           "WHERE resourceType = " + formatter.FormatResourceType(queryLevel) 
            + " ");

    if (dicomIdentifiersComparisons.size() > 0)
    {
      for (std::vector<std::string>::const_iterator it = dicomIdentifiersComparisons.begin(); it < dicomIdentifiersComparisons.end(); ++it)
      {
        sql += (" AND internalId IN (SELECT id FROM DicomIdentifiers WHERE " + *it + ") ");
      }
    }

    if (mainDicomTagsComparisons.size() > 0)
    {
      std::string comparisons;
      for (std::vector<std::string>::const_iterator it = mainDicomTagsComparisons.begin(); it < mainDicomTagsComparisons.end(); ++it)
      {
        sql += (" AND internalId IN (SELECT id FROM MainDicomTags WHERE " + *it + ") ");
      }
    }

    if (!labels.empty())
    {
      /**
       * "In SQL Server, NOT EXISTS and NOT IN predicates are the best
       * way to search for missing values, as long as both columns in
       * question are NOT NULL."
       * https://explainextended.com/2009/09/15/not-in-vs-not-exists-vs-left-join-is-null-sql-server/
       **/

      std::list<std::string> formattedLabels;
      for (std::set<std::string>::const_iterator it = labels.begin(); it != labels.end(); ++it)
      {
        formattedLabels.push_back(formatter.GenerateParameter(*it));
      }

      std::string condition;
      std::string inOrNotIn;
      switch (labelsConstraint)
      {
        case LabelsConstraint_Any:
          condition = "> 0";
          inOrNotIn = "IN";
          break;
          
        case LabelsConstraint_All:
          condition = "= " + boost::lexical_cast<std::string>(labels.size());
          inOrNotIn = "IN";
          break;
          
        case LabelsConstraint_None:
          condition = "> 0";
          inOrNotIn = "NOT IN";
          break;
          
        default:
          throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
      
      sql += (" AND internalId " + inOrNotIn + " (SELECT id"
                                 " FROM (SELECT id, COUNT(1) AS labelsCount "
                                        "FROM Labels "
                                        "WHERE label IN (" + Join(formattedLabels, "", ", ") + ") GROUP BY id"
                                        ") AS temp "
                                 " WHERE labelsCount " + condition + ")");
    }

    if (limit != 0)
    {
      sql += " LIMIT " + boost::lexical_cast<std::string>(limit);
    }
  }

}
