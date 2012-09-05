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


#include "DicomTag.h"

#include "../PalanthirException.h"

#include <iostream>
#include <iomanip>
#include <stdio.h>

namespace Palanthir
{
  bool DicomTag::operator< (const DicomTag& other) const
  {
    if (group_ < other.group_)
      return true;

    if (group_ > other.group_)
      return false;

    return element_ < other.element_;
  }


  std::ostream& operator<< (std::ostream& o, const DicomTag& tag)
  {
    using namespace std;
    ios_base::fmtflags state = o.flags();
    o.flags(ios::right | ios::hex);
    o << "(" << setfill('0') << setw(4) << tag.GetGroup()
      << "," << setw(4) << tag.GetElement() << ")";
    o.flags(state);
    return o;
  }


  std::string DicomTag::Format() const
  {
    char b[16];
    sprintf(b, "%04x,%04x", group_, element_);
    return std::string(b);
  }


  const DicomTag DicomTag::ACCESSION_NUMBER = DicomTag(0x0008, 0x0050);
  const DicomTag DicomTag::IMAGE_INDEX = DicomTag(0x0054, 0x1330);
  const DicomTag DicomTag::INSTANCE_UID = DicomTag(0x0008, 0x0018);
  const DicomTag DicomTag::NUMBER_OF_SLICES = DicomTag(0x0054, 0x0081);
  const DicomTag DicomTag::PATIENT_ID = DicomTag(0x0010, 0x0020);
  const DicomTag DicomTag::SERIES_UID = DicomTag(0x0020, 0x000e);
  const DicomTag DicomTag::STUDY_UID = DicomTag(0x0020, 0x000d);
  const DicomTag DicomTag::PIXEL_DATA = DicomTag(0x7fe0, 0x0010);
}
