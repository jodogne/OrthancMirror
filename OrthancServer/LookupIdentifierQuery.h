/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2015 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
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

#include "ServerEnumerations.h"
#include "IDatabaseWrapper.h"

#include <vector>
#include <boost/noncopyable.hpp>

namespace Orthanc
{
  /**
   * Primitive for wildcard matching, as defined in DICOM:
   * http://dicom.nema.org/dicom/2013/output/chtml/part04/sect_C.2.html#sect_C.2.2.2.4
   * 
   * "Any occurrence of an "*" or a "?", then "*" shall match any
   * sequence of characters (including a zero length value) and "?"
   * shall match any single character. This matching is case
   * sensitive, except for Attributes with an PN Value
   * Representation (e.g., Patient Name (0010,0010))."
   * 
   * Pay attention to the fact that "*" (resp. "?") generally
   * corresponds to "%" (resp. "_") in primitive LIKE of SQL. The
   * values "%", "_", "\" should in the user request should
   * respectively be escaped as "\%", "\_" and "\\".
   * 
   * This matching must be case sensitive: The special case of PN VR
   * is taken into consideration by normalizing the query string in
   * method "NormalizeIdentifier()".
   **/



  class LookupIdentifierQuery : public boost::noncopyable
  {
  private:
    struct Constraint
    {
      DicomTag                  tag_;
      IdentifierConstraintType  type_;
      std::string               value_;

      Constraint(const DicomTag& tag,
                 IdentifierConstraintType type,
                 const std::string& value) : 
        tag_(tag),
        type_(type),
        value_(value)
      {
      }
    };

    typedef std::vector<Constraint*>  Constraints;

    ResourceType  level_;
    Constraints   constraints_;

    void CheckIndex(size_t index) const;

    static std::string NormalizeIdentifier(const std::string& value);

  public:
    LookupIdentifierQuery(ResourceType level) : level_(level)
    {
    }

    ~LookupIdentifierQuery();

    bool IsIdentifier(const DicomTag& tag) const;

    void AddConstraint(DicomTag tag,
                       IdentifierConstraintType type,
                       const std::string& value);

    size_t GetSize()
    {
      return constraints_.size();
    }

    const DicomTag& GetTag(size_t index) const;

    IdentifierConstraintType  GetType(size_t index) const;

    const std::string& GetValue(size_t index) const;

    static void StoreIdentifiers(IDatabaseWrapper& database,
                                 int64_t resource,
                                 ResourceType level,
                                 const DicomMap& map);
    
  };
}
