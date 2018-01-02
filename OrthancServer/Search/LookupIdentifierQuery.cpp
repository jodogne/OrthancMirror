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
#include "LookupIdentifierQuery.h"

#include "../../Core/OrthancException.h"
#include "SetOfResources.h"
#include "../../Core/DicomParsing/FromDcmtkBridge.h"

#include <cassert>



namespace Orthanc
{
  LookupIdentifierQuery::Disjunction::~Disjunction()
  {
    for (size_t i = 0; i < disjunction_.size(); i++)
    {
      delete disjunction_[i];
    }
  }


  void LookupIdentifierQuery::Disjunction::Add(const DicomTag& tag,
                                               IdentifierConstraintType type,
                                               const std::string& value)
  {
    disjunction_.push_back(new Constraint(tag, type, value));
  }


  LookupIdentifierQuery::~LookupIdentifierQuery()
  {
    for (Constraints::iterator it = constraints_.begin();
         it != constraints_.end(); ++it)
    {
      delete *it;
    }
  }


  void LookupIdentifierQuery::AddConstraint(DicomTag tag,
                                            IdentifierConstraintType type,
                                            const std::string& value)
  {
    assert(IsIdentifier(tag));
    constraints_.push_back(new Disjunction);
    constraints_.back()->Add(tag, type, value);
  }


  LookupIdentifierQuery::Disjunction& LookupIdentifierQuery::AddDisjunction()
  {
    constraints_.push_back(new Disjunction);
    return *constraints_.back();
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
    for (size_t i = 0; i < GetSize(); i++)
    {
      std::list<int64_t> a;

      for (size_t j = 0; j < constraints_[i]->GetSize(); j++)
      {
        const Constraint& constraint = constraints_[i]->GetConstraint(j);
        std::list<int64_t> b;
        database.LookupIdentifier(b, level_, constraint.GetTag(), constraint.GetType(), constraint.GetValue());

        a.splice(a.end(), b);
      }

      result.Intersect(a);
    }
  }


  void LookupIdentifierQuery::Print(std::ostream& s) const
  {
    s << "Constraint: " << std::endl;
    for (Constraints::const_iterator
           it = constraints_.begin(); it != constraints_.end(); ++it)
    {
      if (it == constraints_.begin())
        s << "   ";
      else
        s << "OR ";

      for (size_t j = 0; j < (*it)->GetSize(); j++)
      {
        const Constraint& c = (*it)->GetConstraint(j);
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
