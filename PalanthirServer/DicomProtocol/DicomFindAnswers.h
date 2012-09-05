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

#include "../../Core/DicomFormat/DicomMap.h"

#include <vector>
#include <json/json.h>

namespace Palanthir
{
  class DicomFindAnswers
  {
  private:
    std::vector<DicomMap*> items_;

  public:
    ~DicomFindAnswers()
    {
      Clear();
    }

    void Clear();

    void Reserve(size_t index);

    void Add(const DicomMap& map)
    {
      items_.push_back(map.Clone());
    }

    size_t GetSize() const
    {
      return items_.size();
    }

    const DicomMap& GetAnswer(size_t index) const
    {
      return *items_.at(index);
    }

    void ToJson(Json::Value& target) const;
  };
}
