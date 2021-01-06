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

#include "../HttpServer/HttpToolbox.h"

#include <map>

namespace Orthanc
{
  class ORTHANC_PUBLIC RestApiPath : public boost::noncopyable
  {
  private:
    UriComponents uri_;
    bool hasTrailing_;
    std::vector<std::string> components_;

  public:
    explicit RestApiPath(const std::string& uri);

    // This version is slower
    bool Match(HttpToolbox::Arguments& components,
               UriComponents& trailing,
               const std::string& uriRaw) const;

    bool Match(HttpToolbox::Arguments& components,
               UriComponents& trailing,
               const UriComponents& uri) const;

    bool Match(const UriComponents& uri) const;

    size_t GetLevelCount() const;

    bool IsWildcardLevel(size_t level) const;

    bool IsUniversalTrailing() const;

    const std::string& GetWildcardName(size_t level) const;

    const std::string& GetLevelName(size_t level) const;

  };
}
