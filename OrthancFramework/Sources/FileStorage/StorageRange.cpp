/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2026 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2026 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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
#include "StorageRange.h"

#include "../OrthancException.h"
#include "../Toolbox.h"
#include "../SerializationToolbox.h"

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>


#include <signal.h>

namespace Orthanc
{
  void StorageRange::SanityCheck() const
  {
    if (hasStart_ && hasEnd_ && start_ > end_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }

  StorageRange::StorageRange():
    hasStart_(false),
    start_(0),
    hasEnd_(false),
    end_(0)
  {
  }

  void StorageRange::SetStartInclusive(uint64_t start)
  {
    hasStart_ = true;
    start_ = start;
  }

  void StorageRange::SetEndInclusive(uint64_t end)
  {
    hasEnd_ = true;
    end_ = end;
  }

  uint64_t StorageRange::GetStartInclusive() const
  {
    if (!hasStart_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else if (hasEnd_ && start_ > end_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      return start_;
    }
  }

  uint64_t StorageRange::GetEndInclusive() const
  {
    if (!hasEnd_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else if (hasStart_ && start_ > end_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      return end_;
    }
  }

  std::string StorageRange::FormatHttpContentRange(uint64_t fullSize) const
  {
    SanityCheck();

    if (fullSize == 0 ||
        (hasStart_ && start_ >= fullSize) ||
        (hasEnd_ && end_ >= fullSize))
    {
      throw OrthancException(ErrorCode_BadRange);
    }

    std::string s = "bytes ";

    if (hasStart_)
    {
      s += boost::lexical_cast<std::string>(start_);
    }
    else
    {
      s += "0";
    }

    s += "-";

    if (hasEnd_)
    {
      s += boost::lexical_cast<std::string>(end_);
    }
    else
    {
      s += boost::lexical_cast<std::string>(fullSize - 1);
    }

    return s + "/" + boost::lexical_cast<std::string>(fullSize);
  }


  void StorageRange::Extract(std::string &target,
                             const std::string &source) const
  {
    Extract(target, source.empty() ? NULL : source.c_str(), source.size());
  }


  void StorageRange::Extract(std::string& target,
                             const void* data,
                             size_t size) const
  {
    SanityCheck();

    if (size != 0 &&
        data == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }

    if (hasStart_ && start_ >= size)
    {
      throw OrthancException(ErrorCode_BadRange);
    }

    if (hasEnd_ && end_ >= size)
    {
      throw OrthancException(ErrorCode_BadRange);
    }

    if (hasStart_ && hasEnd_)
    {
      target.assign(reinterpret_cast<const char*>(data) + start_, end_ - start_ + 1);
    }
    else if (hasStart_)
    {
      target.assign(reinterpret_cast<const char*>(data) + start_, size - start_);
    }
    else if (hasEnd_)
    {
      target.assign(reinterpret_cast<const char*>(data), end_ + 1);
    }
    else
    {
      target.assign(reinterpret_cast<const char*>(data), size);
    }
  }


  uint64_t StorageRange::GetContentLength(uint64_t fullSize) const
  {
    SanityCheck();

    if (fullSize == 0)
    {
      throw OrthancException(ErrorCode_BadRange);
    }

    if (hasStart_ && start_ >= fullSize)
    {
      throw OrthancException(ErrorCode_BadRange);
    }

    if (hasEnd_ && end_ >= fullSize)
    {
      throw OrthancException(ErrorCode_BadRange);
    }

    if (hasStart_ && hasEnd_)
    {
      return end_ - start_ + 1;
    }
    else if (hasStart_)
    {
      return fullSize - start_;
    }
    else if (hasEnd_)
    {
      return end_ + 1;
    }
    else
    {
      return fullSize;
    }
  }

  StorageRange StorageRange::ParseHttpRange(const std::string& s)
  {
    static const std::string BYTES = "bytes=";

    if (!boost::starts_with(s, BYTES))
    {
      throw OrthancException(ErrorCode_BadRange);  // Range not satisfiable
    }

    std::vector<std::string> tokens;
    Orthanc::Toolbox::TokenizeString(tokens, s.substr(BYTES.length()), '-');

    if (tokens.size() != 2)
    {
      throw OrthancException(ErrorCode_BadRange);
    }

    StorageRange range;

    uint64_t tmp;
    if (!tokens[0].empty())
    {
      if (SerializationToolbox::ParseUnsignedInteger64(tmp, tokens[0]))
      {
        range.SetStartInclusive(tmp);
      }
    }

    if (!tokens[1].empty())
    {
      if (SerializationToolbox::ParseUnsignedInteger64(tmp, tokens[1]))
      {
        range.SetEndInclusive(tmp);
      }
    }

    range.SanityCheck();
    return range;
  }
}
