/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2021-2023 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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
#include "DicomTag.h"

#include "../OrthancException.h"

#include <iostream>
#include <iomanip>
#include <stdio.h>
#include <string.h>

namespace Orthanc
{ 
  static inline uint16_t GetCharValue(char c)
  {
    if (c >= '0' && c <= '9')
      return c - '0';
    else if (c >= 'a' && c <= 'f')
      return c - 'a' + 10;
    else if (c >= 'A' && c <= 'F')
      return c - 'A' + 10;
    else
      return 0;
  }


  static inline uint16_t GetTagValue(const char* c)
  {
    return ((GetCharValue(c[0]) << 12) + 
            (GetCharValue(c[1]) << 8) + 
            (GetCharValue(c[2]) << 4) + 
            GetCharValue(c[3]));
  }


  DicomTag::DicomTag(uint16_t group, uint16_t element) :
    group_(group),
    element_(element)
  {
  }

  uint16_t DicomTag::GetGroup() const
  {
    return group_;
  }

  uint16_t DicomTag::GetElement() const
  {
    return element_;
  }

  bool DicomTag::IsPrivate() const
  {
    return group_ % 2 == 1;
  }


  bool DicomTag::operator< (const DicomTag& other) const
  {
    if (group_ < other.group_)
      return true;

    if (group_ > other.group_)
      return false;

    return element_ < other.element_;
  }


  bool DicomTag::operator<= (const DicomTag& other) const
  {
    if (group_ < other.group_)
      return true;

    if (group_ > other.group_)
      return false;

    return element_ <= other.element_;
  }

  bool DicomTag::operator>(const DicomTag &other) const
  {
    return !(*this <= other);
  }

  bool DicomTag::operator>=(const DicomTag &other) const
  {
    return !(*this < other);
  }

  bool DicomTag::operator==(const DicomTag &other) const
  {
    return group_ == other.group_ && element_ == other.element_;
  }

  bool DicomTag::operator!=(const DicomTag &other) const
  {
    return !(*this == other);
  }


  std::ostream& DicomTag::FormatStream(std::ostream& o) const
  {
    using namespace std;
    ios_base::fmtflags state = o.flags();
    o.flags(ios::right | ios::hex);
    o << "(" << setfill('0') << setw(4) << GetGroup()
      << "," << setw(4) << GetElement() << ")";
    o.flags(state);
    return o;
  }


  std::string DicomTag::Format() const
  {
    char b[16];
    sprintf(b, "%04x,%04x", group_, element_);
    return std::string(b);
  }


  bool DicomTag::ParseHexadecimal(DicomTag& tag,
                                  const char* value)
  {
    size_t length = strlen(value);

    if (length == 9 &&
        isxdigit(value[0]) &&
        isxdigit(value[1]) &&
        isxdigit(value[2]) &&
        isxdigit(value[3]) &&
        (value[4] == '-' || value[4] == ',') &&
        isxdigit(value[5]) &&
        isxdigit(value[6]) &&
        isxdigit(value[7]) &&
        isxdigit(value[8]))        
    {
      uint16_t group = GetTagValue(value);
      uint16_t element = GetTagValue(value + 5);
      tag = DicomTag(group, element);
      return true;
    }
    else if (length == 8 &&
             isxdigit(value[0]) &&
             isxdigit(value[1]) &&
             isxdigit(value[2]) &&
             isxdigit(value[3]) &&
             isxdigit(value[4]) &&
             isxdigit(value[5]) &&
             isxdigit(value[6]) &&
             isxdigit(value[7])) 
    {
      uint16_t group = GetTagValue(value);
      uint16_t element = GetTagValue(value + 4);
      tag = DicomTag(group, element);
      return true;
    }
    else
    {
      return false;
    }
  }


