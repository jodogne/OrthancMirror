/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

#include "../ServerEnumerations.h"
#include "../../../OrthancFramework/Sources/DicomFormat/DicomMap.h"
#include "DatabaseConstraint.h"

#include <boost/shared_ptr.hpp>

namespace Orthanc
{
  class DicomTagConstraint : public boost::noncopyable
  {
  private:
    class NormalizedString;
    class RegularExpression;

    DicomTag                tag_;
    ConstraintType          constraintType_;
    std::set<std::string>   values_;
    bool                    caseSensitive_;
    bool                    mandatory_;

    mutable boost::shared_ptr<RegularExpression>  regex_;  // mutable because the regex is an internal object created only when required (in IsMatch const method)

    void AssignSingleValue(const std::string& value);

  public:
    DicomTagConstraint(const DicomTag& tag,
                       ConstraintType type,
                       const std::string& value,
                       bool caseSensitive,
                       bool mandatory);

    // For list search
    DicomTagConstraint(const DicomTag& tag,
                       ConstraintType type,
                       bool caseSensitive,
                       bool mandatory);

    explicit DicomTagConstraint(const DicomTagConstraint& other);
    
    explicit DicomTagConstraint(const DatabaseConstraint& constraint);

    const DicomTag& GetTag() const
    {
      return tag_;
    }

    void SetTag(const DicomTag& tag)
    {
      tag_ = tag;
    }

    ConstraintType GetConstraintType() const
    {
      return constraintType_;
    }
    
    bool IsCaseSensitive() const
    {
      return caseSensitive_;
    }

    void SetCaseSensitive(bool caseSensitive)
    {
      caseSensitive_ = caseSensitive;
    }

    bool IsMandatory() const
    {
      return mandatory_;
    }

    void AddValue(const std::string& value);

    const std::string& GetValue() const;

    const std::set<std::string>& GetValues() const
    {
      return values_;
    }

    bool IsMatch(const std::string& value) const;

    bool IsMatch(const DicomMap& value) const;

    std::string Format() const;

    DatabaseConstraint ConvertToDatabaseConstraint(ResourceType level,
                                                   DicomTagType tagType) const;
  };
}
