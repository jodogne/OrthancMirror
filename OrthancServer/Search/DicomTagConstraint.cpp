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
#include "DicomTagConstraint.h"

#if defined(ORTHANC_ENABLE_LUA) && ORTHANC_ENABLE_LUA != 0
#  include "../ServerToolbox.h"
#endif

#include "../../Core/OrthancException.h"
#include "../../Core/Toolbox.h"
#include "DatabaseConstraint.h"

#include <boost/regex.hpp>

namespace Orthanc
{
  class DicomTagConstraint::NormalizedString : public boost::noncopyable
  {
  private:
    const std::string&  source_;
    bool                caseSensitive_;
    std::string         upper_;

  public:
    NormalizedString(const std::string& source,
                     bool caseSensitive) :
      source_(source),
      caseSensitive_(caseSensitive)
    {
      if (!caseSensitive_)
      {
        upper_ = Toolbox::ToUpperCaseWithAccents(source);
      }
    }

    const std::string& GetValue() const
    {
      if (caseSensitive_)
      {
        return source_;
      }
      else
      {
        return upper_;
      }
    }
  };


  class DicomTagConstraint::RegularExpression : public boost::noncopyable
  {
  private:
    boost::regex  regex_;

  public:
    RegularExpression(const std::string& source,
                      bool caseSensitive)
    {
      NormalizedString normalized(source, caseSensitive);
      regex_ = boost::regex(Toolbox::WildcardToRegularExpression(normalized.GetValue()));
    }

    const boost::regex& GetValue() const
    {
      return regex_;
    }
  };


  void DicomTagConstraint::AssignSingleValue(const std::string& value)
  {
    if (constraintType_ != ConstraintType_Wildcard &&
        (value.find('*') != std::string::npos ||
         value.find('?') != std::string::npos))
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    if (constraintType_ == ConstraintType_Equal ||
        constraintType_ == ConstraintType_SmallerOrEqual ||
        constraintType_ == ConstraintType_GreaterOrEqual ||
        constraintType_ == ConstraintType_Wildcard)
    {
      values_.clear();
      values_.insert(value);
    }
    else
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }


  DicomTagConstraint::DicomTagConstraint(const DicomTag& tag,
                                         ConstraintType type,
                                         const std::string& value,
                                         bool caseSensitive,
                                         bool mandatory) :
    tag_(tag),
    constraintType_(type),
    caseSensitive_(caseSensitive),
    mandatory_(mandatory)
  {
    AssignSingleValue(value);
  }


  DicomTagConstraint::DicomTagConstraint(const DicomTag& tag,
                                         ConstraintType type,
                                         bool caseSensitive,
                                         bool mandatory) :
    tag_(tag),
    constraintType_(type),
    caseSensitive_(caseSensitive),
    mandatory_(mandatory)
  {
    if (type != ConstraintType_List)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }


