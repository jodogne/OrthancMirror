/**
 * Palanthir - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012 Medical Physics Department, CHU of Liege,
 * Belgium
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

#include "DicomElement.h"
#include "DicomMap.h"

#include <vector>

namespace Palanthir
{
  class DicomArray : public boost::noncopyable
  {
  private:
    typedef std::vector<DicomElement*>  Elements;

    Elements  elements_;

  public:
    DicomArray(const DicomMap& map);

    ~DicomArray();

    size_t GetSize() const
    {
      return elements_.size();
    }

    const DicomElement& GetElement(size_t i) const
    {
      return *elements_[i];
    }

    void Print(FILE* fp) const;
  };
}
