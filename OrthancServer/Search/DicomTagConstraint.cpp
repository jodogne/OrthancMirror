/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2018 Osimis S.A., Belgium
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

#include "../../Core/OrthancException.h"
#include "../../Core/Toolbox.h"

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


  DicomTagConstraint::DicomTagConstraint(const DicomTag& tag,
                                         ConstraintType type,
                                         const std::string& value,
                                         bool caseSensitive) :
    hasTagInfo_(false),
    tagType_(DicomTagType_Generic),  // Dummy initialization
    level_(ResourceType_Patient),    // Dummy initialization
    tag_(tag),
    constraintType_(type),
    caseSensitive_(caseSensitive)
  {
    if (type == ConstraintType_Equal ||
        type == ConstraintType_SmallerOrEqual ||
        type == ConstraintType_GreaterOrEqual ||
        type == ConstraintType_Wildcard)
    {
      values_.insert(value);
    }
    else
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    if (type != ConstraintType_Wildcard &&
        (value.find('*') != std::string::npos ||
         value.find('?') != std::string::npos))
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }


  DicomTagConstraint::DicomTagConstraint(const DicomTag& tag,
                                         ConstraintType type,
                                         bool caseSensitive) :
    hasTagInfo_(false),
    tagType_(DicomTagType_Generic),  // Dummy initialization
    level_(ResourceType_Patient),    // Dummy initialization
    tag_(tag),
    constraintType_(type),
    caseSensitive_(caseSensitive)
  {
    if (type != ConstraintType_List)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }


  void DicomTagConstraint::SetTagInfo(DicomTagType tagType,
                                      ResourceType level)
  {
    hasTagInfo_ = true;
    tagType_ = tagType;
    level_ = level;
  }


  DicomTagType DicomTagConstraint::GetTagType() const
  {
    if (!hasTagInfo_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      return tagType_;
    }
  }


  const ResourceType DicomTagConstraint::GetLevel() const
  {
    if (!hasTagInfo_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      return level_;
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
        tmp->IsNull() ||
        tmp->IsBinary())
    {
      return false;
    }
    else
    {
      return IsMatch(tmp->GetContent());
    }
  }
}