  DicomTagConstraint::DicomTagConstraint(const DatabaseConstraint& constraint) :
    tag_(constraint.GetTag()),
    constraintType_(constraint.GetConstraintType()),
    caseSensitive_(constraint.IsCaseSensitive()),
    mandatory_(constraint.IsMandatory())
  {
#if defined(ORTHANC_ENABLE_LUA) && ORTHANC_ENABLE_LUA != 0
    assert(constraint.IsIdentifier() ==
           ServerToolbox::IsIdentifier(constraint.GetTag(), constraint.GetLevel()));
#endif
    
    if (constraint.IsIdentifier())
    {
      // This conversion is only available for main DICOM tags, not for identifers
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    
    if (constraintType_ == ConstraintType_List)
    {
      for (size_t i = 0; i < constraint.GetValuesCount(); i++)
      {
        AddValue(constraint.GetValue(i));
      }
    }
    else
    {
      AssignSingleValue(constraint.GetSingleValue());
    }
  }


  void DicomTagConstraint::AddValue(const std::string& value)
  {
    if (constraintType_ != ConstraintType_List)
    {
      throw OrthancException(ErrorCode_BadParameterType);
    }
    else
    {
      values_.insert(value);
    }
  }


  const std::string& DicomTagConstraint::GetValue() const
  {
    if (constraintType_ == ConstraintType_List)
    {
      throw OrthancException(ErrorCode_BadParameterType);
    }
    else if (values_.size() != 1)
    {
      throw OrthancException(ErrorCode_InternalError);
    }
    else
    {
      return *values_.begin();
    }
  }


  bool DicomTagConstraint::IsMatch(const std::string& value)
  {
    NormalizedString source(value, caseSensitive_);

    switch (constraintType_)
    {
      case ConstraintType_Equal:
      {
        NormalizedString reference(GetValue(), caseSensitive_);
        return source.GetValue() == reference.GetValue();
      }

      case ConstraintType_SmallerOrEqual:
      {
        NormalizedString reference(GetValue(), caseSensitive_);
        return source.GetValue() <= reference.GetValue();
      }

      case ConstraintType_GreaterOrEqual:
      {
        NormalizedString reference(GetValue(), caseSensitive_);
        return source.GetValue() >= reference.GetValue();
      }

      case ConstraintType_Wildcard:
      {
        if (regex_.get() == NULL)
        {
          regex_.reset(new RegularExpression(GetValue(), caseSensitive_));
        }

        return boost::regex_match(source.GetValue(), regex_->GetValue());
      }

      case ConstraintType_List:
      {
        for (std::set<std::string>::const_iterator
               it = values_.begin(); it != values_.end(); ++it)
        {
          NormalizedString reference(*it, caseSensitive_);
          if (source.GetValue() == reference.GetValue())
          {
            return true;
          }
        }

        return false;
      }

      default:
        throw OrthancException(ErrorCode_InternalError);
    }
  }


  bool DicomTagConstraint::IsMatch(const DicomMap& value)
  {
    const DicomValue* tmp = value.TestAndGetValue(tag_);

    if (tmp == NULL ||
        tmp->IsNull())
    {
      if (mandatory_)
      {
        return false;
      }
      else
      {
        return true;
      }
    }
    else if (tmp->IsBinary())
    {
      return false;
    }
    else
    {
      return IsMatch(tmp->GetContent());
    }
  }


  std::string DicomTagConstraint::Format() const
  {
    switch (constraintType_)
    {
      case ConstraintType_Equal:
        return tag_.Format() + " == " + GetValue();

      case ConstraintType_SmallerOrEqual:
        return tag_.Format() + " <= " + GetValue();

      case ConstraintType_GreaterOrEqual:
        return tag_.Format() + " >= " + GetValue();

      case ConstraintType_Wildcard:
        return tag_.Format() + " ~~ " + GetValue();

      case ConstraintType_List:
      {
        std::string s = tag_.Format() + " IN [ ";

        bool first = true;
        for (std::set<std::string>::const_iterator
               it = values_.begin(); it != values_.end(); ++it)
        {
          if (first)
          {
            first = false;
          }
          else
          {
            s += ", ";
          }

          s += *it;
        }

        return s + "]";
      }

      default:
        throw OrthancException(ErrorCode_InternalError);
    }
  }


  DatabaseConstraint DicomTagConstraint::ConvertToDatabaseConstraint(ResourceType level,
                                                                     DicomTagType tagType) const
  {
    bool isIdentifier, caseSensitive;
    
    switch (tagType)
    {
      case DicomTagType_Identifier:
        isIdentifier = true;
        caseSensitive = true;
        break;

      case DicomTagType_Main:
        isIdentifier = false;
        caseSensitive = IsCaseSensitive();
        break;

      default:
        throw OrthancException(ErrorCode_InternalError);
    }

    std::vector<std::string> values;
    values.reserve(values_.size());
      
    for (std::set<std::string>::const_iterator
           it = values_.begin(); it != values_.end(); ++it)
    {
      if (isIdentifier)
      {
        values.push_back(ServerToolbox::NormalizeIdentifier(*it));
      }
      else
      {
        values.push_back(*it);
      }
    }

    return DatabaseConstraint(level, tag_, isIdentifier, constraintType_,
                              values, caseSensitive, mandatory_);
  }  
}