  void DicomTag::AddTagsForModule(std::set<DicomTag>& target,
                                  DicomModule module)
  {
    // REFERENCE: 11_03pu.pdf, DICOM PS 3.3 2011 - Information Object Definitions

    switch (module)
    {
      case DicomModule_Patient:
        // This is Table C.7-1 "Patient Module Attributes" (p. 373)
        target.insert(DicomTag(0x0010, 0x0010));   // Patient's name
        target.insert(DicomTag(0x0010, 0x0020));   // Patient ID
        target.insert(DicomTag(0x0010, 0x0030));   // Patient's birth date
        target.insert(DicomTag(0x0010, 0x0040));   // Patient's sex
        target.insert(DicomTag(0x0008, 0x1120));   // Referenced patient sequence
        target.insert(DicomTag(0x0010, 0x0032));   // Patient's birth time
        target.insert(DicomTag(0x0010, 0x1000));   // Other patient IDs
        target.insert(DicomTag(0x0010, 0x1002));   // Other patient IDs sequence
        target.insert(DicomTag(0x0010, 0x1001));   // Other patient names
        target.insert(DicomTag(0x0010, 0x2160));   // Ethnic group
        target.insert(DicomTag(0x0010, 0x4000));   // Patient comments
        target.insert(DicomTag(0x0010, 0x2201));   // Patient species description
        target.insert(DicomTag(0x0010, 0x2202));   // Patient species code sequence
        target.insert(DicomTag(0x0010, 0x2292));   // Patient breed description
        target.insert(DicomTag(0x0010, 0x2293));   // Patient breed code sequence
        target.insert(DicomTag(0x0010, 0x2294));   // Breed registration sequence
        target.insert(DicomTag(0x0010, 0x2297));   // Responsible person
        target.insert(DicomTag(0x0010, 0x2298));   // Responsible person role
        target.insert(DicomTag(0x0010, 0x2299));   // Responsible organization
        target.insert(DicomTag(0x0012, 0x0062));   // Patient identity removed
        target.insert(DicomTag(0x0012, 0x0063));   // De-identification method
        target.insert(DicomTag(0x0012, 0x0064));   // De-identification method code sequence

        // Table 10-18 ISSUER OF PATIENT ID MACRO (p. 112)
        target.insert(DicomTag(0x0010, 0x0021));   // Issuer of Patient ID
        target.insert(DicomTag(0x0010, 0x0024));   // Issuer of Patient ID qualifiers sequence
        break;

      case DicomModule_Study:
        // This is Table C.7-3 "General Study Module Attributes" (p. 378)
        target.insert(DicomTag(0x0020, 0x000d));   // Study instance UID
        target.insert(DicomTag(0x0008, 0x0020));   // Study date
        target.insert(DicomTag(0x0008, 0x0030));   // Study time
        target.insert(DicomTag(0x0008, 0x0090));   // Referring physician's name
        target.insert(DicomTag(0x0008, 0x0096));   // Referring physician identification sequence
        target.insert(DicomTag(0x0020, 0x0010));   // Study ID
        target.insert(DicomTag(0x0008, 0x0050));   // Accession number
        target.insert(DicomTag(0x0008, 0x0051));   // Issuer of accession number sequence
        target.insert(DicomTag(0x0008, 0x1030));   // Study description
        target.insert(DicomTag(0x0008, 0x1048));   // Physician(s) of record
        target.insert(DicomTag(0x0008, 0x1049));   // Physician(s) of record identification sequence
        target.insert(DicomTag(0x0008, 0x1060));   // Name of physician(s) reading study
        target.insert(DicomTag(0x0008, 0x1062));   // Physician(s) reading study identification sequence
        target.insert(DicomTag(0x0032, 0x1034));   // Requesting service code sequence
        target.insert(DicomTag(0x0008, 0x1110));   // Referenced study sequence
        target.insert(DicomTag(0x0008, 0x1032));   // Procedure code sequence
        target.insert(DicomTag(0x0040, 0x1012));   // Reason for performed procedure code sequence
        break;

      case DicomModule_Series:
        // This is Table C.7-5 "General Series Module Attributes" (p. 385)
        target.insert(DicomTag(0x0008, 0x0060));   // Modality 
        target.insert(DicomTag(0x0020, 0x000e));   // Series Instance UID 
        target.insert(DicomTag(0x0020, 0x0011));   // Series Number 
        target.insert(DicomTag(0x0020, 0x0060));   // Laterality 
        target.insert(DicomTag(0x0008, 0x0021));   // Series Date 
        target.insert(DicomTag(0x0008, 0x0031));   // Series Time 
        target.insert(DicomTag(0x0008, 0x1050));   // Performing Physicians’ Name 
        target.insert(DicomTag(0x0008, 0x1052));   // Performing Physician Identification Sequence 
        target.insert(DicomTag(0x0018, 0x1030));   // Protocol Name
        target.insert(DicomTag(0x0008, 0x103e));   // Series Description 
        target.insert(DicomTag(0x0008, 0x103f));   // Series Description Code Sequence 
        target.insert(DicomTag(0x0008, 0x1070));   // Operators' Name 
        target.insert(DicomTag(0x0008, 0x1072));   // Operator Identification Sequence 
        target.insert(DicomTag(0x0008, 0x1111));   // Referenced Performed Procedure Step Sequence
        target.insert(DicomTag(0x0008, 0x1250));   // Related Series Sequence
        target.insert(DicomTag(0x0018, 0x0015));   // Body Part Examined
        target.insert(DicomTag(0x0018, 0x5100));   // Patient Position
        target.insert(DicomTag(0x0028, 0x0108));   // Smallest Pixel Value in Series 
        target.insert(DicomTag(0x0029, 0x0109));   // Largest Pixel Value in Series 
        target.insert(DicomTag(0x0040, 0x0275));   // Request Attributes Sequence 
        target.insert(DicomTag(0x0010, 0x2210));   // Anatomical Orientation Type

        // Table 10-16 PERFORMED PROCEDURE STEP SUMMARY MACRO ATTRIBUTES
        target.insert(DicomTag(0x0040, 0x0253));   // Performed Procedure Step ID 
        target.insert(DicomTag(0x0040, 0x0244));   // Performed Procedure Step Start Date 
        target.insert(DicomTag(0x0040, 0x0245));   // Performed Procedure Step Start Time 
        target.insert(DicomTag(0x0040, 0x0254));   // Performed Procedure Step Description 
        target.insert(DicomTag(0x0040, 0x0260));   // Performed Protocol Code Sequence 
        target.insert(DicomTag(0x0040, 0x0280));   // Comments on the Performed Procedure Step
        break;

      case DicomModule_Instance:
        // This is Table C.12-1 "SOP Common Module Attributes" (p. 1207)
        target.insert(DicomTag(0x0008, 0x0016));   // SOP Class UID
        target.insert(DicomTag(0x0008, 0x0018));   // SOP Instance UID 
        target.insert(DicomTag(0x0008, 0x0005));   // Specific Character Set 
        target.insert(DicomTag(0x0008, 0x0012));   // Instance Creation Date 
        target.insert(DicomTag(0x0008, 0x0013));   // Instance Creation Time 
        target.insert(DicomTag(0x0008, 0x0014));   // Instance Creator UID 
        target.insert(DicomTag(0x0008, 0x001a));   // Related General SOP Class UID 
        target.insert(DicomTag(0x0008, 0x001b));   // Original Specialized SOP Class UID 
        target.insert(DicomTag(0x0008, 0x0110));   // Coding Scheme Identification Sequence 
        target.insert(DicomTag(0x0008, 0x0201));   // Timezone Offset From UTC 
        target.insert(DicomTag(0x0018, 0xa001));   // Contributing Equipment Sequence
        target.insert(DicomTag(0x0020, 0x0013));   // Instance Number 
        target.insert(DicomTag(0x0100, 0x0410));   // SOP Instance Status 
        target.insert(DicomTag(0x0100, 0x0420));   // SOP Authorization DateTime 
        target.insert(DicomTag(0x0100, 0x0424));   // SOP Authorization Comment 
        target.insert(DicomTag(0x0100, 0x0426));   // Authorization Equipment Certification Number
        target.insert(DicomTag(0x0400, 0x0500));   // Encrypted Attributes Sequence
        target.insert(DicomTag(0x0400, 0x0561));   // Original Attributes Sequence 
        target.insert(DicomTag(0x0040, 0xa390));   // HL7 Structured Document Reference Sequence
        target.insert(DicomTag(0x0028, 0x0303));   // Longitudinal Temporal Information Modified 

        // Table C.12-6 "DIGITAL SIGNATURES MACRO ATTRIBUTES" (p. 1216)
        target.insert(DicomTag(0x4ffe, 0x0001));   // MAC Parameters sequence
        target.insert(DicomTag(0xfffa, 0xfffa));   // Digital signatures sequence
        break;

        // TODO IMAGE MODULE?

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }


#if ORTHANC_BUILDING_FRAMEWORK_LIBRARY == 1
  std::ostream& operator<< (std::ostream& o, const DicomTag& tag)
  {
    tag.FormatStream(o);
    return o;
  }
#endif
}
