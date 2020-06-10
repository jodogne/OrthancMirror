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
#include "LookupIdentifierQuery.h"

#include "../../Core/DicomParsing/FromDcmtkBridge.h"
#include "../../Core/OrthancException.h"
#include "../ServerToolbox.h"
#include "SetOfResources.h"

#include <cassert>



namespace Orthanc
{
  LookupIdentifierQuery::SingleConstraint::
  SingleConstraint(const DicomTag& tag,
                   IdentifierConstraintType type,
                   const std::string& value) : 
    tag_(tag),
    type_(type),
    value_(ServerToolbox::NormalizeIdentifier(value))
  {
  }


  LookupIdentifierQuery::RangeConstraint::
  RangeConstraint(const DicomTag& tag,
                  const std::string& start,
                  const std::string& end) : 
    tag_(tag),
    start_(ServerToolbox::NormalizeIdentifier(start)),
    end_(ServerToolbox::NormalizeIdentifier(end))
  {
  }


  LookupIdentifierQuery::Disjunction::~Disjunction()
  {
    for (size_t i = 0; i < singleConstraints_.size(); i++)
    {
      delete singleConstraints_[i];
    }

    for (size_t i = 0; i < rangeConstraints_.size(); i++)
    {
      delete rangeConstraints_[i];
    }
  }


  void LookupIdentifierQuery::Disjunction::Add(const DicomTag& tag,
                                               IdentifierConstraintType type,
                                               const std::string& value)
  {
    singleConstraints_.push_back(new SingleConstraint(tag, type, value));
  }


  void LookupIdentifierQuery::Disjunction::AddRange(const DicomTag& tag,
                                                    const std::string& start,
                                                    const std::string& end)
  {
    rangeConstraints_.push_back(new RangeConstraint(tag, start, end));
  }


  LookupIdentifierQuery::~LookupIdentifierQuery()
  {
    for (Disjunctions::iterator it = disjunctions_.begin();
         it != disjunctions_.end(); ++it)
    {
      delete *it;
    }
  }


  bool LookupIdentifierQuery::IsIdentifier(const DicomTag& tag)
  {
    return ServerToolbox::IsIdentifier(tag, level_);
  }


  void LookupIdentifierQuery::AddConstraint(DicomTag tag,
                                            IdentifierConstraintType type,
                                            const std::string& value)
  {
    assert(IsIdentifier(tag));
    disjunctions_.push_back(new Disjunction);
    disjunctions_.back()->Add(tag, type, value);
  }


  void LookupIdentifierQuery::AddRange(DicomTag tag,
                                       const std::string& start,
                                       const std::string& end)
  {
    assert(IsIdentifier(tag));
    disjunctions_.push_back(new Disjunction);
    disjunctions_.back()->AddRange(tag, start, end);
  }


  LookupIdentifierQuery::Disjunction& LookupIdentifierQuery::AddDisjunction()
  {
    disjunctions_.push_back(new Disjunction);
    return *disjunctions_.back();
  }


  void LookupIdentifierQuery::Apply(std::list<std::string>& result,
                                    IDatabaseWrapper& database)
  {
    SetOfResources resources(database, level_);
    Apply(resources, database);

    resources.Flatten(result);
  }


  void LookupIdentifierQuery::Apply(SetOfResources& result,
                                    IDatabaseWrapper& database)
  {
    for (size_t i = 0; i < disjunctions_.size(); i++)
    {
      std::list<int64_t> a;

      for (size_t j = 0; j < disjunctions_[i]->GetSingleConstraintsCount(); j++)
      {
        const SingleConstraint& constraint = disjunctions_[i]->GetSingleConstraint(j);
        std::list<int64_t> b;
        database.LookupIdentifier(b, level_, constraint.GetTag(), 
                                  constraint.GetType(), constraint.GetValue());

        a.splice(a.end(), b);
      }

      for (size_t j = 0; j < disjunctions_[i]->GetRangeConstraintsCount(); j++)
      {
        const RangeConstraint& constraint = disjunctions_[i]->GetRangeConstraint(j);
        std::list<int64_t> b;
        database.LookupIdentifierRange(b, level_, constraint.GetTag(), 
                                       constraint.GetStart(), constraint.GetEnd());

        a.splice(a.end(), b);
      }

      result.Intersect(a);
    }
  }


  void LookupIdentifierQuery::Print(std::ostream& s) const
  {
    s << "Constraint: " << std::endl;
    for (Disjunctions::const_iterator
           it = disjunctions_.begin(); it != disjunctions_.end(); ++it)
    {
      if (it == disjunctions_.begin())
        s << "   ";
      else
        s << "OR ";

      for (size_t j = 0; j < (*it)->GetSingleConstraintsCount(); j++)
      {
        const SingleConstraint& c = (*it)->GetSingleConstraint(j);
        s << FromDcmtkBridge::GetTagName(c.GetTag(), "");

        switch (c.GetType())
        {
          case IdentifierConstraintType_Equal: s << " == "; break;
          case IdentifierConstraintType_SmallerOrEqual: s << " <= "; break;
          case IdentifierConstraintType_GreaterOrEqual: s << " >= "; break;
          case IdentifierConstraintType_Wildcard: s << " ~= "; break;
          default:
            s << " ? ";
        }

        s << c.GetValue() << std::endl;
      }
    }
  }
}
