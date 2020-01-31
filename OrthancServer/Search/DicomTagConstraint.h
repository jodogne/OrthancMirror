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


#pragma once

#include "../ServerEnumerations.h"
#include "../../Core/DicomFormat/DicomMap.h"
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

    boost::shared_ptr<RegularExpression>  regex_;

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

    DicomTagConstraint(const DatabaseConstraint& constraint);

    const DicomTag& GetTag() const
    {
      return tag_;
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

    bool IsMatch(const std::string& value);

    bool IsMatch(const DicomMap& value);

    std::string Format() const;

    DatabaseConstraint ConvertToDatabaseConstraint(ResourceType level,
                                                   DicomTagType tagType) const;
  };
}
