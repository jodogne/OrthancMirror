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
#include "DicomArray.h"

#include "../OrthancException.h"

#include <stdio.h>

namespace Orthanc
{
  DicomArray::DicomArray(const DicomMap& map)
  {
    elements_.reserve(map.content_.size());
    
    for (DicomMap::Content::const_iterator it = 
           map.content_.begin(); it != map.content_.end(); ++it)
    {
      elements_.push_back(new DicomElement(it->first, *it->second));
    }
  }


  DicomArray::~DicomArray()
  {
    for (size_t i = 0; i < elements_.size(); i++)
    {
      delete elements_[i];
    }
  }


  size_t DicomArray::GetSize() const
  {
    return elements_.size();
  }


  const DicomElement &DicomArray::GetElement(size_t i) const
  {
    if (i >= elements_.size())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
    else
    {
      return *elements_[i];
    }
  }

  void DicomArray::GetTags(std::set<DicomTag>& tags) const
  {
    tags.clear();

    for (size_t i = 0; i < elements_.size(); i++)
    {
      tags.insert(elements_[i]->GetTag());
    }
   
  }

  void DicomArray::Print(FILE* fp) const
  {
    for (size_t  i = 0; i < elements_.size(); i++)
    {
      DicomTag t = elements_[i]->GetTag();
      const DicomValue& v = elements_[i]->GetValue();
      std::string s = v.IsNull() ? "(null)" : v.GetContent();
      printf("0x%04x 0x%04x [%s]\n", t.GetGroup(), t.GetElement(), s.c_str());
    }
  }
}
