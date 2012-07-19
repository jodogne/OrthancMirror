/**
 * Palantir - A Lightweight, RESTful DICOM Store
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


#include "DicomFindAnswers.h"

#include "../FromDcmtkBridge.h"

namespace Palantir
{
  void DicomFindAnswers::Clear()
  {
    for (size_t i = 0; i < items_.size(); i++)
    {
      delete items_[i];
    }
  }

  void DicomFindAnswers::Reserve(size_t size)
  {
    if (size > items_.size())
    {
      items_.reserve(size);
    }
  }

  void DicomFindAnswers::ToJson(Json::Value& target) const
  {
    target = Json::arrayValue;

    for (size_t i = 0; i < GetSize(); i++)
    {
      Json::Value answer(Json::objectValue);
      FromDcmtkBridge::ToJson(answer, GetAnswer(i));
      target.append(answer);
    }
  }
}
