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


#include "../PrecompiledHeaders.h"
#include "RestApiPath.h"

#include "../OrthancException.h"

#include <cassert>

namespace Orthanc
{
  RestApiPath::RestApiPath(const std::string& uri)
  {
    Toolbox::SplitUriComponents(uri_, uri);

    if (uri_.size() == 0)
    {
      hasTrailing_ = false;
      return;
    }

    if (uri_.back() == "*")
    {
      hasTrailing_ = true;
      uri_.pop_back();
    }
    else
    {
      hasTrailing_ = false;
    }

    components_.resize(uri_.size());
    for (size_t i = 0; i < uri_.size(); i++)
    {
      size_t s = uri_[i].size();
      assert(s > 0);

      if (uri_[i][0] == '{' && 
          uri_[i][s - 1] == '}')
      {
        components_[i] = uri_[i].substr(1, s - 2);
        uri_[i] = "";
      }
      else
      {
        components_[i] = "";
      }
    }
  }

  bool RestApiPath::Match(IHttpHandler::Arguments& components,
                          UriComponents& trailing,
                          const std::string& uriRaw) const
  {
    UriComponents uri;
    Toolbox::SplitUriComponents(uri, uriRaw);
    return Match(components, trailing, uri);
  }

  bool RestApiPath::Match(IHttpHandler::Arguments& components,
                          UriComponents& trailing,
                          const UriComponents& uri) const
  {
    assert(uri_.size() == components_.size());

    if (uri.size() < uri_.size())
    {
      return false;
    }

    if (!hasTrailing_ && uri.size() > uri_.size())
    {
      return false;
    }

    components.clear();
    trailing.clear();

    assert(uri_.size() <= uri.size());
    for (size_t i = 0; i < uri_.size(); i++)
    {
      if (components_[i].size() == 0)
      {
        // This URI component is not a free parameter
        if (uri_[i] != uri[i])
        {
          return false;
        }
      }
      else
      {
        // This URI component is a free parameter
        components[components_[i]] = uri[i];
      }
    }

    if (hasTrailing_)
    {
      trailing.assign(uri.begin() + uri_.size(), uri.end());
    }

    return true;
  }


  bool RestApiPath::Match(const UriComponents& uri) const
  {
    IHttpHandler::Arguments components;
    UriComponents trailing;
    return Match(components, trailing, uri);
  }


  bool RestApiPath::IsWildcardLevel(size_t level) const
  {
    assert(uri_.size() == components_.size());

    if (level >= uri_.size())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    return uri_[level].length() == 0;
  }

  const std::string& RestApiPath::GetWildcardName(size_t level) const
  {
    assert(uri_.size() == components_.size());

    if (!IsWildcardLevel(level))
    {
      throw OrthancException(ErrorCode_BadParameterType);
    }

    return components_[level];
  }

  const std::string& RestApiPath::GetLevelName(size_t level) const
  {
    assert(uri_.size() == components_.size());

    if (IsWildcardLevel(level))
    {
      throw OrthancException(ErrorCode_BadParameterType);
    }

    return uri_[level];
  }
}

