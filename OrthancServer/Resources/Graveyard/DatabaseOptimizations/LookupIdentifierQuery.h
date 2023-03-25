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


#pragma once

#include "../IDatabaseWrapper.h"

#include "SetOfResources.h"

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
    // This class encodes a conjunction ("AND") of disjunctions. Each
    // disjunction represents an "OR" of several constraints.

  public:
    class SingleConstraint
    {
    private:
      DicomTag                  tag_;
      IdentifierConstraintType  type_;
      std::string               value_;

    public:
      SingleConstraint(const DicomTag& tag,
                       IdentifierConstraintType type,
                       const std::string& value);

      const DicomTag& GetTag() const
      {
        return tag_;
      }

      IdentifierConstraintType GetType() const
      {
        return type_;
      }
      
      const std::string& GetValue() const
      {
        return value_;
      }
    };


    class RangeConstraint
    {
    private:
      DicomTag     tag_;
      std::string  start_;
      std::string  end_;

    public:
      RangeConstraint(const DicomTag& tag,
                      const std::string& start,
                      const std::string& end);

      const DicomTag& GetTag() const
      {
        return tag_;
      }

      const std::string& GetStart() const
      {
        return start_;
      }

      const std::string& GetEnd() const
      {
        return end_;
      }
    };


    class Disjunction : public boost::noncopyable
    {
    private:
      std::vector<SingleConstraint*>  singleConstraints_;
      std::vector<RangeConstraint*>   rangeConstraints_;

    public:
      ~Disjunction();

      void Add(const DicomTag& tag,
               IdentifierConstraintType type,
               const std::string& value);

      void AddRange(const DicomTag& tag,
                    const std::string& start,
                    const std::string& end);

      size_t GetSingleConstraintsCount() const
      {
        return singleConstraints_.size();
      }

      const SingleConstraint&  GetSingleConstraint(size_t i) const
      {
        return *singleConstraints_[i];
      }

      size_t GetRangeConstraintsCount() const
      {
        return rangeConstraints_.size();
      }

      const RangeConstraint&  GetRangeConstraint(size_t i) const
      {
        return *rangeConstraints_[i];
      }
    };


  private:
    typedef std::vector<Disjunction*>  Disjunctions;

    ResourceType  level_;
    Disjunctions  disjunctions_;

  public:
    LookupIdentifierQuery(ResourceType level) : level_(level)
    {
    }

    ~LookupIdentifierQuery();

    bool IsIdentifier(const DicomTag& tag);

    void AddConstraint(DicomTag tag,
                       IdentifierConstraintType type,
                       const std::string& value);

    void AddRange(DicomTag tag,
                  const std::string& start,
                  const std::string& end);

    Disjunction& AddDisjunction();

    ResourceType GetLevel() const
    {
      return level_;
    }

    // The database must be locked
    void Apply(std::list<std::string>& result,
               IDatabaseWrapper& database);

    void Apply(SetOfResources& result,
               IDatabaseWrapper& database);

    void Print(std::ostream& s) const;
  };
}
