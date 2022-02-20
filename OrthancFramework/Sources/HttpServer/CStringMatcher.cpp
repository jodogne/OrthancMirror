/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#include "../PrecompiledHeaders.h"
#include "CStringMatcher.h"

#include "../OrthancException.h"

#include <boost/algorithm/searching/boyer_moore.hpp>

namespace Orthanc
{
  class CStringMatcher::Search : public boost::noncopyable
  {
  private:
    typedef boost::algorithm::boyer_moore<const char*>  Algorithm;

    Algorithm algorithm_;

  public:
    // WARNING - The lifetime of "pattern_" must be larger than
    // "search_", as the latter internally keeps a pointer to "pattern" (*)
    explicit Search(const std::string& pattern) :
      algorithm_(pattern.c_str(), pattern.c_str() + pattern.size())
    {
    }

    const char* Apply(const char* start,
                      const char* end) const
    {
#if BOOST_VERSION >= 106200
      return algorithm_(start, end).first;
#else
      return algorithm_(start, end);
#endif
    }
  };



  CStringMatcher::CStringMatcher(const std::string& pattern) :
    pattern_(pattern),
    valid_(false),
    matchBegin_(NULL),
    matchEnd_(NULL)
  {
    // WARNING - Don't use "pattern" (local variable, will be
    // destroyed once exiting the constructor) but "pattern_"
    // (variable member, will last as long as the algorithm),
    // otherwise lifetime is bad! (*)
    search_.reset(new Search(pattern_));
  }

  const std::string& CStringMatcher::GetPattern() const
  {
    return pattern_;
  }

  bool CStringMatcher::IsValid() const
  {
    return valid_;
  }
  

  bool CStringMatcher::Apply(const char* start,
                             const char* end)
  {
    assert(search_.get() != NULL);

    if (start > end)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

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

  
  bool CStringMatcher::Apply(const std::string& corpus)
  {
    if (corpus.empty())
    {
      return false;
    }
    else
    {
      return Apply(corpus.c_str(), corpus.c_str() + corpus.size());
    }
  }


  const char* CStringMatcher::GetMatchBegin() const
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


  const char* CStringMatcher::GetMatchEnd() const
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
}
