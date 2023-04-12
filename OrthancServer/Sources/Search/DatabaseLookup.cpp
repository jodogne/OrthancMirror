/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2021-2023 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#include "../PrecompiledHeadersServer.h"
#include "DatabaseLookup.h"

#include "../ServerToolbox.h"
#include "../../../OrthancFramework/Sources/DicomParsing/FromDcmtkBridge.h"
#include "../../../OrthancFramework/Sources/DicomParsing/ToDcmtkBridge.h"
#include "../../../OrthancFramework/Sources/OrthancException.h"
#include "../../../OrthancFramework/Sources/Toolbox.h"

#include <boost/lexical_cast.hpp>


namespace Orthanc
{
  DatabaseLookup::~DatabaseLookup()
  {
    for (size_t i = 0; i < constraints_.size(); i++)
    {
      assert(constraints_[i] != NULL);
      delete constraints_[i];
    }
  }


  const DicomTagConstraint& DatabaseLookup::GetConstraint(size_t index) const
  {
    if (index >= constraints_.size())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
    else
    {
      assert(constraints_[index] != NULL);
      return *constraints_[index];
    }
  }


  void DatabaseLookup::AddConstraintInternal(DicomTagConstraint* constraint)
  {
    if (constraint == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
    else
    {
      constraints_.push_back(constraint);
    }
  }


  bool DatabaseLookup::IsMatch(const DicomMap& value) const
  {
    for (size_t i = 0; i < constraints_.size(); i++)
    {
      assert(constraints_[i] != NULL);
      if (!constraints_[i]->IsMatch(value))
      {
        return false;
      }
    }

    return true;
  }


  bool DatabaseLookup::IsMatch(DcmItem& item,
                               Encoding encoding,
                               bool hasCodeExtensions) const
  {
    for (size_t i = 0; i < constraints_.size(); i++)
    {
      assert(constraints_[i] != NULL);

      const bool isOptionalConstraint = !constraints_[i]->IsMandatory();
      const DcmTagKey tag = ToDcmtkBridge::Convert(constraints_[i]->GetTag());

      DcmElement* element = NULL;
      if (!item.findAndGetElement(tag, element).good())
      {
        return isOptionalConstraint;
      }

      if (element == NULL)
      {
        return false;
      }

      std::set<DicomTag> ignoreTagLength;
      std::unique_ptr<DicomValue> value(FromDcmtkBridge::ConvertLeafElement
                                        (*element, DicomToJsonFlags_None, 
                                         0, encoding, hasCodeExtensions, ignoreTagLength));

      // WARNING: Also modify "HierarchicalMatcher::Setup()" if modifying this code
      if (value.get() == NULL ||
          value->IsNull())
      {
        return isOptionalConstraint;        
      }
      else if (value->IsBinary() ||
               !constraints_[i]->IsMatch(value->GetContent()))
      {
        return false;
      }
    }

    return true;
  }


  void DatabaseLookup::AddDicomConstraintInternal(const DicomTag& tag,
                                                  ValueRepresentation vr,
                                                  const std::string& dicomQuery,
                                                  bool caseSensitive,
                                                  bool mandatoryTag)
  {
    if ((vr == ValueRepresentation_Date ||
         vr == ValueRepresentation_DateTime ||
         vr == ValueRepresentation_Time) &&
        dicomQuery.find('-') != std::string::npos)
    {
      /**
       * Range matching is only defined for TM, DA and DT value
       * representations. This code fixes issues 35 and 37.
       *
       * Reference: "Range matching is not defined for types of
       * Attributes other than dates and times", DICOM PS 3.4,
       * C.2.2.2.5 ("Range Matching").
       **/
      size_t separator = dicomQuery.find('-');
      std::string lower = dicomQuery.substr(0, separator);
      std::string upper = dicomQuery.substr(separator + 1);

      if (!lower.empty())
      {
        AddConstraintInternal(new DicomTagConstraint
                              (tag, ConstraintType_GreaterOrEqual, lower, caseSensitive, mandatoryTag));
      }

      if (!upper.empty())
      {
        AddConstraintInternal(new DicomTagConstraint
                              (tag, ConstraintType_SmallerOrEqual, upper, caseSensitive, mandatoryTag));
      }
    }
    else if (dicomQuery.find('\\') != std::string::npos)
    {
      DicomTag fixedTag(tag);

      std::unique_ptr<DicomTagConstraint> constraint
        (new DicomTagConstraint(fixedTag, ConstraintType_List, caseSensitive, mandatoryTag));

      std::vector<std::string> items;
      Toolbox::TokenizeString(items, dicomQuery, '\\');

      for (size_t i = 0; i < items.size(); i++)
      {
        constraint->AddValue(items[i]);
      }

      AddConstraintInternal(constraint.release());
    }
    else if (
      /**
       * New test in Orthanc 1.6.0: Wild card matching is only allowed
       * for a subset of value representations: AE, CS, LO, LT, PN,
       * SH, ST, UC, UR, UT.
       * http://dicom.nema.org/medical/dicom/2019e/output/chtml/part04/sect_C.2.2.2.4.html
       **/
      (vr == ValueRepresentation_ApplicationEntity ||    // AE
       vr == ValueRepresentation_CodeString ||           // CS
       vr == ValueRepresentation_LongString ||           // LO
       vr == ValueRepresentation_LongText ||             // LT
       vr == ValueRepresentation_PersonName ||           // PN
       vr == ValueRepresentation_ShortString ||          // SH
       vr == ValueRepresentation_ShortText ||            // ST
       vr == ValueRepresentation_UnlimitedCharacters ||  // UC
       vr == ValueRepresentation_UniversalResource ||    // UR
       vr == ValueRepresentation_UnlimitedText           // UT
        ) &&
      (dicomQuery.find('*') != std::string::npos ||
       dicomQuery.find('?') != std::string::npos))
    {
      AddConstraintInternal(new DicomTagConstraint
                            (tag, ConstraintType_Wildcard, dicomQuery, caseSensitive, mandatoryTag));
    }
    else
    {
      AddConstraintInternal(new DicomTagConstraint
                            (tag, ConstraintType_Equal, dicomQuery, caseSensitive, mandatoryTag));
    }
  }


  void DatabaseLookup::AddDicomConstraint(const DicomTag& tag,
                                          const std::string& dicomQuery,
                                          bool caseSensitivePN,
                                          bool mandatoryTag)
  {
    ValueRepresentation vr = FromDcmtkBridge::LookupValueRepresentation(tag);

    if (vr == ValueRepresentation_Sequence)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    /**
     * DICOM specifies that searches must always be case sensitive,
     * except for tags with a PN value representation. For PN, Orthanc
     * uses the configuration option "CaseSensitivePN" to decide
     * whether matching is case-sensitive or case-insensitive.
     *
     * Reference: DICOM PS 3.4
     *   - C.2.2.2.1 ("Single Value Matching") 
     *   - C.2.2.2.4 ("Wild Card Matching")
     * http://medical.nema.org/Dicom/2011/11_04pu.pdf
     *
     * "Except for Attributes with a PN Value Representation, only
     * entities with values which match exactly the value specified in the
     * request shall match. This matching is case-sensitive, i.e.,
     * sensitive to the exact encoding of the key attribute value in
     * character sets where a letter may have multiple encodings (e.g.,
     * based on its case, its position in a word, or whether it is
     * accented)
     * 
     * For Attributes with a PN Value Representation (e.g., Patient Name
     * (0010,0010)), an application may perform literal matching that is
     * either case-sensitive, or that is insensitive to some or all
     * aspects of case, position, accent, or other character encoding
     * variants."
     *
     * (0008,0018) UI SOPInstanceUID     => Case-sensitive
     * (0008,0050) SH AccessionNumber    => Case-sensitive
     * (0010,0020) LO PatientID          => Case-sensitive
     * (0020,000D) UI StudyInstanceUID   => Case-sensitive
     * (0020,000E) UI SeriesInstanceUID  => Case-sensitive
     **/
    
    if (vr == ValueRepresentation_PersonName)
    {
      AddDicomConstraintInternal(tag, vr, dicomQuery, caseSensitivePN, mandatoryTag);
    }
    else
    {
      AddDicomConstraintInternal(tag, vr, dicomQuery, true /* case sensitive */, mandatoryTag);
    }
  }


  void DatabaseLookup::AddRestConstraint(const DicomTag& tag,
                                         const std::string& dicomQuery,
                                         bool caseSensitive,
                                         bool mandatoryTag)
  {
    AddDicomConstraintInternal(tag, FromDcmtkBridge::LookupValueRepresentation(tag),
                               dicomQuery, caseSensitive, mandatoryTag);
  }


  bool DatabaseLookup::HasOnlyMainDicomTags() const
  {
    std::set<DicomTag> allMainTags;
    DicomMap::GetAllMainDicomTags(allMainTags);

    for (size_t i = 0; i < constraints_.size(); i++)
    {
      assert(constraints_[i] != NULL);
      
      if (allMainTags.find(constraints_[i]->GetTag()) == allMainTags.end())
      {
        // This is not a main DICOM tag
        return false;
      }
    }

    return true;
  }


  std::string DatabaseLookup::Format() const
  {
    std::string s;
    
    for (size_t i = 0; i < constraints_.size(); i++)
    {
      assert(constraints_[i] != NULL);
      s += ("Contraint " + boost::lexical_cast<std::string>(i) + ": " +
            constraints_[i]->Format() + "\n");
    }

    return s;
  }

  
  bool DatabaseLookup::HasTag(const DicomTag& tag) const
  {
    for (size_t i = 0; i < constraints_.size(); i++)
    {
      assert(constraints_[i] != NULL);
      if (constraints_[i]->GetTag() == tag)
      {
        return true;
      }
    }

    return false;
  }

  bool DatabaseLookup::GetConstraint(const DicomTagConstraint*& constraint, const DicomTag& tag) const
  {
    for (size_t i = 0; i < constraints_.size(); i++)
    {
      assert(constraints_[i] != NULL);
      if (constraints_[i]->GetTag() == tag)
      {
        constraint = constraints_[i];
        return true;
      }
    }

    return false;
  }


  void DatabaseLookup::RemoveConstraint(const DicomTag& tag)
  {
    for (size_t i = 0; i < constraints_.size(); i++)
    {
      assert(constraints_[i] != NULL);
      if (constraints_[i]->GetTag() == tag)
      {
        delete constraints_[i];
        constraints_.erase(constraints_.begin() + i);
      }
    }
  }

  DatabaseLookup* DatabaseLookup::Clone() const
  {
    std::unique_ptr<DatabaseLookup> clone(new DatabaseLookup());

    for (size_t i = 0; i < constraints_.size(); i++)
    {
      clone->AddConstraintInternal(new DicomTagConstraint(*constraints_[i]));
    }

    return clone.release();
  }


  void DatabaseLookup::AddLabel(const std::string& label)
  {
    if (!label.empty())
    {
      ServerToolbox::CheckValidLabel(label);
      labels_.insert(label);
    }
  }
}
