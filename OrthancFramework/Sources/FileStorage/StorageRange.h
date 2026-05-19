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


#pragma once

#include "../OrthancFramework.h"

#include <stdint.h>
#include <string>


namespace Orthanc
{
  // In Orthanc <= 1.12.11, this class was known as "StorageAccessor::Range"
  class ORTHANC_PUBLIC StorageRange
  {
  private:
    bool      hasStart_;
    uint64_t  start_;
    bool      hasEnd_;
    uint64_t  end_;

    void SanityCheck() const;

  public:
    StorageRange();

    void SetStartInclusive(uint64_t start);

    void SetEndInclusive(uint64_t end);

    bool HasStart() const
    {
      return hasStart_;
    }

    bool HasEnd() const
    {
      return hasEnd_;
    }

    uint64_t GetStartInclusive() const;

    uint64_t GetEndInclusive() const;

    std::string FormatHttpContentRange(uint64_t fullSize) const;

    void Extract(std::string& target,
                 const std::string& source) const;

    uint64_t GetContentLength(uint64_t fullSize) const;

    static StorageRange ParseHttpRange(const std::string& s);
  };
}
