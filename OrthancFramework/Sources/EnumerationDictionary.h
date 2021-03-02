/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 **/


#pragma once

#include "OrthancException.h"

#include "Toolbox.h"
#include <boost/lexical_cast.hpp>
#include <string>
#include <map>

namespace Orthanc
{
  template <typename Enumeration>
  class EnumerationDictionary
  {
  private:
    typedef std::map<Enumeration, std::string>  EnumerationToString;
    typedef std::map<std::string, Enumeration>  StringToEnumeration;

    EnumerationToString enumerationToString_;
    StringToEnumeration stringToEnumeration_;

  public:
    void Clear()
    {
      enumerationToString_.clear();
      stringToEnumeration_.clear();
    }

    bool Contains(Enumeration value) const
    {
      return enumerationToString_.find(value) != enumerationToString_.end();
    }

    void Add(Enumeration value, const std::string& str)
    {
      // Check if these values are free
      if (enumerationToString_.find(value) != enumerationToString_.end() ||
          stringToEnumeration_.find(str) != stringToEnumeration_.end() ||
          Toolbox::IsInteger(str) /* Prevent the registration of a number */)
      {
        throw OrthancException(ErrorCode_BadRequest);
      }

      // OK, the string is free and is not a number
      enumerationToString_[value] = str;
      stringToEnumeration_[str] = value;
      stringToEnumeration_[boost::lexical_cast<std::string>(static_cast<int>(value))] = value;
    }

    Enumeration Translate(const std::string& str) const
    {
      if (Toolbox::IsInteger(str))
      {
        return static_cast<Enumeration>(boost::lexical_cast<int>(str));
      }

      typename StringToEnumeration::const_iterator
        found = stringToEnumeration_.find(str);

      if (found == stringToEnumeration_.end())
      {
        throw OrthancException(ErrorCode_InexistentItem);
      }
      else
      {
        return found->second;
      }
    }

    std::string Translate(Enumeration e) const
    {
      typename EnumerationToString::const_iterator
        found = enumerationToString_.find(e);

      if (found == enumerationToString_.end())
      {
        // No name for this item
        return boost::lexical_cast<std::string>(static_cast<int>(e));
      }
      else
      {
        return found->second;
      }
    }
  };
}
