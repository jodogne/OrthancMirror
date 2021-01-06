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


#include "../PrecompiledHeaders.h"
#include "DicomElement.h"


namespace Orthanc
{
  DicomElement::DicomElement(uint16_t group,
                             uint16_t element,
                             const DicomValue &value) :
    tag_(group, element),
    value_(value.Clone())
  {
  }

  DicomElement::DicomElement(const DicomTag &tag,
                             const DicomValue &value) :
    tag_(tag),
    value_(value.Clone())
  {
  }

  DicomElement::~DicomElement()
  {
    delete value_;
  }

  const DicomTag &DicomElement::GetTag() const
  {
    return tag_;
  }

  const DicomValue &DicomElement::GetValue() const
  {
    return *value_;
  }

  uint16_t DicomElement::GetTagGroup() const
  {
    return tag_.GetGroup();
  }

  uint16_t DicomElement::GetTagElement() const
  {
    return tag_.GetElement();
  }

  bool DicomElement::operator<(const DicomElement &other) const
  {
    return GetTag() < other.GetTag();
  }
}
