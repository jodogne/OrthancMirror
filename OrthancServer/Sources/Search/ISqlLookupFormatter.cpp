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
#include "ISqlLookupFormatter.h"

#include "../../../OrthancFramework/Sources/OrthancException.h"
#include "../../../OrthancFramework/Sources/Toolbox.h"
#include "../Database/FindRequest.h"
#include "DatabaseDicomTagConstraint.h"
#include "../Database/MainDicomTagsRegistry.h"

#include <cassert>
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

  static std::string FormatLevel(const char* prefix, ResourceType level)
  {
    switch (level)
    {
      case ResourceType_Patient:
        return std::string(prefix) + "patients";
        
      case ResourceType_Study:
        return std::string(prefix) + "studies";
        
      case ResourceType_Series:
        return std::string(prefix) + "series";
        
      case ResourceType_Instance:
        return std::string(prefix) + "instances";

      default:
        throw OrthancException(ErrorCode_InternalError);
    }
  }      


  static bool FormatComparison(std::string& target,
                               ISqlLookupFormatter& formatter,
                               const IDatabaseConstraint& constraint,
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
                         const DatabaseDicomTagConstraint& constraint,
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

  static void FormatJoin(std::string& target,
                         const DatabaseMetadataConstraint& constraint,
                         ResourceType level,
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

    target += "Metadata ";

    target += tag + " ON " + tag + ".id = " + FormatLevel(level) +
               ".internalId AND " + tag + ".type = " +
               boost::lexical_cast<std::string>(constraint.GetMetadata());
  }


  static void FormatJoinForOrdering(std::string& target,
                                    const DicomTag& tag,
                                    size_t index,
                                    ResourceType requestLevel)
  {
    std::string orderArg = "order" + boost::lexical_cast<std::string>(index);

    target.clear();

    ResourceType tagLevel;
    DicomTagType tagType;
    MainDicomTagsRegistry registry;

    registry.LookupTag(tagLevel, tagType, tag);

    if (tagLevel == ResourceType_Patient && requestLevel == ResourceType_Study)
    { // Patient tags are copied at study level
      tagLevel = ResourceType_Study;
    }

    std::string tagTable;
    if (tagType == DicomTagType_Identifier)
    {
      tagTable = "DicomIdentifiers ";
    }
    else
    {
      tagTable = "MainDicomTags ";
    }

    std::string tagFilter = orderArg + ".tagGroup = " + boost::lexical_cast<std::string>(tag.GetGroup()) + " AND " + orderArg + ".tagElement = " + boost::lexical_cast<std::string>(tag.GetElement());

    if (tagLevel == requestLevel)
    {
      target = " LEFT JOIN " + tagTable + " " + orderArg + " ON " + orderArg + ".id = " + FormatLevel(requestLevel) +
                ".internalId AND " + tagFilter;
    }
    else if (static_cast<int32_t>(requestLevel) - static_cast<int32_t>(tagLevel) == 1)
    {
      target = " INNER JOIN Resources " + orderArg + "parent ON " + orderArg + "parent.internalId = " + FormatLevel(requestLevel) + ".parentId "
               " LEFT JOIN " + tagTable + " " + orderArg + " ON " + orderArg + ".id = " + orderArg + "parent.internalId AND " + tagFilter;
    }
    else if (static_cast<int32_t>(requestLevel) - static_cast<int32_t>(tagLevel) == 2)
    {
      target = " INNER JOIN Resources " + orderArg + "parent ON " + orderArg + "parent.internalId = " + FormatLevel(requestLevel) + ".parentId "
               " INNER JOIN Resources " + orderArg + "grandparent ON " + orderArg + "grandparent.internalId = " + orderArg + "parent.parentId "
               " LEFT JOIN " + tagTable + " " + orderArg + " ON " + orderArg + ".id = " + orderArg + "grandparent.internalId AND " + tagFilter;
    }
    else if (static_cast<int32_t>(requestLevel) - static_cast<int32_t>(tagLevel) == 3)
    {
      target = " INNER JOIN Resources " + orderArg + "parent ON " + orderArg + "parent.internalId = " + FormatLevel(requestLevel) + ".parentId "
               " INNER JOIN Resources " + orderArg + "grandparent ON " + orderArg + "grandparent.internalId = " + orderArg + "parent.parentId "
               " INNER JOIN Resources " + orderArg + "grandgrandparent ON " + orderArg + "grandgrandparent.internalId = " + orderArg + "grandparent.parentId "
               " LEFT JOIN " + tagTable + " " + orderArg + " ON " + orderArg + ".id = " + orderArg + "grandgrandparent.internalId AND " + tagFilter;
    }
  }

  static void FormatJoinForOrdering(std::string& target,
                                    const MetadataType& metadata,
                                    size_t index,
                                    ResourceType requestLevel)
  {
    std::string arg = "order" + boost::lexical_cast<std::string>(index);


    target = " INNER JOIN Metadata " + arg + " ON " + arg + ".id = " + FormatLevel(requestLevel) +
             ".internalId AND " + arg + ".type = " +
             boost::lexical_cast<std::string>(metadata);
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
                                const DatabaseDicomTagConstraint& constraint,
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


  void ISqlLookupFormatter::GetLookupLevels(ResourceType& lowerLevel,
                                            ResourceType& upperLevel,
                                            const ResourceType& queryLevel,
                                            const DatabaseDicomTagConstraints& lookup)
  {
    assert(ResourceType_Patient < ResourceType_Study &&
           ResourceType_Study < ResourceType_Series &&
           ResourceType_Series < ResourceType_Instance);
    
    lowerLevel = queryLevel;
    upperLevel = queryLevel;

    for (size_t i = 0; i < lookup.GetSize(); i++)
    {
      ResourceType level = lookup.GetConstraint(i).GetLevel();

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
                                  const DatabaseDicomTagConstraints& lookup,
                                  ResourceType queryLevel,
                                  const std::set<std::string>& labels,
                                  LabelsConstraint labelsConstraint,
                                  size_t limit)
  {
    // get the limit levels of the DICOM Tags lookup
    ResourceType lowerLevel, upperLevel;
    GetLookupLevels(lowerLevel, upperLevel, queryLevel, lookup);

    assert(upperLevel <= queryLevel &&
           queryLevel <= lowerLevel);

    const bool escapeBrackets = formatter.IsEscapeBrackets();
    
    std::string joins, comparisons;

    size_t count = 0;
    
    for (size_t i = 0; i < lookup.GetSize(); i++)
    {
      const DatabaseDicomTagConstraint& constraint = lookup.GetConstraint(i);

      std::string comparison;
      
      if (FormatComparison(comparison, formatter, constraint, count, escapeBrackets))
      {
        std::string join;
        FormatJoin(join, constraint, count);
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


  void ISqlLookupFormatter::Apply(std::string& sql,
                                  ISqlLookupFormatter& formatter,
                                  const FindRequest& request)
  {
    const bool escapeBrackets = formatter.IsEscapeBrackets();
    ResourceType queryLevel = request.GetLevel();
    const std::string& strQueryLevel = FormatLevel(queryLevel);

    ResourceType lowerLevel, upperLevel;
    GetLookupLevels(lowerLevel, upperLevel, queryLevel, request.GetDicomTagConstraints());

    assert(upperLevel <= queryLevel &&
           queryLevel <= lowerLevel);

    std::string ordering;
    std::string orderingJoins;

    if (request.GetOrdering().size() > 0)
    {
      int counter = 0;
      std::vector<std::string> orderByFields;
      for (std::deque<FindRequest::Ordering*>::const_iterator it = request.GetOrdering().begin(); it != request.GetOrdering().end(); ++it)
      {
        std::string orderingJoin;

        switch ((*it)->GetKeyType())
        {
          case FindRequest::KeyType_DicomTag:
            FormatJoinForOrdering(orderingJoin, (*it)->GetDicomTag(), counter, request.GetLevel());
            break;
          case FindRequest::KeyType_Metadata:
            FormatJoinForOrdering(orderingJoin, (*it)->GetMetadataType(), counter, request.GetLevel());
            break;
          default:
            throw OrthancException(ErrorCode_InternalError);
        }
        orderingJoins += orderingJoin;
        
        std::string orderByField;

#if ORTHANC_SQLITE_VERSION < 3030001
        // this is a way to push NULL values at the end before "NULLS LAST" was introduced:
        // first filter by 0/1 and then by the column value itself
        orderByField += "order" + boost::lexical_cast<std::string>(counter) + ".value IS NULL, ";
#endif
        switch ((*it)->GetCast())
        {
          case FindRequest::OrderingCast_Int:
            orderByField += "CAST(order" + boost::lexical_cast<std::string>(counter) + ".value AS INTEGER)";
            break;
          case FindRequest::OrderingCast_Float:
            orderByField += "CAST(order" + boost::lexical_cast<std::string>(counter) + ".value AS REAL)";
            break;
          default:
            orderByField += "order" + boost::lexical_cast<std::string>(counter) + ".value";
        }

        if ((*it)->GetDirection() == FindRequest::OrderingDirection_Ascending)
        {
          orderByField += " ASC";
        }
        else
        {
          orderByField += " DESC";
        }
        orderByFields.push_back(orderByField);
        ++counter;
      }

      std::string orderByFieldsString;
      Toolbox::JoinStrings(orderByFieldsString, orderByFields, ", ");

      ordering = "ROW_NUMBER() OVER (ORDER BY " + orderByFieldsString;
#if ORTHANC_SQLITE_VERSION >= 3030001
      ordering += " NULLS LAST";
#endif
      ordering += ") AS rowNumber";
    }
    else
    {
      ordering = "ROW_NUMBER() OVER (ORDER BY " + strQueryLevel + ".publicId) AS rowNumber";  // we need a default ordering in order to make default queries repeatable when using since&limit
    }

    sql = ("SELECT " +
           strQueryLevel + ".publicId, " +
           strQueryLevel + ".internalId, " +
           ordering + 
           " FROM Resources AS " + strQueryLevel);


    std::string joins, comparisons;

    // handle parent constraints
    if (request.GetOrthancIdentifiers().IsDefined() && request.GetOrthancIdentifiers().DetectLevel() <= queryLevel)
    {
      ResourceType topParentLevel = request.GetOrthancIdentifiers().DetectLevel();

      if (topParentLevel == queryLevel)
      {
        comparisons += " AND " + FormatLevel(topParentLevel) + ".publicId = " + formatter.GenerateParameter(request.GetOrthancIdentifiers().GetLevel(topParentLevel));
      }
      else
      {
        comparisons += " AND " + FormatLevel("parent", topParentLevel) + ".publicId = " + formatter.GenerateParameter(request.GetOrthancIdentifiers().GetLevel(topParentLevel));

        for (int level = queryLevel; level > topParentLevel; level--)
        {
          joins += " INNER JOIN Resources " +
                  FormatLevel("parent", static_cast<ResourceType>(level - 1)) + " ON " +
                  FormatLevel("parent", static_cast<ResourceType>(level - 1)) + ".internalId = ";
          if (level == queryLevel)
          {
            joins += FormatLevel(static_cast<ResourceType>(level)) + ".parentId";
          }
          else
          {
            joins += FormatLevel("parent", static_cast<ResourceType>(level)) + ".parentId";
          }
        }
      }
    }

    size_t count = 0;
    
    const DatabaseDicomTagConstraints& dicomTagsConstraints = request.GetDicomTagConstraints();
    for (size_t i = 0; i < dicomTagsConstraints.GetSize(); i++)
    {
      const DatabaseDicomTagConstraint& constraint = dicomTagsConstraints.GetConstraint(i);

      std::string comparison;
      
      if (FormatComparison(comparison, formatter, constraint, count, escapeBrackets))
      {
        std::string join;
        FormatJoin(join, constraint, count);

        if (constraint.GetLevel() <= queryLevel)
        {
          joins += join;
        }
        else if (constraint.GetLevel() == queryLevel + 1 && !comparison.empty())
        {
          // new in v 1.12.6, the constraints on child tags are actually looking for one child with this value
          comparison = " EXISTS (SELECT 1 FROM Resources AS " + FormatLevel(static_cast<ResourceType>(queryLevel + 1)) + 
                       join + 
                       " WHERE " + comparison + " AND " + 
                       FormatLevel(static_cast<ResourceType>(queryLevel + 1)) + ".parentId = " + FormatLevel(static_cast<ResourceType>(queryLevel)) + ".internalId) ";
        }

        if (!comparison.empty())
        {
          comparisons += " AND " + comparison;
        }

        count ++;
      }
    }

    for (std::deque<DatabaseMetadataConstraint*>::const_iterator it = request.GetMetadataConstraint().begin(); it != request.GetMetadataConstraint().end(); ++it)
    {
      std::string comparison;
      
      if (FormatComparison(comparison, formatter, *(*it), count, escapeBrackets))
      {
        std::string join;
        FormatJoin(join, *(*it), request.GetLevel(), count);
        joins += join;

        if (!comparison.empty())
        {
          comparisons += " AND " + comparison;
        }
        
        count ++;
      }
    }

    for (int level = queryLevel - 1; level >= upperLevel; level--)
    {
      sql += (" INNER JOIN Resources " +
              FormatLevel(static_cast<ResourceType>(level)) + " ON " +
              FormatLevel(static_cast<ResourceType>(level)) + ".internalId=" +
              FormatLevel(static_cast<ResourceType>(level + 1)) + ".parentId");
    }
      
    // disabled in v 1.12.6 now that the child levels are considered as "is there at least one child that meets this constraint"
    // for (int level = queryLevel + 1; level <= lowerLevel; level++)
    // {
    //   sql += (" INNER JOIN Resources " +
    //           FormatLevel(static_cast<ResourceType>(level)) + " ON " +
    //           FormatLevel(static_cast<ResourceType>(level - 1)) + ".internalId=" +
    //           FormatLevel(static_cast<ResourceType>(level)) + ".parentId");
    // }

    std::list<std::string> where;
    where.push_back(strQueryLevel + ".resourceType = " +
                    formatter.FormatResourceType(queryLevel) + comparisons);


    if (!request.GetLabels().empty())
    {
      /**
       * "In SQL Server, NOT EXISTS and NOT IN predicates are the best
       * way to search for missing values, as long as both columns in
       * question are NOT NULL."
       * https://explainextended.com/2009/09/15/not-in-vs-not-exists-vs-left-join-is-null-sql-server/
       **/

      const std::set<std::string>& labels = request.GetLabels();
      std::list<std::string> formattedLabels;
      for (std::set<std::string>::const_iterator it = labels.begin(); it != labels.end(); ++it)
      {
        formattedLabels.push_back(formatter.GenerateParameter(*it));
      }

      std::string condition;
      switch (request.GetLabelsConstraint())
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
      
      where.push_back("(SELECT COUNT(1) FROM Labels AS selectedLabels WHERE selectedLabels.id = " + strQueryLevel +
                      ".internalId AND selectedLabels.label IN (" + Join(formattedLabels, "", ", ") + ")) " + condition);
    }

    sql += joins + orderingJoins + Join(where, " WHERE ", " AND ");

    if (request.HasLimits())
    {
      sql += formatter.FormatLimits(request.GetLimitsSince(), request.GetLimitsCount());
    }
  }


  void ISqlLookupFormatter::ApplySingleLevel(std::string& sql,
                                             ISqlLookupFormatter& formatter,
                                             const DatabaseDicomTagConstraints& lookup,
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

    for (size_t i = 0; i < lookup.GetSize(); i++)
    {
      const DatabaseDicomTagConstraint& constraint = lookup.GetConstraint(i);

      std::string comparison;
      
      if (FormatComparison2(comparison, formatter, constraint, escapeBrackets))
      {
        if (!comparison.empty())
        {
          if (constraint.IsIdentifier())
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
