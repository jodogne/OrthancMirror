/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2015 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
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
#include "HierarchicalMatcher.h"

#include "../../Core/OrthancException.h"
#include "../FromDcmtkBridge.h"

#include <dcmtk/dcmdata/dcfilefo.h>

namespace Orthanc
{
  HierarchicalMatcher::HierarchicalMatcher(ParsedDicomFile& query,
                                           bool caseSensitivePN)
  {
    Setup(*query.GetDcmtkObject().getDataset(), 
          caseSensitivePN,
          query.GetEncoding());
  }


  HierarchicalMatcher::~HierarchicalMatcher()
  {
    for (Constraints::iterator it = constraints_.begin();
         it != constraints_.end(); ++it)
    {
      if (it->second != NULL)
      {
        delete it->second;
      }
    }

    for (Sequences::iterator it = sequences_.begin();
         it != sequences_.end(); ++it)
    {
      if (it->second != NULL)
      {
        delete it->second;
      }
    }
  }


  void HierarchicalMatcher::Setup(DcmItem& dataset,
                                  bool caseSensitivePN,
                                  Encoding encoding)
  {
    for (unsigned long i = 0; i < dataset.card(); i++)
    {
      DcmElement* element = dataset.getElement(i);
      if (element == NULL)
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      DicomTag tag(FromDcmtkBridge::Convert(element->getTag()));
      ValueRepresentation vr = FromDcmtkBridge::GetValueRepresentation(tag);

      if (constraints_.find(tag) != constraints_.end() ||
          sequences_.find(tag) != sequences_.end())
      {
        throw OrthancException(ErrorCode_BadRequest);        
      }

      if (vr == ValueRepresentation_Sequence)
      {
        DcmSequenceOfItems& sequence = dynamic_cast<DcmSequenceOfItems&>(*element);

        if (sequence.card() == 0 ||
            (sequence.card() == 1 && sequence.getItem(0)->card() == 0))
        {
          // Universal matching of a sequence
          sequences_[tag] = NULL;
        }
        else if (sequence.card() == 1)
        {
          sequences_[tag] = new HierarchicalMatcher(*sequence.getItem(0), caseSensitivePN, encoding);
        }
        else
        {
          throw OrthancException(ErrorCode_BadRequest);        
        }
      }
      else
      {
        std::auto_ptr<DicomValue> value(FromDcmtkBridge::ConvertLeafElement
                                        (*element, DicomToJsonFlags_None, encoding));

        if (value->IsBinary() ||
            value->IsNull())
        {
          throw OrthancException(ErrorCode_BadRequest);
        }
        else if (value->GetContent().empty())
        {
          // This is an universal matcher
          constraints_[tag] = NULL;
        }
        else
        {
          // DICOM specifies that searches must be case sensitive, except
          // for tags with a PN value representation
          bool sensitive = true;
          if (vr == ValueRepresentation_PatientName)
          {
            sensitive = caseSensitivePN;
          }

          constraints_[tag] = IFindConstraint::ParseDicomConstraint(tag, value->GetContent(), sensitive);
        }
      }
    }
  }


  std::string HierarchicalMatcher::Format(const std::string& prefix) const
  {
    std::string s;
    
    for (Constraints::const_iterator it = constraints_.begin();
         it != constraints_.end(); ++it)
    {
      s += prefix + it->first.Format() + " ";

      if (it->second == NULL)
      {
        s += "*\n";
      }
      else
      {
        s += it->second->Format() + "\n";
      }
    }

    for (Sequences::const_iterator it = sequences_.begin();
         it != sequences_.end(); ++it)
    {
      s += prefix + it->first.Format() + " ";

      if (it->second == NULL)
      {
        s += "*\n";
      }
      else
      {
        s += "Sequence:\n" + it->second->Format(prefix + "  ");
      }
    }

    return s;
  }
}
