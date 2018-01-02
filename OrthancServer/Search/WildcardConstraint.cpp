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
#include "WildcardConstraint.h"

#include <boost/regex.hpp>

namespace Orthanc
{
  struct WildcardConstraint::PImpl
  {
    boost::regex  pattern_;
    std::string   wildcard_;
    bool          isCaseSensitive_;

    PImpl(const std::string& wildcard,
          bool isCaseSensitive)
    {
      isCaseSensitive_ = isCaseSensitive;
    
      if (isCaseSensitive)
      {
        wildcard_ = wildcard;
      }
      else
      {
        wildcard_ = Toolbox::ToUpperCaseWithAccents(wildcard);
      }

      pattern_ = boost::regex(Toolbox::WildcardToRegularExpression(wildcard_));
    }
  };


  WildcardConstraint::WildcardConstraint(const WildcardConstraint& other) :
    pimpl_(new PImpl(*other.pimpl_))
  {
  }


  WildcardConstraint::WildcardConstraint(const std::string& wildcard,
                                         bool isCaseSensitive) :
    pimpl_(new PImpl(wildcard, isCaseSensitive))
  {
  }

  bool WildcardConstraint::Match(const std::string& value) const
  {
    if (pimpl_->isCaseSensitive_)
    {
      return boost::regex_match(value, pimpl_->pattern_);
    }
    else
    {
      return boost::regex_match(Toolbox::ToUpperCaseWithAccents(value), pimpl_->pattern_);
    }
  }

  void WildcardConstraint::Setup(LookupIdentifierQuery& lookup,
                                 const DicomTag& tag) const
  {
    lookup.AddConstraint(tag, IdentifierConstraintType_Wildcard, pimpl_->wildcard_);
  }

  std::string WildcardConstraint::Format() const
  {
    return pimpl_->wildcard_;
  }
}
