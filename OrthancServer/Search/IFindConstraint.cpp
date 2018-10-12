/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2018 Osimis S.A., Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * In addition, as a special exception, the copyright holders of this
 * program give permission to link the code of its release with the
 * OpenSSL project's "OpenSSL" library (or with modified versions of it
 * that use the same license as the "OpenSSL" library), and distribute
 * the linked executables. You must obey the GNU General Public License
 * in all respects for all of the code used other than "OpenSSL". If you
 * modify file(s) with this exception, you may extend this exception to
 * your version of the file(s), but you are not obligated to do so. If
 * you do not wish to do so, delete this exception statement from your
 * version. If you delete this exception statement from all source files
 * in the program, then also delete it here.
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
#include "IFindConstraint.h"

#include "ListConstraint.h"
#include "RangeConstraint.h"
#include "ValueConstraint.h"
#include "WildcardConstraint.h"

#include "../../Core/DicomParsing/FromDcmtkBridge.h"
#include "../../Core/OrthancException.h"

namespace Orthanc
{
  IFindConstraint* IFindConstraint::ParseDicomConstraint(const DicomTag& tag,
                                                         const std::string& dicomQuery,
                                                         bool caseSensitive)
  {
    ValueRepresentation vr = FromDcmtkBridge::LookupValueRepresentation(tag);

    if (vr == ValueRepresentation_Sequence)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

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
      return new RangeConstraint(lower, upper, caseSensitive);
    }
    else if (dicomQuery.find('\\') != std::string::npos)
    {
      std::auto_ptr<ListConstraint> constraint(new ListConstraint(caseSensitive));

      std::vector<std::string> items;
      Toolbox::TokenizeString(items, dicomQuery, '\\');

      for (size_t i = 0; i < items.size(); i++)
      {
        constraint->AddAllowedValue(items[i]);
      }

      return constraint.release();
    }
    else if (dicomQuery.find('*') != std::string::npos ||
             dicomQuery.find('?') != std::string::npos)
    {
      return new WildcardConstraint(dicomQuery, caseSensitive);
    }
    else
    {
      /**
       * Case-insensitive match for PN value representation (Patient
       * Name). Case-senstive match for all the other value
       * representations.
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

      return new ValueConstraint(dicomQuery, caseSensitive);
    }
  }
}
