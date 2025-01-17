/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2025 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2025 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

#include "DatabaseDicomTagConstraint.h"

#include <deque>

namespace Orthanc
{
  class DatabaseDicomTagConstraints : public boost::noncopyable
  {
  private:
    std::deque<DatabaseDicomTagConstraint*>  constraints_;

  public:
    ~DatabaseDicomTagConstraints()
    {
      Clear();
    }

    void Clear();

    void AddConstraint(DatabaseDicomTagConstraint* constraint);  // Takes ownership

    bool IsEmpty() const
    {
      return constraints_.empty();
    }

    size_t GetSize() const
    {
      return constraints_.size();
    }

    const DatabaseDicomTagConstraint& GetConstraint(size_t index) const;

    std::string Format() const;
  };
}
