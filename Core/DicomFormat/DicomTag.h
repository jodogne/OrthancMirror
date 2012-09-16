/**
 * Orthanc - A Lightweight, RESTful DICOM Store
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

#include <string>
#include <stdint.h>


namespace Orthanc
{
  class DicomTag
  {
    // This must stay a POD (plain old data structure) 

  private:
    uint16_t group_;
    uint16_t element_;

  public:
    DicomTag(uint16_t group,
             uint16_t element) :
      group_(group),
      element_(element)
    {
    }

    uint16_t GetGroup() const
    {
      return group_;
    }

    uint16_t GetElement() const
    {
      return element_;
    }

    bool operator< (const DicomTag& other) const;

    std::string Format() const;

    friend std::ostream& operator<< (std::ostream& o, const DicomTag& tag);

    // Alias for the most useful tags
    static const DicomTag ACCESSION_NUMBER;
    static const DicomTag IMAGE_INDEX;
    static const DicomTag INSTANCE_UID;
    static const DicomTag NUMBER_OF_SLICES;
    static const DicomTag PATIENT_ID;
    static const DicomTag SERIES_UID;
    static const DicomTag STUDY_UID;
    static const DicomTag PIXEL_DATA;
  };
}
