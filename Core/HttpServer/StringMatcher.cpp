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


#include "../PrecompiledHeaders.h"
#include "StringMatcher.h"

#include "../OrthancException.h"

#include <boost/algorithm/searching/boyer_moore.hpp>
//#include <boost/algorithm/searching/boyer_moore_horspool.hpp>
//#include <boost/algorithm/searching/knuth_morris_pratt.hpp>

namespace Orthanc
{
  class StringMatcher::Search
  {
  private:
    typedef boost::algorithm::boyer_moore<Iterator>  Algorithm;
    //typedef boost::algorithm::boyer_moore_horspool<std::string::const_iterator>  Algorithm;

    Algorithm algorithm_;

  public:
    // WARNING - The lifetime of "pattern_" must be larger than
    // "search_", as the latter internally keeps a pointer to "pattern" (*)
    Search(const std::string& pattern) :
      algorithm_(pattern.begin(), pattern.end())
    {
    }

    Iterator Apply(Iterator start,
                   Iterator end) const
    {
#if BOOST_VERSION >= 106200
      return algorithm_(start, end).first;
#else
      return algorithm_(start, end);
#endif
    }
  };
    

  StringMatcher::StringMatcher(const std::string& pattern) :
    pattern_(pattern),
    valid_(false)
  {
    // WARNING - Don't use "pattern" (local variable, will be
    // destroyed once exiting the constructor) but "pattern_"
    // (variable member, will last as long as the algorithm),
    // otherwise lifetime is bad! (*)
    search_.reset(new Search(pattern_));
  }
  

  bool StringMatcher::Apply(Iterator start,
                            Iterator end)
  {
    assert(search_.get() != NULL);
    matchBegin_ = search_->Apply(start, end);

    if (matchBegin_ == end)
    {
      valid_ = false;
    }
    else
    {
      matchEnd_ = matchBegin_ + pattern_.size();
      assert(matchEnd_ <= end);
      valid_ = true;
    }

    return valid_;
  }


  StringMatcher::Iterator StringMatcher::GetMatchBegin() const
  {
    if (valid_)
    {
      return matchBegin_;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }


  StringMatcher::Iterator StringMatcher::GetMatchEnd() const
  {
    if (valid_)
    {
      return matchEnd_;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }


  const char* StringMatcher::GetPointerBegin() const
  {
    return &GetMatchBegin()[0];
  }


  const char* StringMatcher::GetPointerEnd() const
  {
    return GetPointerBegin() + pattern_.size();
  }
}
